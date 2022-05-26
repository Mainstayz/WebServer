/*
 * @Author       : mark
 * @Date         : 2020-06-15
 * @copyleft Apache 2.0
 */ 

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <functional>
class ThreadPool {
public:
    explicit ThreadPool(size_t threadCount = 8): pool_(std::make_shared<Pool>()) {
            assert(threadCount > 0);
            for(size_t i = 0; i < threadCount; i++) {
                // 创建多个线程
                std::thread([pool = pool_] {
                    std::unique_lock<std::mutex> locker(pool->mtx);
                    while(true) {
                        // 抢任务
                        if(!pool->tasks.empty()) {
                            // 获取任务
                            auto task = std::move(pool->tasks.front());
                            pool->tasks.pop();
                            // 解锁
                            locker.unlock();
                            // 执行任务
                            task();
                            // 锁住
                            locker.lock();
                        } 
                        // 如果这个池子关闭了，结束当前线程
                        else if(pool->isClosed) break;
                        else pool->cond.wait(locker); // 阻塞当前线程
                    }
                }).detach();
            }
    }

    ThreadPool() = default;

    ThreadPool(ThreadPool&&) = default;
    
    ~ThreadPool() {
        if(static_cast<bool>(pool_)) {
            {
                // 所有人都操作这个池子，所以得加锁
                std::lock_guard<std::mutex> locker(pool_->mtx);
                pool_->isClosed = true;
            }
            // 通知所有线程
            pool_->cond.notify_all();
        }
    }

    template<class F>
    void AddTask(F&& task) {
        {
            // 锁住你这自此
            std::lock_guard<std::mutex> locker(pool_->mtx);
            // https://www.jianshu.com/p/298cab007497
            pool_->tasks.emplace(std::forward<F>(task));
        }
        // 通知一个线程强
        pool_->cond.notify_one();
    }

private:
    struct Pool {
        std::mutex mtx;
        std::condition_variable cond;
        bool isClosed;
        std::queue<std::function<void()>> tasks;
    };
    std::shared_ptr<Pool> pool_;
};


#endif //THREADPOOL_H