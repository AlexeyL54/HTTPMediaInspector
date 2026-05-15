#include "../lib/cpp-httplib/httplib.h"
#include "../src/Inspector.hpp"
#include "../src/Server.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <random>
#include <string>
#include <thread>

namespace fs = std::filesystem;

// --- Вспомогательные функции  ---

static std::string generateRandomDirName() {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_int_distribution<> dis(100000, 999999);
  return "/tmp/test_media_" + std::to_string(dis(gen));
}

static void createDummyFile(const std::string &path) {
  std::ofstream file(path);
  file << "dummy data";
  file.close();
}

static std::string getJsonFromServer(const std::string &host, int port) {
  httplib::Client client(host, port);

  httplib::Result res = client.Get("/media_files");
  if (!res || res->status != 200) {
    return "";
  }
  return res->body;
}

static bool contains(const std::string &haystack, const std::string &needle) {
  return haystack.find(needle) != std::string::npos;
}

// --- Тесты ---

TEST(InspectorWithServer, EmptyDirReturnsValidJson) {
  std::string testDir = generateRandomDirName();
  fs::create_directories(testDir);
  uint16_t port = 18080; // Используем нестандартный порт для тестов

  {
    auto inspector = std::make_unique<Inspector>(testDir, 1);
    Server server(port, std::move(inspector));

    // Запускаем сервер в отдельном потоке
    std::thread serverThread([&server]() { server.run(); });

    // Даём серверу время запуститься
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Получаем JSON через HTTP
    std::string content = getJsonFromServer("localhost", port);

    EXPECT_EQ(content, "{\"audio\":[],\"video\":[],\"images\":[]}");

    server.stop();
    if (serverThread.joinable()) {
      serverThread.join();
    }
  }

  fs::remove_all(testDir);
}

