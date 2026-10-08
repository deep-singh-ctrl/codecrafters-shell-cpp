#include <iostream>
#include <string>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <fcntl.h>
#include <sstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <cstdlib>
#include <iterator>


class job {
public:
    int pid;
    int job_id;
    std::vector<std::string> command;
    bool running;

    job(int p, int i, std::vector<std::string> &c){
        command = c;
        pid = p;
        job_id = i;
        running = true;
    }
};

std::vector<job> backgroundJobs; 
char** getArgvPointer(std::vector<std::string> &userInput);
int handleBuiltin(std::vector<std::string>& userInput);

void executeExternalCommand(std::vector<std::string>& userInput) {
    char** argvPointer = getArgvPointer(userInput);

    const char* env_p = std::getenv("PATH");
    //WARNING : No error handling in case PATH does not exist
    std::string env_val(env_p);
    std::stringstream ss(env_val);
    std::string token;

    char delimiter = ':';
    std::vector<std::string> results;
    
    while (std::getline(ss, token, delimiter)) {
        results.push_back(token);
    }

    namespace fs = std::filesystem;
    std::string executableName = (userInput[0]);
    for (std::string& s : results) {
        fs::path filePath = s + "/" + executableName;
        if (std::filesystem::exists(filePath)) {
            
            if ((fs::status(filePath).permissions() & fs::perms::owner_exec) != fs::perms::none ||
                (fs::status(filePath).permissions() & fs::perms::others_exec) != fs::perms::none ||
                (fs::status(filePath).permissions() & fs::perms::group_exec) != fs::perms::none) {
                execv(filePath.string().data(), argvPointer);
                exit(1);
            }
        }
    }
    std::cerr << executableName << ": command not found" << std::endl;
    exit(1);
}

enum parser_state {
    NORMAL,
    SINGLE_QUOTE_MODE,
    DOUBLE_QUOTE_MODE,
};

void parseUserInput(std::vector<std::string> &userInput, 
    const std::string &command){
    std::string token = "";
    int current_state = NORMAL;
    for(int i = 0; i < command.size(); i++){
        char ch = command[i];
        switch (current_state) {
        case NORMAL:
            if(ch == '\''){
                current_state = SINGLE_QUOTE_MODE;
            }
            else if(ch == '\"'){
                current_state = DOUBLE_QUOTE_MODE;
            }
            else if(ch == ' '){
                if(token.size() > 0){
                    userInput.push_back(token);
                    token = "";
                }
            }
            else if(ch == '\\'){
                // here assuming that there IS a character after the escape else the
                // shell just waits for an extra character.
                token += command[i+1];
                i++;
            }
            else{
                token += ch;
            }
            break;
        case SINGLE_QUOTE_MODE:
            if(ch == '\''){
                current_state = NORMAL;
            }
            else{
                token += ch;
            }
            break;
        case DOUBLE_QUOTE_MODE:
            if(ch == '\"'){
                current_state = NORMAL;
            }
            else if(ch == '\\'){
                if(i+1 < command.size() && command[i+1] == '\"' || 
                command[i+1] == '\\' || command[i+1] == '$' || 
                command[i+1] == '`'){
                    token += command[i+1];
                }
                else{
                    token += command[i];
                    token += command[i+1];
                }
                i++;
            }
            else{
                token += ch;
            }
            break;
        }
    }
    if(token.size() > 0) userInput.push_back(token);
}

