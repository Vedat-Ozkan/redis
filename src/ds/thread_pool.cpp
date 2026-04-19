#include "thread_pool.h"

using namespace std;

void ThreadPool::worker_loop() {
  while (true) {
    function<void()> work;
    {
      unique_lock<mutex> lock(m_);
      cv_.wait(lock, [this] { return stopping_ || !q_.empty(); });
      if (stopping_ && q_.empty()) {
        return;
      }
      work = move(q_.front());
      q_.pop();
    }
    work();
  }
}

ThreadPool::ThreadPool(size_t n) {
  for (size_t i = 0; i < n; i++) {
    workers_.emplace_back([this] { worker_loop(); });
  }
}

ThreadPool::~ThreadPool() {
  {
    lock_guard<mutex> lk(m_);
    stopping_ = true;
  }
  cv_.notify_all();
  for (auto& t : workers_) {
    t.join();
  }
}

void ThreadPool::submit(function<void()> task) {
  {
    lock_guard<mutex> lock(m_);
    q_.push(move(task));
  }
  cv_.notify_one();
}
