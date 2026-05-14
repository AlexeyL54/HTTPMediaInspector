// Server.cpp
#include "Server.hpp"
#include "../lib/cpp-httplib/httplib.h"
#include <iostream>

Server::Server(uint16_t port, std::unique_ptr<Inspector> inspector)
    : port_(port), inspector_(std::move(inspector)) {

  // Запускаем фоновый поток инспектора
  if (inspector_) {
    inspector_->run();
  }
}

Server::~Server() { stop(); }

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

void Server::stop() { running_ = false; }

std::string Server::handleMediaFiles() const {
  if (!inspector_) {
    return R"({"error": "Inspector not available"})";
  }
  return inspector_->getResponse();
}
