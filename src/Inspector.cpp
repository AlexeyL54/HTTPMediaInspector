// Inspector.cpp
#include "Inspector.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <ftw.h>
#include <iostream>

extern "C" {
#include "../lib/cJSON/cJSON.h"
}

// Статический указатель для доступа к экземпляру из callback-функции nftw
static Inspector *g_currentInspector = nullptr;

// Таблица сопоставления расширений с категориями
static const std::vector<ExtensionMapping> g_extensionMappings = {
    // Аудиофайлы
    {"mp3", MediaCategory::Audio},
    {"wav", MediaCategory::Audio},
    {"flac", MediaCategory::Audio},
    {"ogg", MediaCategory::Audio},
    {"m4a", MediaCategory::Audio},
    {"aac", MediaCategory::Audio},
    {"wma", MediaCategory::Audio},

    // Видеофайлы
    {"mp4", MediaCategory::Video},
    {"avi", MediaCategory::Video},
    {"mkv", MediaCategory::Video},
    {"mov", MediaCategory::Video},
    {"wmv", MediaCategory::Video},
    {"flv", MediaCategory::Video},
    {"webm", MediaCategory::Video},
    {"mpeg", MediaCategory::Video},
    {"mpg", MediaCategory::Video},

    // Изображения
    {"jpg", MediaCategory::Images},
    {"jpeg", MediaCategory::Images},
    {"png", MediaCategory::Images},
    {"gif", MediaCategory::Images},
    {"bmp", MediaCategory::Images},
    {"tiff", MediaCategory::Images},
    {"webp", MediaCategory::Images},
    {"svg", MediaCategory::Images}};

const std::vector<ExtensionMapping> &Inspector::getExtensionMappings() {
  return g_extensionMappings;
}

Inspector::Inspector(const std::string &path, uint32_t interval)
    : scanPath_(path), interval_(interval) {}

Inspector::~Inspector() {
  running_ = false;
  if (scanThread_ && scanThread_->joinable()) {
    scanThread_->join();
  }
}

void Inspector::run() {
  scanThread_ = std::make_unique<std::thread>([this]() { scanLoop(); });
}

void Inspector::scanLoop() {
  performScan();

  while (running_) {
    std::this_thread::sleep_for(std::chrono::seconds(interval_.load()));
    if (running_) {
      performScan();
    }
  }
}

void Inspector::performScan() {
  // Подменяем векторы на время сканирования, чтобы не блокировать чтение
  std::vector<std::string> newAudio;
  std::vector<std::string> newVideo;
  std::vector<std::string> newImages;

  // Сохраняем указатели на старые векторы
  std::vector<std::string> *savedAudio = &audioFiles_;
  std::vector<std::string> *savedVideo = &videoFiles_;
  std::vector<std::string> *savedImages = &imageFiles_;

  // Перенаправляем член-векторы на локальные (временные)
  audioFiles_ = std::vector<std::string>();
  videoFiles_ = std::vector<std::string>();
  imageFiles_ = std::vector<std::string>();

  g_currentInspector = this;

  // Рекурсивный обход директории с помощью nftw()
  // FTW_PHYS: не следовать по символическим ссылкам
  // FTW_MOUNT: оставаться в пределах одной файловой системы
  // FTW_DEPTH: обход в глубину
  int result = nftw(scanPath_.c_str(), nftwCallback, 20,
                    FTW_PHYS | FTW_MOUNT | FTW_DEPTH);

  if (result != 0) {
    std::cerr << "Предупреждение: nftw() завершился с кодом " << result
              << std::endl;
  }

  g_currentInspector = nullptr;

  // Сортируем для консистентного вывода
  std::sort(audioFiles_.begin(), audioFiles_.end());
  std::sort(videoFiles_.begin(), videoFiles_.end());
  std::sort(imageFiles_.begin(), imageFiles_.end());

  // Атомарно заменяем старые данные новыми (минимальная критическая секция)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    savedAudio->swap(audioFiles_);
    savedVideo->swap(videoFiles_);
    savedImages->swap(imageFiles_);

    // Восстанавливаем член-векторы (после swap они содержат старые данные)
    audioFiles_.swap(*savedAudio);
    videoFiles_.swap(*savedVideo);
    imageFiles_.swap(*savedImages);
  }

  // Формируем JSON-ответ
  buildJsonResponse();
}

int Inspector::nftwCallback(const char *path, const struct stat *sb,
                            int typeflag, struct FTW *ftwbuf) {
  (void)sb;     // Неиспользуемый параметр
  (void)ftwbuf; // Неиспользуемый параметр

  // Пропускаем директории и не-регулярные файлы
  if (typeflag != FTW_F) {
    return 0;
  }

  // Извлекаем имя файла (последний компонент пути)
  const char *filename = strrchr(path, '/');
  filename = (filename != nullptr) ? filename + 1 : path;

  MediaCategory category = getCategory(filename);

  if (!g_currentInspector) {
    return 0;
  }

  // Добавляем файл в соответствующий список
  switch (category) {
  case MediaCategory::Audio:
    g_currentInspector->audioFiles_.push_back(filename);
    break;
  case MediaCategory::Video:
    g_currentInspector->videoFiles_.push_back(filename);
    break;
  case MediaCategory::Images:
    g_currentInspector->imageFiles_.push_back(filename);
    break;
  default:
    break; // Не мультимедийный файл — пропускаем
  }

  return 0;
}

std::string Inspector::getExtension(const char *filename) {
  const char *dot = strrchr(filename, '.');
  if (!dot || dot == filename) {
    return "";
  }

  std::string ext(dot + 1);
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  return ext;
}

MediaCategory Inspector::getCategory(const char *filename) {
  std::string ext = getExtension(filename);
  if (ext.empty()) {
    return static_cast<MediaCategory>(-1);
  }

  for (const ExtensionMapping &mapping : getExtensionMappings()) {
    if (ext == mapping.extension) {
      return mapping.category;
    }
  }

  return static_cast<MediaCategory>(-1);
}

void Inspector::buildJsonResponse() {
  cJSON *root = cJSON_CreateObject();
  if (!root) {
    return;
  }

  std::lock_guard<std::mutex> lock(mutex_);

  // Добавляем массив аудиофайлов
  cJSON *audioArray = cJSON_CreateArray();
  for (const std::string &file : audioFiles_) {
    cJSON_AddItemToArray(audioArray, cJSON_CreateString(file.c_str()));
  }
  cJSON_AddItemToObject(root, "audio", audioArray);

  // Добавляем массив видеофайлов
  cJSON *videoArray = cJSON_CreateArray();
  for (const std::string &file : videoFiles_) {
    cJSON_AddItemToArray(videoArray, cJSON_CreateString(file.c_str()));
  }
  cJSON_AddItemToObject(root, "video", videoArray);

  // Добавляем массив изображений
  cJSON *imagesArray = cJSON_CreateArray();
  for (const std::string &file : imageFiles_) {
    cJSON_AddItemToArray(imagesArray, cJSON_CreateString(file.c_str()));
  }
  cJSON_AddItemToObject(root, "images", imagesArray);

  // Преобразуем в строку
  char *jsonString = cJSON_PrintUnformatted(root);
  if (jsonString) {
    response_ = jsonString;
    cJSON_free(jsonString);
  }

  cJSON_Delete(root);
}

std::string Inspector::getResponse() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return response_;
}
