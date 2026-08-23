#include <iostream>
#include <string>
#include <filesystem>
#include <vector>
#include <sstream>

int main() {
    // Flush after every std::cout / std::cerr
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    // TODO: Uncomment the code below to pass the first stage
    while (true) {
        std::cout << "$ ";
        std::string command;
        std::getline(std::cin, command);

        if (command == "exit") {
            break;
        } else if (command.substr(0, 4) == "echo") {
            std::cout << command.substr(5) << std::endl;
        } else if (command.substr(0, 4) == "type") {
            std::string argument = command.substr(5);

            if (argument == "echo" || argument == "type" || argument == "exit") {
                std::cout << argument << " is a shell builtin" << std::endl;
            } else {
                const char* env_p = std::getenv("PATH");

                if (env_p == nullptr) {
                    std::cout << "Environment variable not found." << std::endl;
                    return 1;
                }

                std::string env_val(env_p);
                std::stringstream ss(env_val);
                std::string token;

                // For LINUX the delimiter for PATH is a colon
                char delimiter = ':';
                std::vector<std::string> results;

                while (std::getline(ss, token, delimiter)) {
                    results.push_back(token);
                }

                bool valid = false;
                namespace fs = std::filesystem;
                for (std::string& s : results) {
                    fs::path filePath = s + "/" + command.substr(5);
                    if (std::filesystem::exists(filePath)) {
                        
                        if ((fs::status(filePath).permissions() & fs::perms::owner_exec) != fs::perms::none ||
                            (fs::status(filePath).permissions() & fs::perms::others_exec) != fs::perms::none ||
                            (fs::status(filePath).permissions() & fs::perms::group_exec) != fs::perms::none) {
                            std::cout << command.substr(5) << " is " << filePath.string() << std::endl;
                            valid = true;
                            break;
                        }
                    }
                }

                if (!valid) {
                    std::cout << command.substr(5) << ": command not found" << std::endl;
                }
            }

        } else {
            std::cout << command << ": command not found" << std::endl;
        }
    }
    return 0;
}