void runBackgroundJob(std::vector<std::string> &userInput){
    // running an executable in the background
    // trimming the trailing & from the userInput
    userInput.pop_back();
    const char* env_p = std::getenv("PATH");
    //WARNING : No error handling in case PATH does not exist
    std::string env_val(env_p);
    std::stringstream ss(env_val);
    std::string token;
    std::vector<char* > argv;
    for(std::string &s : userInput){
      argv.push_back(s.data());
    }
    // execv expects a NULL at the end of the argument list so we append one to argv.
    argv.push_back(NULL);
    char** argvPointer = argv.data();
    // Use this argvPointer inside exec when you fork for a new process 
    // For LINUX the delimiter for PATH directories is a colon
    char delimiter = ':';
    std::vector<std::string> results;
    
    while (std::getline(ss, token, delimiter)) {
        results.push_back(token);
    }

    bool foundExecutable = false;
    namespace fs = std::filesystem;
    std::string executableName = (userInput[0]);
    for (std::string& s : results) {
        fs::path filePath = s + "/" + executableName;
        if (std::filesystem::exists(filePath)) {
            
            if ((fs::status(filePath).permissions() & fs::perms::owner_exec) != fs::perms::none ||
                (fs::status(filePath).permissions() & fs::perms::others_exec) != fs::perms::none ||
                (fs::status(filePath).permissions() & fs::perms::group_exec) != fs::perms::none) {
                 foundExecutable = true;
                 pid_t child = fork();
                 // assuming that the child can ALWAYS be created.
                 if(child == 0){
                    execv(filePath.string().data(), argvPointer);
                  }
                 else if(child > 0){
                    int jd = 1;
                    while(true){
                        for(int i = 0; i < backgroundJobs.size(); i++){
                            if(backgroundJobs[i].job_id == jd){
                                jd++;
                                i = 0;
                                continue;
                            }
                        }
                        break;
                    }
                    std::cout << "[" << jd << "]" << " " << child << std::endl;
                    job j(child, jd, userInput);
                    backgroundJobs.push_back(j);
                 }
                 break;
            }
        }
    }
    if(!foundExecutable){
        std::cerr << executableName << ": command not found" << std::endl;
    }
}

char** getArgvPointer(std::vector<std::string> &userInput){
    std::vector<char* > argv;
    for(std::string &s : userInput){
      argv.push_back(s.data());
    }
    // execv expects a NULL at the end of the argument list so we append one to argv.
    argv.push_back(NULL);
    char** argvPointer = argv.data();
    return argvPointer;
}

void runExecutableFilePath(std::vector<std::string> &userInput){
    
    char** argvPointer = getArgvPointer(userInput);
    // Use this argvPointer inside exec when you fork for a new process 
    // For LINUX the delimiter for PATH directories is a colon

    const char* env_p = std::getenv("PATH");
    //WARNING : No error handling in case PATH does not exist
    std::string env_val(env_p);
    std::stringstream ss(env_val);
    std::string token;

    char delimiter = ':';
    std::vector<std::string> results;
    
    while (std::getline(ss, token, delimiter)) {
        results.push_back(token);
    }

    bool foundExecutable = false;
    namespace fs = std::filesystem;
    std::string executableName = (userInput[0]);
    for (std::string& s : results) {
        fs::path filePath = s + "/" + executableName;
        if (std::filesystem::exists(filePath)) {
            
            if ((fs::status(filePath).permissions() & fs::perms::owner_exec) != fs::perms::none ||
                (fs::status(filePath).permissions() & fs::perms::others_exec) != fs::perms::none ||
                (fs::status(filePath).permissions() & fs::perms::group_exec) != fs::perms::none) {
                 foundExecutable = true;
                 pid_t child = fork();
                 // assuming that the child can ALWAYS be created.
                 if(child == 0){
                    execv(filePath.string().data(), argvPointer);
                  }
                 else if(child > 0){
                  waitpid(child, NULL, 0);
                 }
                 break;
            }
        }
    }
    if(!foundExecutable){
        std::cerr << executableName << ": command not found" << std::endl;
    }
}

// Returns true if the shell should exit
bool executeSingleCommand(std::vector<std::string>& userInput) {
    int status = handleBuiltin(userInput);
    
    if (status == 2) {
        return true; // User typed 'exit', break the main loop
    }
    if (status == 1) {
        return false; // Built-in ran successfully, do not exit
    }
    
    // status == 0 means it's an external command. 
    // runExecutableFilePath will handle the fork() and waitpid().
    runExecutableFilePath(userInput);
    return false; 
}

void executePipeline(std::vector<std::vector<std::string>> &pipeline){
    int fd[2];
    pipe(fd);
    
    pid_t child_write = fork();
    if(child_write == 0){
        dup2(fd[1] , STDOUT_FILENO);
        close(fd[0]);  
        
        // Traffic cop for the left side of the pipe
        if (handleBuiltin(pipeline[0]) == 0) {
            executeExternalCommand(pipeline[0]);
        }
        exit(0); // Safely kill the child when done
    }
    
    pid_t child_read = fork();
    if(child_read == 0){
        dup2(fd[0] , STDIN_FILENO);
        close(fd[1]);  
        
        // Traffic cop for the right side of the pipe
        if (handleBuiltin(pipeline[1]) == 0) {
            executeExternalCommand(pipeline[1]);
        }
        exit(0);
    }
    
    close(fd[0]);
    close(fd[1]);
    waitpid(child_write, NULL, 0);  
    waitpid(child_read, NULL, 0);
}


