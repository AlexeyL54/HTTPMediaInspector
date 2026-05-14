// Inspector.hpp
#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

extern "C" {
#include "../lib/cJSON/cJSON.h"
}

/**
 * @brief Категория мультимедийного файла.
 */
enum class MediaCategory {
  Audio, /// Аудиофайлы (mp3, wav, flac, ...)
  Video, /// Видеофайлы (mp4, avi, mkv, ...)
  Images /// Изображения (jpg, png, gif, ...)
};

/**
 * @brief Сопоставление расширения файла с категорией.
 */
struct ExtensionMapping {
  const char *extension;  /// Расширение файла (без точки, в нижнем регистре)
  MediaCategory category; /// Соответствующая категория
};

/**
 * @brief Сканирует директорию для поиска мультимедийных файлов и формирует
 * JSON.
 *
 * Запускает фоновый поток, который периодически сканирует директорию с помощью
 * nftw() для поиска аудио, видео и изображений. Результаты хранятся в виде JSON
 * и могут быть получены через getResponse().
 */
class Inspector {
public:
  /**
   * @brief Конструктор.
   * @param path Путь для сканирования.
   * @param interval Интервал сканирования в секундах.
   */
  Inspector(const std::string &path, uint32_t interval = 5);

  /**
   * @brief Деструктор. Останавливает фоновый поток сканирования.
   */
  ~Inspector();

  // Запрещаем копирование
  Inspector(const Inspector &) = delete;
  Inspector &operator=(const Inspector &) = delete;

  /**
   * @brief Запускает фоновый поток сканирования.
   */
  void run();

  /**
   * @brief Возвращает текущий JSON-ответ со списками файлов.
   * @return JSON-строка вида:
   *         {"audio":["a.mp3"],"video":["v.mp4"],"images":["i.jpg"]}
   */
  std::string getResponse() const;

private:
  /**
   * @brief Основной цикл фонового потока сканирования.
   */
  void scanLoop();

  /**
   * @brief Выполняет однократное сканирование с помощью nftw().
   */
  void performScan();

  /**
   * @brief Формирует JSON-ответ из текущих списков файлов.
   */
  void buildJsonResponse();

  /**
   * @brief Callback-функция nftw() для обработки каждого файла.
   * @param path Путь к файлу.
   * @param sb   Структура stat файла.
   * @param typeflag Тип файла.
   * @param ftwbuf  Буфер nftw().
   * @return 0 для продолжения, не 0 для остановки.
   */
  static int nftwCallback(const char *path, const struct stat *sb, int typeflag,
                          struct FTW *ftwbuf);

  /**
   * @brief Возвращает расширение файла в нижнем регистре.
   * @param filename Имя файла.
   * @return Расширение без точки в нижнем регистре.
   */
  static std::string getExtension(const char *filename);

  /**
   * @brief Определяет категорию мультимедиа по имени файла.
   * @param filename Имя файла.
   * @return Категория MediaCategory или значение -1, если файл не
   * мультимедийный.
   */
  static MediaCategory getCategory(const char *filename);

  /**
   * @brief Возвращает таблицу сопоставления расширений с категориями.
   * @return Ссылка на вектор сопоставлений.
   */
  static const std::vector<ExtensionMapping> &getExtensionMappings();

  std::string scanPath_;                    /// Путь для сканирования
  std::string response_;                    /// Текущий JSON-ответ
  mutable std::mutex mutex_;                /// Мьютекс для защиты данных
  std::atomic<bool> running_{true};         /// Флаг работы фонового потока
  std::atomic<uint32_t> interval_;          /// Интервал сканирования (сек)
  std::unique_ptr<std::thread> scanThread_; /// Поток сканирования

  // Списки файлов (защищаются мьютексом)
  std::vector<std::string> audioFiles_; /// Список аудиофайлов
  std::vector<std::string> videoFiles_; /// Список видеофайлов
  std::vector<std::string> imageFiles_; /// Список изображений
};