TEST(InspectorWithServer, DetectsMediaFiles) {
  std::string testDir = generateRandomDirName();
  fs::create_directories(testDir);
  uint16_t port = 18081;

  createDummyFile(testDir + "/song.mp3");
  createDummyFile(testDir + "/clip.avi");
  createDummyFile(testDir + "/photo.png");
  createDummyFile(testDir + "/readme.txt");

  {
    auto inspector = std::make_unique<Inspector>(testDir, 1);
    Server server(port, std::move(inspector));

    std::thread serverThread([&server]() { server.run(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::string content = getJsonFromServer("localhost", port);

    EXPECT_TRUE(contains(content, "song.mp3")) << "MP3 file not detected";
    EXPECT_TRUE(contains(content, "clip.avi")) << "AVI file not detected";
    EXPECT_TRUE(contains(content, "photo.png")) << "PNG file not detected";
    EXPECT_FALSE(contains(content, "readme.txt"))
        << "TXT file incorrectly detected";

    size_t audioPos = content.find("\"audio\":");
    size_t videoPos = content.find("\"video\":");
    size_t imgPos = content.find("\"images\":");

    ASSERT_NE(audioPos, std::string::npos);
    ASSERT_NE(videoPos, std::string::npos);
    ASSERT_NE(imgPos, std::string::npos);

    EXPECT_GT(content.find("song.mp3"), audioPos);
    EXPECT_LT(content.find("song.mp3"), videoPos);

    server.stop();
    if (serverThread.joinable()) {
      serverThread.join();
    }
  }

  fs::remove_all(testDir);
}

TEST(InspectorWithServer, DynamicUpdateAddRemove) {
  std::string testDir = generateRandomDirName();
  fs::create_directories(testDir);
  uint16_t port = 18082;

  auto inspector = std::make_unique<Inspector>(testDir, 1);
  Inspector *inspectorPtr = inspector.get();
  Server server(port, std::move(inspector));

  std::thread serverThread([&server]() { server.run(); });

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  // Начальное состояние - пусто
  std::string content1 = getJsonFromServer("localhost", port);
  EXPECT_FALSE(contains(content1, "new_file.wav"));

  // Добавляем файл
  createDummyFile(testDir + "/new_file.wav");
  std::this_thread::sleep_for(std::chrono::seconds(2));

  std::string content2 = getJsonFromServer("localhost", port);
  EXPECT_TRUE(contains(content2, "new_file.wav"))
      << "File not detected after add";

  // Удаляем файл
  fs::remove(testDir + "/new_file.wav");
  std::this_thread::sleep_for(std::chrono::seconds(2));

  std::string content3 = getJsonFromServer("localhost", port);
  EXPECT_FALSE(contains(content3, "new_file.wav"))
      << "File still present after delete";

  server.stop();
  if (serverThread.joinable()) {
    serverThread.join();
  }

  fs::remove_all(testDir);
}

TEST(InspectorWithServer, StopWorks) {
  std::string testDir = generateRandomDirName();
  fs::create_directories(testDir);
  uint16_t port = 18083;

  auto inspector = std::make_unique<Inspector>(testDir, 1);
  Server server(port, std::move(inspector));

  std::thread serverThread([&server]() { server.run(); });

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  server.stop();

  // Добавляем файл после остановки
  createDummyFile(testDir + "/after_stop.jpg");
  std::this_thread::sleep_for(std::chrono::seconds(2));

  httplib::Client client("localhost", port);
  httplib::Result res = client.Get("/media_files");

  EXPECT_TRUE(!res || res->status != 200);

  if (serverThread.joinable()) {
    serverThread.join();
  }

  fs::remove_all(testDir);
}

TEST(InspectorWithServer, RecursiveScan) {
  std::string testDir = generateRandomDirName();
  std::string subDir = testDir + "/sub1/sub2";
  fs::create_directories(subDir);
  uint16_t port = 18084;

  createDummyFile(subDir + "/deep.mp4");

  {
    auto inspector = std::make_unique<Inspector>(testDir, 1);
    Server server(port, std::move(inspector));

    std::thread serverThread([&server]() { server.run(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::string content = getJsonFromServer("localhost", port);
    EXPECT_TRUE(contains(content, "deep.mp4"))
        << "File in subdirectory not detected";

    server.stop();
    if (serverThread.joinable()) {
      serverThread.join();
    }
  }

  fs::remove_all(testDir);
}

TEST(InspectorWithServer, CustomIntervalAndPath) {
  std::string testDir = generateRandomDirName();
  fs::create_directories(testDir);
  uint16_t port = 18085;
  uint32_t interval = 2;

  auto inspector = std::make_unique<Inspector>(testDir, interval);
  Server server(port, std::move(inspector));

  std::thread serverThread([&server]() { server.run(); });

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  auto start = std::chrono::steady_clock::now();

  createDummyFile(testDir + "/test_interval.mp3");

  std::this_thread::sleep_for(std::chrono::seconds(interval + 1));

  std::string content = getJsonFromServer("localhost", port);
  EXPECT_TRUE(contains(content, "test_interval.mp3"))
      << "File not detected with custom interval";

  auto elapsed = std::chrono::steady_clock::now() - start;
  EXPECT_GE(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count(),
            interval);

  server.stop();
  if (serverThread.joinable()) {
    serverThread.join();
  }

  fs::remove_all(testDir);
}

TEST(InspectorWithServer, ConcurrentAccess) {
  std::string testDir = generateRandomDirName();
  fs::create_directories(testDir);
  uint16_t port = 18087;

  // Создаём много файлов
  for (int i = 0; i < 10; ++i) {
    createDummyFile(testDir + "/file" + std::to_string(i) + ".mp3");
  }

  auto inspector = std::make_unique<Inspector>(testDir, 1);
  Server server(port, std::move(inspector));

  std::thread serverThread([&server]() { server.run(); });

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  // Делаем много параллельных запросов
  std::vector<std::thread> clientThreads;
  std::atomic<int> successCount{0};

  for (int i = 0; i < 20; ++i) {
    clientThreads.emplace_back([port, &successCount]() {
      httplib::Client client("localhost", port);
      auto res = client.Get("/media_files");
      if (res && res->status == 200 && !res->body.empty()) {
        successCount++;
      }
    });
  }

  for (std::thread &t : clientThreads) {
    t.join();
  }

  EXPECT_EQ(successCount, 20) << "Some concurrent requests failed";

  server.stop();
  if (serverThread.joinable()) {
    serverThread.join();
  }

  fs::remove_all(testDir);
}
