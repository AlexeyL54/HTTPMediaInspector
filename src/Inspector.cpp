#include "Inspector.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <fts.h>
#include <iostream>
#include <string>

Inspector::Inspector(const std::string &path, uint32_t interval)
    : scanPath_(path), interval_(interval) {
  response_ = R"({"audio":[],"video":[],"images":[]})";
}

Inspector::~Inspector() { stop(); }

/**
 * @brief Запускает процесс сканирования в отдельном потоке
 */
void Inspector::run() {
  scanThread_ = std::make_unique<std::thread>([this]() { scanLoop(); });
}

/**
 * @brief Останавливает процесс сканирования
 *
 * Безопасно завершает рабочий поток и ожидает его окончания
 */
void Inspector::stop() {
  running_ = false;

  if (scanThread_ && scanThread_->joinable()) {
    scanThread_->join();
  }
}

/**
 * @brief Основной цикл сканирования
 *
 * Выполняет периодическое сканирование директории с заданным интервалом
 */
void Inspector::scanLoop() {
  performScan();

  while (running_) {
    std::this_thread::sleep_for(std::chrono::seconds(interval_.load()));

    if (running_) {
      performScan();
    }
  }
}

/**
 * @brief Извлекает расширение файла из пути
 * @param path Путь к файлу
 * @return Расширение файла в нижнем регистре или пустая строка, если расширение
 * отсутствует
 */
std::string Inspector::getExtension(const std::string path) {
  size_t dotPos = path.find_last_of('.');
  size_t slashPos = path.find_last_of('/');

  if (dotPos == std::string::npos ||
      (slashPos != std::string::npos && slashPos > dotPos)) {
    return "";
  }

  std::string ext = path.substr(dotPos + 1);
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  return ext;
}

/**
 * @brief Определяет категорию медиафайла по его расширению
 * @param path Путь к файлу
 * @return Категория медиафайла (Audio, Video, Images, Unknown)
 */
MediaCategory Inspector::getCategory(const std::string path) {
  std::string ext = getExtension(path);
  auto it = extensionMappings.find(ext);

  if (it != extensionMappings.end()) {
    return it->second;
  }
  return MediaCategory::Unknown;
}

/**
 * @brief Экранирует специальные символы для корректного JSON формата
 * @param str Входная строка
 * @return Строка с экранированными символами
 */
static std::string escapeJson(const std::string &str) {
  std::string result;
  result.reserve(str.length());

  for (char c : str) {
    switch (c) {
    case '"':
      result += "\\\"";
      break;
    case '\\':
      result += "\\\\";
      break;
    case '\b':
      result += "\\b";
      break;
    case '\f':
      result += "\\f";
      break;
    case '\n':
      result += "\\n";
      break;
    case '\r':
      result += "\\r";
      break;
    case '\t':
      result += "\\t";
      break;
    default:
      if (static_cast<unsigned char>(c) < 0x20) {
        char buf[8];
        snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
        result += buf;
      } else {
        result += c;
      }
      break;
    }
  }
  return result;
}

/**
 * @brief Выполняет однократное сканирование директории
 *
 * Рекурсивно обходит директорию, классифицирует файлы и вызывает
 * updateResponse()
 */
void Inspector::performScan() {
  std::vector<std::string> newAudio;
  std::vector<std::string> newVideo;
  std::vector<std::string> newImages;

  char *paths[] = {const_cast<char *>(scanPath_.c_str()), nullptr};

  // FTS_PHYSICAL: физический обход (не следует по симлинкам на директории,
  // чтобы избежать циклов)
  // FTS_NOCHDIR: не менять текущую директорию процесса
  // FTS_XDEV: не переходить на другие файловые устройства (опционально, но
  // полезно для стабильности)
  FTSENT *ent;
  FTS *ftsp = fts_open(paths, FTS_PHYSICAL | FTS_NOCHDIR | FTS_XDEV, nullptr);

  if (!ftsp) {
    std::cerr << "Ошибка: не удалось открыть директорию для сканирования: "
              << scanPath_ << std::endl;
    return;
  }

  while ((ent = fts_read(ftsp)) != nullptr) {
    switch (ent->fts_info) {
    case FTS_F: {
      std::string path(ent->fts_path);

      MediaCategory cat = getCategory(path);
      switch (cat) {
      case MediaCategory::Audio:
        newAudio.push_back(std::move(path));
        break;
      case MediaCategory::Video:
        newVideo.push_back(std::move(path));
        break;
      case MediaCategory::Images:
        newImages.push_back(std::move(path));
        break;
      default:
        break;
      }
      break;
    }
    case FTS_NS:
      std::cerr << "Ошибка: не удалось получить доступ к " << ent->fts_path
                << std::endl;
      break;
    default:
      break;
    }
  }

  fts_close(ftsp);

  updateResponse(newAudio, newVideo, newImages);
}

/**
 * @brief Формирует и сохраняет JSON строку в response_ с результатами
 * сканирования
 * @param audio Вектор аудиофайлов
 * @param video Вектор видеофайлов
 * @param images Вектор изображений
 */
void Inspector::updateResponse(const std::vector<std::string> &audio,
                               const std::vector<std::string> &video,
                               const std::vector<std::string> &images) {
  std::string newResponse;
  newResponse.reserve(1024);

  newResponse += "{\"audio\":[";
  for (size_t i = 0; i < audio.size(); ++i) {
    if (i > 0)
      newResponse += ',';
    newResponse += '"';
    newResponse += escapeJson(audio[i]);
    newResponse += '"';
  }
  newResponse += "],\"video\":[";
  for (size_t i = 0; i < video.size(); ++i) {
    if (i > 0)
      newResponse += ',';
    newResponse += '"';
    newResponse += escapeJson(video[i]);
    newResponse += '"';
  }
  newResponse += "],\"images\":[";
  for (size_t i = 0; i < images.size(); ++i) {
    if (i > 0)
      newResponse += ',';
    newResponse += '"';
    newResponse += escapeJson(images[i]);
    newResponse += '"';
  }
  newResponse += "]}";

  {
    std::lock_guard<std::mutex> lock(responseMutex_);
    response_ = std::move(newResponse);
  }
}

/**
 * @brief Возвращает копию текущего JSON ответа с результатами сканирования
 * @return JSON-строка со списками аудио, видео и изображений
 */
std::string Inspector::getResponse() const {
  std::lock_guard<std::mutex> lock(responseMutex_);
  return response_;
}