// Returns:
// 0 = Not a built-in (needs external execution)
// 1 = Handled as a built-in successfully
// 2 = The user typed 'exit', shell should terminate
int handleBuiltin(std::vector<std::string>& userInput) {
    if (userInput.empty()) return 1; 

    std::string cmd = userInput[0];

    if (cmd == "exit") {
        return 2; 
    } 
    else if (cmd == "cd"){
        std::filesystem::path home_directory = std::getenv("HOME");
        if(userInput[1].substr(0,1) == "~"){
            userInput[1] = home_directory.string() + "/" + userInput[1].substr(1);
        }
        std::filesystem::path new_directory = userInput[1];
        if(std::filesystem::exists(new_directory)){
            std::filesystem::current_path(new_directory);
        }
        else{
            std::cerr << "cd: " << new_directory.string() <<": No such file or directory" << std::endl; 
        }
        return 1;
    }
    else if(cmd == "pwd"){
        //I am not dealing with errors here, if error is thrown that's an afterthought
        std::filesystem::path cwd = std::filesystem::current_path();
        std::cout << cwd.string() << std::endl;
        return 1;
    }
    else if(cmd == "jobs"){
        // mark the jobs which are done as finished. 
        for(int i = 0; i < backgroundJobs.size(); i++){
            if(waitpid(backgroundJobs[i].pid, NULL, WNOHANG) != 0){
                backgroundJobs[i].running = false;
            }
        }

        for(int i = 0; i < backgroundJobs.size(); i++){
            std::string status = (backgroundJobs[i].running ? "Running " : "Done ");
            if(i == backgroundJobs.size()-2){
                std::cout << "[" << backgroundJobs[i].job_id << "]- " << status;    
            }
            else if(i == backgroundJobs.size()-1){
                std::cout << "[" << backgroundJobs[i].job_id << "]+ " << status;    
            }
            else{
                std::cout << "[" << backgroundJobs[i].job_id << "]  " << status;
            }
            for(std::string &x: backgroundJobs[i].command){
                std::cout << x << " ";
            }
            std::cout << std::endl;
        }
        
        // reaping zombie process. 
        for(int i = 0; i < backgroundJobs.size(); i++){
            if(backgroundJobs[i].running == false){
                backgroundJobs.erase(backgroundJobs.begin() + i);
                i--;
            }
        }
        return 1;
    }
    else if (cmd == "echo") {
        for(int i = 1; i < userInput.size() - 1; i++){
            std::cout << userInput[i] << " ";
        }
        std::cout << userInput.back() << std::endl;
        return 1;
    } 
    else if (cmd == "type") {
        std::string argument = userInput[1];

        if (argument == "echo" || argument == "type" || argument == "exit" || argument == "pwd" || argument == "jobs") {
            std::cout << argument << " is a shell builtin" << std::endl;
        } else {
            const char* env_p = std::getenv("PATH");
            // need to add if a file is in some directory here
            if (env_p == nullptr) {
                std::cerr << "Environment variable not found." << std::endl;
                return 1;
            }

            std::string env_val(env_p);
            std::stringstream ss(env_val);
            std::string token;
            std::vector<char* > argv;
            for(std::string &s : userInput){
            argv.push_back(s.data());
            }
            // execv expects a NULL at the end of the argument list so we append one to argv.
            argv.push_back(NULL);
            char** argvPointer = argv.data();
            // Use this argvPointer inside exec when you fork for a new process 
            // For LINUX the delimiter for PATH directories is a colon
            char delimiter = ':';
            std::vector<std::string> results;
            while (std::getline(ss, token, delimiter)) {
                results.push_back(token);
            }
            bool foundExecutable = false;
            namespace fs = std::filesystem;
            std::string executableName = (userInput[1]);
            for (std::string& s : results) {
                fs::path filePath = s + "/" + executableName;
                if (std::filesystem::exists(filePath)) {
                    
                    if ((fs::status(filePath).permissions() & fs::perms::owner_exec) != fs::perms::none ||
                        (fs::status(filePath).permissions() & fs::perms::others_exec) != fs::perms::none ||
                        (fs::status(filePath).permissions() & fs::perms::group_exec) != fs::perms::none) {
                        foundExecutable = true;
                        std::cout << argument << " is " << filePath.string() << std::endl;
                        break;
                    }
                }
            }
            if(!foundExecutable){
                std::cout << argument << ": not found" << std::endl;
            }
        }
        return 1;
    }

    // If we make it all the way down here, it wasn't a built-in!
    return 0; 
}

