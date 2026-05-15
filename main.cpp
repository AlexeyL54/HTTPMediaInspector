// main.cpp
#include "src/Inspector.hpp"
#include "src/Server.hpp"

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>

/**
 * @brief Выводит справку по использованию программы.
 * @param argv0 Имя программы (argv[0]).
 */
static void printUsage(const char *argv0) {
  std::cout << "Использование: " << argv0
            << " [-t интервал_сек] [-d директория] [-p порт]\n"
            << "  -t интервал_сек : Интервал сканирования в секундах (по "
               "умолчанию: 5)\n"
            << "  -d директория   : Директория для сканирования (по умолчанию "
               "домашняя)\n"
            << "  -p порт         : Порт HTTP-сервера (по умолчанию: 1234)\n"
            << "  -h              : Показать эту справку\n"
            << std::endl;
}

/**
 * @brief Выводит информацию о запуске сервера.
 * @param port порт.
 * @param interval интервал сканирования файловой системы
 * @param dir путь
 */
static void printInfo(uint16_t port, uint32_t interval, const std::string dir) {
  std::cout << "Запуск сервера на http://localhost:" << port << "/media_files"
            << std::endl;
  std::cout << "Нажмите Ctrl+C для остановки" << std::endl;
  std::cout << "Сканирование директории: " << dir << " каждые " << interval
            << " секунд" << std::endl;
}

int main(int argc, char *argv[]) {
  uint16_t port = 1234;
  uint32_t interval = 5;
  std::string dir = getenv("HOME");

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "-t" && i + 1 < argc) {
      interval = static_cast<uint32_t>(std::stoi(argv[++i]));
    } else if (arg == "-d" && i + 1 < argc) {
      dir = argv[++i];
    } else if (arg == "-p" && i + 1 < argc) {
      port = static_cast<uint16_t>(std::stoi(argv[++i]));
    } else if (arg == "-h") {
      printUsage(argv[0]);
      return 0;
    } else {
      std::cerr << "Неизвестная опция: " << arg << std::endl;
      printUsage(argv[0]);
      return 1;
    }
  }

  std::unique_ptr<Inspector> inspector =
      std::make_unique<Inspector>(dir, interval);

  printInfo(port, interval, dir);

  try {
    Server server(port, std::move(inspector));
    server.run();
  } catch (const std::exception &e) {
    std::cerr << "Ошибка сервера: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
