#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
  std::vector<std::thread> workers_;
  std::queue<std::function<void()>> q_;
  std::mutex m_;
  std::condition_variable cv_;
  bool stopping_ = false;

  void worker_loop();

 public:
  ThreadPool(size_t n);
  ~ThreadPool();

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  void submit(std::function<void()> task);
};
