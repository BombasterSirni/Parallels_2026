#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <stop_token>
#include <thread>
#include <unordered_map>
#include <utility>

template <typename T>
class TaskServer {
public:
    using Task = std::function<T()>;

    TaskServer() = default;
    ~TaskServer() { stop(); }

    TaskServer(const TaskServer &) = delete;
    TaskServer &operator=(const TaskServer &) = delete;

    void start() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_) {
            return;
        }
        running_ = true;
        worker_ = std::jthread([this](std::stop_token stoken) { worker_loop(stoken); });
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!running_) {
                return;
            }
            running_ = false;
        }

        if (worker_.joinable()) {
            worker_.request_stop();
        }
        task_cv_.notify_all();
        result_cv_.notify_all();
    }

    std::size_t add_task(Task task) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            throw std::runtime_error("Server is not running");
        }

        const std::size_t id = next_id_++;
        tasks_.emplace(id, std::move(task));
        task_cv_.notify_one();
        return id;
    }

    T request_result(std::size_t id) {
        std::unique_lock<std::mutex> lock(mutex_);
        result_cv_.wait(lock, [&]() {
            return results_.find(id) != results_.end() || (!running_ && tasks_.empty());
        });

        auto it = results_.find(id);
        if (it == results_.end()) {
            throw std::runtime_error("Result is unavailable");
        }

        T value = std::move(it->second);
        results_.erase(it);
        return value;
    }

private:
    void worker_loop(std::stop_token stoken) {
        while (!stoken.stop_requested()) {
            std::pair<std::size_t, Task> current;

            {
                std::unique_lock<std::mutex> lock(mutex_);
                task_cv_.wait(lock, [&]() {
                    return stoken.stop_requested() || !tasks_.empty() || !running_;
                });

                if ((stoken.stop_requested() || !running_) && tasks_.empty()) {
                    break;
                }
                if (tasks_.empty()) {
                    continue;
                }

                current = std::move(tasks_.front());
                tasks_.pop();
            }

            T value = current.second();

            {
                std::lock_guard<std::mutex> lock(mutex_);
                results_[current.first] = std::move(value);
            }
            result_cv_.notify_all();
        }
    }

    std::mutex mutex_;
    std::condition_variable task_cv_;
    std::condition_variable result_cv_;
    std::queue<std::pair<std::size_t, Task>> tasks_;
    std::unordered_map<std::size_t, T> results_;
    std::jthread worker_;

    std::size_t next_id_ = 1;
    bool running_ = false;
};
