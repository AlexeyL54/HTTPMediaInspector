#pragma once

#include "Inspector.hpp"
#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>

/**
 * @brief HTTP-сервер для отдачи информации о мультимедийных файлах.
 *
 * Использует cpp-httplib для предоставления REST-эндпоинта /media_files,
 * который возвращает JSON со списками аудио, видео и изображений.
 */
class Server {
public:
  /**
   * @brief Конструктор.
   * @param port порт для прослушивания.
   * @param inspector Экземпляр Inspector для сканирования файлов.
   */
  Server(uint16_t port, std::unique_ptr<Inspector> inspector);

  /**
   * @brief Деструктор. Останавливает все потоки.
   */
  ~Server();

  /**
   * @brief Запускает HTTP-сервер и блокируется до вызова stop().
   */
  void run();

  /**
   * @brief Сигнализирует серверу об остановке после завершения текущих
   * операций.
   */
  void stop();

private:
  /**
   * @brief Обработчик HTTP-запроса GET /media_files.
   * @return JSON-строка со списками аудио, видео и изображений.
   */
  std::string handleMediaFiles() const;

  std::atomic<bool> running_{true};           ///< Флаг работы сервера
  uint16_t port_;                             ///< Порт для прослушивания
  std::unique_ptr<Inspector> inspector_;      ///< Инспектор для сканирования
  std::unique_ptr<std::thread> serverThread_; ///< Поток HTTP-сервера
};
