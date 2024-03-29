#include <iostream>
#include <string>

int main() {
    std::string userInput;

    while (true) {
        // 提示用户输入命令
        std::cout << "Enter a command: ";
        std::getline(std::cin, userInput);



        // 根据用户输入做出相应的回复
        if (userInput == "hello") {
            std::cout << "Hi there!" << std::endl;
        } else if (userInput == "bye") {
            std::cout << "Goodbye!" << std::endl;
            break; // 退出循环
        } else {
            std::cout << "Unknown command. Please try again." << std::endl;
        }
    }

    return 0;
}
