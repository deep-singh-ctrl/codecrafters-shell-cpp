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
// Declaring some global parameters here
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

void runExecutableFilePath(std::vector<std::string> &userInput){
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

int main() {
    // Flush after every std::cout / std::cerr
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
    // TODO: Uncomment the code below to pass the first stage
    while (true) {
        for(int i = 0; i < backgroundJobs.size(); i++){
            if(waitpid(backgroundJobs[i].pid, NULL, WNOHANG) != 0){
                backgroundJobs[i].running = false;
            }
        }
        for(int i = 0; i < backgroundJobs.size(); i++){
            if(backgroundJobs[i].running == false){
                if(i == backgroundJobs.size() - 1){
                    std::cout << "[" << backgroundJobs[i].job_id << "]+ " << "Done " << std::endl; 
                }
                else if(i == backgroundJobs.size() - 2){
                    std::cout << "[" << backgroundJobs[i].job_id << "]- " << "Done " << std::endl;
                }
                else{
                    std::cout << "[" << backgroundJobs[i].job_id << "] " << "Done " << std::endl;
                }
                for(std::string &x: backgroundJobs[i].command){
                    std::cout << x << " ";
                }
            }
        }
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
        // the command.
        parseUserInput(userInput, command);
        // i Do assume that a file name is provided for redirection
        if(userInput.size() == 0){
            continue;
        }
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
        if (userInput[0] == "cd"){
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
        }
        else if(command == "pwd"){
            //I am not dealing with errors here, if error is thrown that's an afterthought
            std::filesystem::path cwd = std::filesystem::current_path();
            std::cout << cwd.string() << std::endl;
        }
        else if(command == "jobs"){
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
        }
        else if (command == "exit") {
            break;
        } else if (command.substr(0, 4) == "echo") {
            for(int i = 1; i < userInput.size() - 1; i++){
                std::cout << userInput[i] << " ";
            }
            std::cout << userInput.back() << std::endl;
        } else if (command.substr(0, 4) == "type") {
            std::string argument = command.substr(5);

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
            
        } else if(userInput.back() == "&"){
            runBackgroundJob(userInput);
        }
        else {
            runExecutableFilePath(userInput);
        }

        if(redirected){
            std::cout.flush();
            std::cerr.flush();
            dup2(orig_stdout, STDOUT_FILENO);
            dup2(orig_stderr, STDERR_FILENO);
        }
        close(orig_stdout);
        close(orig_stderr);
    }
    return 0;
}