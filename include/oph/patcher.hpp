#pragma once

// C++ standard
#include <atomic>
#include <concepts>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

// This project
#include "formatter.hpp"
#include "memory.hpp"
#include "thread-pool.hpp"

namespace oph {
class Patcher {
 public:
  enum LangType {
    kCpp,
  };

  Patcher(LangType format_type) : formatter_(NewFormatter(format_type)) {
  }

  Patcher(LangType format_type, size_t num_threads)
      : formatter_(NewFormatter(format_type)), scan_pool_(num_threads) {
  }

  ~Patcher() {
    delete formatter_;
  }

  void AddModule(const std::string& process_name, const std::vector<std::string>& module_names = {}) {
    dump_store_.DumpModule(process_name, module_names);
  }

  Patcher& WriteLineBreak() {
    formatter_->WriteLineBreak();
    return *this;
  }

  Patcher& WriteModule(const std::string& name) {
    if (dump_store_.Contains(name)) {
      formatter_->WriteModule(name, dump_store_.GetModule(name).GetVersion());
    } else {
      formatter_->WriteModule(name, "ERROR");
    }
    return *this;
  }

  Patcher& WriteComment(std::string_view comment) {
    formatter_->WriteComment(comment);
    return *this;
  }

  template <typename ScanFunc>
    requires std::is_invocable_v<ScanFunc, const DumpStore&> && std::is_same_v<std::invoke_result_t<ScanFunc, const DumpStore&>, uint64_t>
  Patcher& WriteOffset(std::string_view name, ScanFunc&& scan_func) {
    formatter_->WriteOffset(name);

    size_t scan_index = scan_results_.size();
    scan_results_.resize(scan_index + 1);

    scan_wg_.Add();
    scan_pool_.EnqueueDetach(&Patcher::ScanOffset, this, scan_index, std::forward<ScanFunc>(scan_func));

    return *this;
  }

  template <typename ScanFunc>
    requires std::is_invocable_v<ScanFunc, const DumpStore&> && std::is_same_v<std::invoke_result_t<ScanFunc, const DumpStore&>, std::vector<uint64_t>>
  Patcher& WriteOffsets(std::string_view name, ScanFunc&& scan_func) {
    formatter_->WriteOffsets(name);

    size_t scan_index = scan_results_.size();
    scan_results_.resize(scan_index + 1);

    scan_wg_.Add();
    scan_pool_.EnqueueDetach(&Patcher::ScanOffsets, this, scan_index, std::forward<ScanFunc>(scan_func));

    return *this;
  }

  template <typename ScanFunc>
    requires std::is_invocable_v<ScanFunc, const DumpStore&> && std::is_same_v<std::invoke_result_t<ScanFunc, const DumpStore&>, std::vector<uint8_t>>
  Patcher& WriteBytes(std::string_view name, ScanFunc&& scan_func) {
    formatter_->WriteBytes(name);

    size_t scan_index = scan_results_.size();
    scan_results_.resize(scan_index + 1);

    scan_wg_.Add();
    scan_pool_.EnqueueDetach(&Patcher::ScanBytes, this, scan_index, std::forward<ScanFunc>(scan_func));

    return *this;
  }

  void Export(std::ostream& os) {
    scan_wg_.Wait();
    formatter_->Export(os, scan_results_);
  }

  void Export(const std::string& file_path) {
    std::ofstream file(file_path, std::ios::binary | std::ios::trunc);
    if (file.is_open()) {
      Export(file);
    }
  }

 private:
  class WaitGroup {
   public:
    void Add() {
      counter_++;
    }

    void Done() {
      counter_--;
      cv_.notify_all();
    }

    void Wait() {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [&] { return counter_ <= 0; });
    }

   private:
    std::atomic_int32_t counter_;
    std::mutex mutex_;
    std::condition_variable cv_;
  };

  void ScanOffset(size_t scan_index, std::function<uint64_t(const DumpStore&)>&& scan_func) {
    try {
      scan_results_[scan_index] = formatter_->MakeOffset(scan_func(dump_store_));
    } catch (...) {
      scan_results_[scan_index] = "ERROR";
    }
    scan_wg_.Done();
  }

  void ScanOffsets(size_t scan_index, std::function<std::vector<uint64_t>(const DumpStore&)>&& scan_func) {
    try {
      scan_results_[scan_index] = formatter_->MakeOffsets(scan_func(dump_store_));
    } catch (...) {
      scan_results_[scan_index] = "ERROR";
    }
    scan_wg_.Done();
  }

  void ScanBytes(size_t scan_index, std::function<std::vector<uint8_t>(const DumpStore&)>&& scan_func) {
    try {
      scan_results_[scan_index] = formatter_->MakeBytes(scan_func(dump_store_));
    } catch (...) {
      scan_results_[scan_index] = "ERROR";
    }
    scan_wg_.Done();
  }

  static Formatter* NewFormatter(LangType lang_type) {
    switch (lang_type) {
      case oph::Patcher::kCpp:
        return new CppFormatter();
      default:
        return new CppFormatter();
    }
  }

  DumpStore dump_store_;
  Formatter* formatter_;
  std::vector<std::string> scan_results_;
  WaitGroup scan_wg_;
  ThreadPool scan_pool_;
};
}  // namespace oph
