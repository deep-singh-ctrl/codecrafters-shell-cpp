#include <iostream>
#include <string>

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // TODO: Uncomment the code below to pass the first stage
  void echo(){
    std::string line;
    std::getline(std::cin, line);
    cout << line << endl;
  }
  while(true){
    std::cout << "$ ";
    std::string command;
    std::getline(std::cin, command);
    if(command == "exit") break;
    if(command == echo) {
      echo();
    }
    std::cout << command << ": command not found" << std::endl;
  }
}
