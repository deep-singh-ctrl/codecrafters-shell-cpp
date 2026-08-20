#include <iostream>
#include <string>

void echo(){
  std::string line;
  std::getline(std::cin, line);
  std::cout << line << std::endl;
}

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
    if(command == "echo") {
      echo();
    }
    std::cout << command << ": command not found" << std::endl;
  }
}
