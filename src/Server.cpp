// Server.cpp
#include "Server.hpp"
#include "../lib/cpp-httplib/httplib.h"
#include <iostream>

/**
 * @brief Конструктор.
 * @param port порт для прослушивания.
 * @param inspector Экземпляр Inspector для сканирования файлов.
 */
Server::Server(uint16_t port, std::unique_ptr<Inspector> inspector)
    : port_(port), inspector_(std::move(inspector)) {

  // Запускаем фоновый поток инспектора
  if (inspector_) {
    inspector_->run();
  }
}

/**
 * @brief Сигнализирует серверу об остановке после завершения текущих
 * операций.
 */
Server::~Server() { stop(); }

/**
 * @brief Запускает HTTP-сервер и блокируется до вызова stop().
 */
void Server::run() {
  httplib::Server svr;

  // Настройка эндпоинта
  svr.Get("/media_files",
          [this](const httplib::Request &, httplib::Response &res) {
            res.set_content(handleMediaFiles(), "application/json");
          });

  // Обработчик ошибки 404
  svr.set_error_handler([](const httplib::Request &, httplib::Response &res) {
    if (res.status == 404) {
      res.set_content(R"({"error": "Not found"})", "application/json");
    }
  });

  std::cout << "HTTP-сервер слушает порт " << port_ << std::endl;

  serverThread_ = std::make_unique<std::thread>([this, &svr]() {
    if (!svr.listen("localhost", port_)) {
      std::cerr << "Не удалось запустить HTTP-сервер на порту " << port_
                << std::endl;
    }
  });

  // Ожидаем сигнала остановки
  while (running_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  svr.stop();
  if (serverThread_ && serverThread_->joinable()) {
    serverThread_->join();
  }
}

/**
 * @brief Сигнализирует серверу об остановке после завершения текущих
 * операций.
 */
void Server::stop() { running_ = false; }

/**
 * @brief Обработчик HTTP-запроса GET /media_files.
 * @return JSON-строка со списками аудио, видео и изображений.
 */
std::string Server::handleMediaFiles() const {
  if (!inspector_) {
    return R"({"error": "Inspector not available"})";
  }
  return inspector_->getResponse();
}
