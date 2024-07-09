#pragma once

// C++ standard
#include <concepts>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace oph {
class ThreadPool {
 public:
  ThreadPool(size_t num_threads = std::thread::hardware_concurrency()) {
    workers_.reserve(num_threads);
    for (size_t i = 0; i < num_threads; i++) {
      workers_.emplace_back([this](std::stop_token stoken) {
        std::function<void()> task;
        for (;;) {
          std::unique_lock<std::mutex> lock(tasks_mutex_);
          tasks_cv_.wait(lock, [this, &stoken]() { return stoken.stop_requested() || !tasks_.empty(); });
          if (stoken.stop_requested() && tasks_.empty()) {
            return;
          }

          task = std::move(tasks_.front());
          tasks_.pop();
          lock.unlock();

          task();
        }
      });
    }
  }

  ~ThreadPool() {
    for (auto& worker : workers_) {
      worker.request_stop();
    }
    tasks_cv_.notify_all();
    for (auto& worker : workers_) {
      worker.join();
    }
  }

  template <typename Callable, typename... Args>
    requires std::is_invocable_v<Callable, Args...>
  std::future<std::invoke_result_t<Callable, Args...>> Enqueue(Callable&& func, Args&&... args) {
    using ReturnType = std::invoke_result_t<Callable, Args...>;

    auto promise = std::make_shared<std::promise<ReturnType>>();
    auto future = promise->get_future();

    tasks_mutex_.lock();
    tasks_.emplace([_func = std::forward<Callable>(func), ... _args = std::forward<Args>(args), _promise = promise]() mutable {
      try {
        if constexpr (std::is_same_v<ReturnType, void>) {
          std::invoke(_func, std::move(_args)...);
          _promise->set_value();
        } else {
          _promise->set_value(std::invoke(_func, std::move(_args)...));
        }
      } catch (...) {
        _promise->set_exception(std::current_exception());
      }
    });
    tasks_mutex_.unlock();
    tasks_cv_.notify_one();

    return future;
  }

  template <typename Callable, typename... Args>
    requires std::is_invocable_v<Callable, Args...>
  void EnqueueDetach(Callable&& func, Args&&... args) {
    using ReturnType = std::invoke_result_t<Callable, Args...>;

    tasks_mutex_.lock();
    tasks_.emplace([_func = std::forward<Callable>(func), ... _args = std::forward<Args>(args)]() mutable {
      try {
        if constexpr (std::is_same_v<ReturnType, void>) {
          std::invoke(_func, std::move(_args)...);
        } else {
          std::ignore = std::invoke(_func, std::move(_args)...);
        }
      } catch (...) {
      }
    });
    tasks_mutex_.unlock();
    tasks_cv_.notify_one();
  }

 private:
  ThreadPool(const ThreadPool&) = delete;
  ThreadPool(ThreadPool&&) noexcept = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;
  ThreadPool& operator=(ThreadPool&&) noexcept = delete;

  std::vector<std::jthread> workers_;
  std::queue<std::function<void()>> tasks_;
  std::mutex tasks_mutex_;
  std::condition_variable tasks_cv_;
};
}  // namespace oph
