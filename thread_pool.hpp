#pragma once

#include <functional>
#include <future>
#include <iostream>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
    bool terminated_{};     // status of the pool
    int min_workers_;       // min number of threads
    int max_workers_;       // max number of threads
    int max_qsize_;         // max size of the tasks queue
    int num_workers_{};     // number of active threads
    int free_workers_{};    // number of threads waiting on workers_cv_
    mutable std::mutex m_;  // mutex to protect all the shared variables
    std::vector<std::thread> workers_;
    std::vector<bool> active_workers_;
    std::queue<std::function<void()>> tasks_;
    std::condition_variable workers_cv_;
    std::condition_variable submit_cv_;

    void startWorker(int id) {
        workers_[id] = std::thread([this, id]() {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock ul(m_);

                    /* this check and the following operations must be performed atomically */
                    if (tasks_.empty() && num_workers_ > min_workers_) {
                        active_workers_[id] = false;
                        num_workers_--;
                        break;
                    }

                    /* this thread is now available */
                    free_workers_++;

                    /* if the pool is not terminated and if there are no tasks, sleep */
                    workers_cv_.wait(ul, [this]() { return (!tasks_.empty() || terminated_); });

                    /*
                     * we can return without caring about the shared variables
                     * since the pool is terminated
                     */
                    if (terminated_) break;

                    /* this thread is now busy */
                    free_workers_--;

                    /* extract a task and tell submit() there's a new free spot in the queue */
                    task = tasks_.front();
                    tasks_.pop();
                    submit_cv_.notify_one();
                }
                task();
            }
        });
        active_workers_[id] = true;
        num_workers_++;
    }

    void addWorker() {
        for (int id = 0; id < max_workers_; id++) {
            if (!active_workers_[id]) {
                if (workers_[id].joinable()) workers_[id].join();
                startWorker(id);
                return;
            }
        }
    }

public:
    ThreadPool(int min_workers, int max_workers, int max_qsize)
        : min_workers_{min_workers},
          max_workers_{max_workers},
          max_qsize_{max_qsize},
          workers_{std::vector<std::thread>(max_workers_)},
          active_workers_{std::vector<bool>(max_workers_, false)} {
        std::lock_guard lg(m_);
        for (int id = 0; id < min_workers_; id++) startWorker(id);
    }

    ~ThreadPool() {
        terminate();
        /* the joining of the threads must be permormed here, not in terminate() */
        for (auto& w : workers_) {
            if (w.joinable()) w.join();
        }
    }

    ThreadPool(const ThreadPool& src) = delete;

    ThreadPool& operator=(const ThreadPool& src) = delete;

    /*
     * By receiving the packaged_task by value, we ensure that its ownership
     * is immediately transfered to the thread-pool ('pt' will be move-constructed)
     * Since the packaged_task copy constructor is deleted, the only way
     * to call this function is by providing an rvalue-reference
     * By receveing the packaged_task by rvalue-reference instead, we make a single
     * copy-construnction instead of two
     */
    template <typename R>
    void submit(std::packaged_task<R()> pt) {
        std::unique_lock ul(m_);

        /*
         * the "terminated_" flag must be read with the mutex acuired,
         * to avoid "conflicts" with terminate()
         */
        if (terminated_) throw std::runtime_error("Thread pool is terminated");

        /* if the queue is "full", wait for a task to complete */
        submit_cv_.wait(ul, [this]() { return (tasks_.size() < max_qsize_); });

        /*
         * move the pt to the heap and wrap it in a lambda using a shared_ptr
         * the lambda will copy pt_ptr using its copy constructor
         */
        auto pt_ptr = std::make_shared<std::packaged_task<R()>>(std::move(pt));
        tasks_.push([pt_ptr]() { (*pt_ptr)(); });

        /* if possible, create a new thread */
        if (!free_workers_ && num_workers_ < max_workers_) addWorker();

        /* tell the workers there's a new job for them */
        workers_cv_.notify_one();
    }

    void terminate() {
        std::lock_guard lg(m_);
        if (terminated_) return;
        terminated_ = true;
        workers_cv_.notify_all();
    }

    void printStatus() const {
        std::lock_guard lg(m_);
        if (terminated_) {
            std::cout << "Thread-Pool is terminated" << std::endl;
        } else {
            std::cout << "=== Thread-Pool status ===" << std::endl;
            std::cout << "min workers: " << min_workers_ << std::endl;
            std::cout << "max workers: " << max_workers_ << std::endl;
            std::cout << "tasks queue size: " << tasks_.size() << std::endl;
            std::cout << "num workers: " << num_workers_ << std::endl;
            std::cout << "free workers: " << free_workers_ << std::endl;
            std::cout << "workers pool: ";
            for (auto b : active_workers_) std::cout << b << " ";
            std::cout << std::endl;
        }
    }
};