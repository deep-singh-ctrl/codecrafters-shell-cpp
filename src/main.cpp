#include <iostream>
#include <string>



int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // TODO: Uncomment the code below to pass the first stage
  while(true){
    std::cout << "$ ";
    std::string command;
    std::getline(std::cin, command);
    if(command == "exit") break;
    if(command.substr(0,4) == "echo") {
      std::cout << command.substr(5) << std::endl;
    }
    if(command.substr(0,4) == "type") {
      std::string argument = command.substr(5);
      if(argument == "echo" || argument == "type" || argument == "exit"){
        std::cout << argument << " is a shell builtin" << std::endl;
      }
      else{
        std::cout << argument << ": command not found" << std::endl;  
      }
    }
    else{
      std::cout << command << ": command not found" << std::endl;
    }
  }
}

