// src/controller.cpp
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include "../include/Communication.hpp"

int main() {
  std::cout << "\n[CONTROLLER] PID: " << getpid() << std::endl;

  // Abre o FIFO criado pelo gerador para enviar comandos
  int commandFd = open(FIFO_COMMAND, O_WRONLY);
  if (commandFd < 0) {
    std::cerr << "[CONTROLLER] Error: generator not running?" << std::endl;
    return 1;
  }

  // Mapa que associa nomes de comandos a funções que os executam.
  // A função recebe o valor (string) e o descritor do FIFO.
  std::map<std::string, std::function<void(const std::string&, int)>> handlers;

  handlers["start"] = [&](const std::string&, int fd) {
    Command cmd(CMD_START, 0.0);
    write(fd, &cmd, sizeof(Command));
    std::cout << "START sent" << std::endl;
  };

  handlers["stop"] = [&](const std::string&, int fd) {
    Command cmd(CMD_STOP, 0.0);
    write(fd, &cmd, sizeof(Command));
    std::cout << "STOP sent" << std::endl;
  };

  handlers["freq"] = [&](const std::string& value, int fd) {
    try {
      double freq = std::stod(value);
      Command cmd(CMD_SET_FREQ, freq);
      write(fd, &cmd, sizeof(Command));
      std::cout << "FREQ=" << freq << " Hz sent" << std::endl;
    } catch (...) {
      std::cout << "Error: invalid frequency value" << std::endl;
    }
  };

  handlers["amp"] = [&](const std::string& value, int fd) {
    try {
      double amp = std::stod(value);
      Command cmd(CMD_SET_AMP, amp);
      write(fd, &cmd, sizeof(Command));
      std::cout << "AMP=" << amp << " sent" << std::endl;
    } catch (...) {
      std::cout << "Error: invalid amplitude value" << std::endl;
    }
  };

  handlers["quit"] = [&](const std::string&, int fd) {
    Command cmd(CMD_QUIT, 0.0);
    write(fd, &cmd, sizeof(Command));
    std::cout << "QUIT sent" << std::endl;
    close(fd);
    exit(0);
  };

  // Exibe menu de ajuda
  std::cout << "\n=== CONTROLLER ===" << std::endl;
  for (const auto& [cmd, _] : handlers) {
    std::cout << "  " << cmd << std::endl;
  }
  std::cout << "==================\n" << std::endl;

  std::string line;
  while (true) {
    std::cout << "> ";
    std::getline(std::cin, line);

    if (line.empty()) continue;

    std::istringstream iss(line);
    std::string cmdStr;
    std::string value;

    iss >> cmdStr;
    std::getline(iss >> std::ws,
                 value);  // Pega o resto da linha como argumento

    auto it = handlers.find(cmdStr);
    if (it != handlers.end()) {
      it->second(value, commandFd);
    } else {
      std::cout << "Unknown command: " << cmdStr << std::endl;
    }
  }

  close(commandFd);
  return 0;
}
