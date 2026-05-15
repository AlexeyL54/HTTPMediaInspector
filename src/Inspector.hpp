#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

/**
 * @brief Категории медиафайлов
 */
enum class MediaCategory {
  Audio,  ///< Аудиофайлы
  Video,  ///< Видеофайлы
  Images, ///< Изображения
  Unknown ///< Неизвестный тип
};

/**
 * @brief Маппинг расширений файлов на категории медиа
 */
static const std::unordered_map<std::string_view, MediaCategory>
    extensionMappings = {
        {"mp3", MediaCategory::Audio},   {"wav", MediaCategory::Audio},
        {"flac", MediaCategory::Audio},  {"ogg", MediaCategory::Audio},
        {"m4a", MediaCategory::Audio},   {"aac", MediaCategory::Audio},
        {"wma", MediaCategory::Audio},   {"mp4", MediaCategory::Video},
        {"avi", MediaCategory::Video},   {"mkv", MediaCategory::Video},
        {"mov", MediaCategory::Video},   {"wmv", MediaCategory::Video},
        {"flv", MediaCategory::Video},   {"webm", MediaCategory::Video},
        {"mpeg", MediaCategory::Video},  {"mpg", MediaCategory::Video},
        {"jpg", MediaCategory::Images},  {"jpeg", MediaCategory::Images},
        {"png", MediaCategory::Images},  {"gif", MediaCategory::Images},
        {"bmp", MediaCategory::Images},  {"tiff", MediaCategory::Images},
        {"webp", MediaCategory::Images}, {"svg", MediaCategory::Images}};

/**
 * @brief Класс для инспектирования директории и сбора информации о медиафайлах
 *
 * Класс выполняет рекурсивное сканирование указанной директории,
 * классифицирует файлы по типам (аудио, видео, изображения) и
 * сохраняет результаты в JSON строку, доступную через getResponse().
 */
class Inspector {
public:
  /**
   * @brief Конструктор класса Inspector
   * @param path Путь к директории для сканирования
   * @param interval Интервал между сканированиями в секундах (по умолчанию: 5)
   */
  Inspector(const std::string &path, uint32_t interval = 5);

  /**
   * @brief Деструктор - останавливает поток сканирования
   */
  ~Inspector();

  Inspector(const Inspector &) = delete;
  Inspector &operator=(const Inspector &) = delete;

  /**
   * @brief Запускает процесс сканирования в отдельном потоке
   */
  void run();

  /**
   * @brief Останавливает процесс сканирования
   *
   * Безопасно завершает рабочий поток и ожидает его окончания
   */
  void stop();

  /**
   * @brief Возвращает копию текущего JSON ответа с результатами сканирования
   * @return JSON-строка со списками аудио, видео и изображений
   */
  std::string getResponse() const;

private:
  /**
   * @brief Основной цикл сканирования
   *
   * Выполняет периодическое сканирование директории с заданным интервалом
   */
  void scanLoop();

  /**
   * @brief Выполняет однократное сканирование директории
   *
   * Рекурсивно обходит директорию, классифицирует файлы и вызывает
   * updateResponse()
   */
  void performScan();

  /**
   * @brief Извлекает расширение файла из пути
   * @param path Путь к файлу
   * @return Расширение файла в нижнем регистре или пустая строка, если
   * расширение отсутствует
   */
  static std::string getExtension(const std::string path);

  /**
   * @brief Определяет категорию медиафайла по его расширению
   * @param path Путь к файлу
   * @return Категория медиафайла (Audio, Video, Images, Unknown)
   */
  static MediaCategory getCategory(const std::string path);

  /**
   * @brief Формирует и сохраняет JSON строку в response_ с результатами
   * сканирования
   * @param audio Список аудиофайлов
   * @param video Список видеофайлов
   * @param images Список изображений
   */
  void updateResponse(const std::vector<std::string> &audio,
                      const std::vector<std::string> &video,
                      const std::vector<std::string> &images);

  std::string scanPath_;            ///< Путь к директории для сканирования
  std::atomic<bool> running_{true}; ///< Флаг состояния работы инспектора
  std::atomic<uint32_t> interval_;  ///< Интервал сканирования в секундах
  std::unique_ptr<std::thread>
      scanThread_; ///< Поток для выполнения сканирования

  mutable std::mutex responseMutex_; ///< Мьютекс для защиты response_
  std::string response_;             ///< Текущий JSON ответ
};