int main() {
    // Flush after every std::cout / std::cerr
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
    // TODO: Uncomment the code below to pass the first stage
    while (true) {
        //reaping jobs at the start of any command. 
        for(int i = 0; i < backgroundJobs.size(); i++){
            if(waitpid(backgroundJobs[i].pid, NULL, WNOHANG) != 0){
                backgroundJobs[i].running = false;
            }
        }
        // display done
        for(int i = 0; i < backgroundJobs.size(); i++){
            if(backgroundJobs[i].running == false){
                if(i == backgroundJobs.size() - 1){
                    std::cout << "[" << backgroundJobs[i].job_id << "]+ " << "Done " ;
                }
                else if(i == backgroundJobs.size() - 2){
                    std::cout << "[" << backgroundJobs[i].job_id << "]- " << "Done " ;
                }
                else{
                    std::cout << "[" << backgroundJobs[i].job_id << "] " << "Done " ;
                }
                for(std::string &x: backgroundJobs[i].command){
                    std::cout << x << " ";
                }
                std::cout << std::endl;
            }
        }
        // reaping zombie process. 
        for(int i = 0; i < backgroundJobs.size(); i++){
            if(backgroundJobs[i].running == false){
                backgroundJobs.erase(backgroundJobs.begin() + i);
                i--;
            }
        }
        std::cout << "$ ";
        std::string command;
        std::getline(std::cin, command);
        std::vector<std::string> userInput;
        if (std::cin.eof()) {
            break;
        }

        // this helps us take care of single quotes, double quotes etc inside
        // the command. gives us a clean token vector to deal with 
        parseUserInput(userInput, command);
        // begining logic to implement pipelines. 
        if(find(userInput.begin(), userInput.end(), "|") != userInput.end()){
            std::vector<std::vector<std::string>> pipeline;
            std::vector<std::string> currentCommand;
            for(std::string &str : userInput){
                if(str == "|"){
                    pipeline.push_back(currentCommand);
                    while(!currentCommand.size() == 0){
                        currentCommand.pop_back();
                    }
                }
                else{
                    currentCommand.push_back(str);
                }
            }
            pipeline.push_back(currentCommand); // pushing in the last command. 
            executePipeline(pipeline);
            continue;
        }

        if(userInput.size() == 0){
            continue;
        }
        // i Do assume that a file name is provided for redirection

        int orig_stdout = dup(STDOUT_FILENO);
        int orig_stderr = dup(STDERR_FILENO);
        bool redirected = false;
        
        for(auto it = userInput.begin(); it != userInput.end(); ){
            if(*it == ">" || *it == "1>"){
                redirected = true;
                int fd = open((*(it + 1)).c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                dup2(fd, STDOUT_FILENO);
                close(fd);
                userInput.erase(it, (it+2));
                break;
            }
            else if(*it == ">>" || *it == "1>>"){
                redirected = true;
                int fd = open((*(it + 1)).c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
                dup2(fd, STDOUT_FILENO);
                close(fd);
                userInput.erase(it, (it+2));
                break;
            }
            else if(*it == "2>"){
                redirected = true;
                int fd = open((*(it + 1)).c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                dup2(fd, STDERR_FILENO);
                close(fd);
                userInput.erase(it, (it+2));
                break;
            }
            else if(*it == "2>>"){
                redirected = true;
                int fd = open((*(it + 1)).c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
                dup2(fd, STDERR_FILENO);
                close(fd);
                userInput.erase(it, (it+2));
                break;
            }
            else {
                it++;
            }
        }
        

        if (userInput.back() == "&") {
            runBackgroundJob(userInput);
        }
        else {
            if (executeSingleCommand(userInput)) {
                break; // Breaks the while(true) loop to exit the shell
            }
        }

        if (redirected) {
            std::cout.flush();
            std::cerr.flush();
            dup2(orig_stdout, STDOUT_FILENO);
            dup2(orig_stderr, STDERR_FILENO);
        }
        
    }
    return 0;
}