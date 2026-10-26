#include "Parallel.hpp"
#ifdef _MSC_VER
#include <algorithm>
#include <execution>
#include <numeric>
#include <vector>
namespace pc {
void parallelFor(std::size_t count,const std::function<void(std::size_t)>& task) {
    std::vector<std::size_t> indices(count);
    std::iota(indices.begin(),indices.end(),std::size_t(0));
    std::for_each(std::execution::par,indices.begin(),indices.end(),[&](std::size_t i) {task(i);});
}
}
#else
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <thread>
#include <vector>
namespace pc {
namespace {
// One job at a time: the caller and the workers take its indices in turn.
class Pool {
public:
    Pool() {
        const unsigned cores=std::max(1u,std::thread::hardware_concurrency());
        for(unsigned i=1;i<std::min(cores,8u);++i)std::thread([this] {work();}).detach();
    }
    void run(std::size_t count,const std::function<void(std::size_t)>& task) {
        std::lock_guard serial(callers_); // jobs from several threads take turns
        Job job{&task,count};
        {std::lock_guard lock(mutex_);job_=&job;++generation_;}
        wake_.notify_all();
        take(job);
        {
            // The job lives on this stack: wait until no worker holds it.
            std::unique_lock lock(mutex_);
            done_.wait(lock,[&] {return job.finished==count&&users_==0;});
            job_=nullptr;
        }
        if(job.error)std::rethrow_exception(job.error);
    }
private:
    struct Job {
        const std::function<void(std::size_t)>* task;
        std::size_t count;
        std::atomic<std::size_t> next{0},finished{0};
        std::exception_ptr error;
        std::mutex errorMutex;
    };
    std::mutex callers_,mutex_;
    std::condition_variable wake_,done_;
    Job* job_=nullptr;
    unsigned long long generation_=0;
    unsigned users_=0;
    void take(Job& job) {
        for(std::size_t i;(i=job.next.fetch_add(1))<job.count;) {
            try {(*job.task)(i);}
            catch(...) {std::lock_guard lock(job.errorMutex);if(!job.error)job.error=std::current_exception();}
            if(job.finished.fetch_add(1)+1==job.count) {std::lock_guard lock(mutex_);done_.notify_all();}
        }
    }
    void work() {
        unsigned long long seen=0;
        for(;;) {
            Job* job;
            {
                std::unique_lock lock(mutex_);
                wake_.wait(lock,[&] {return job_&&generation_!=seen;});
                seen=generation_;job=job_;++users_;
            }
            take(*job);
            {std::lock_guard lock(mutex_);--users_;}
            done_.notify_all();
        }
    }
};
}
void parallelFor(std::size_t count,const std::function<void(std::size_t)>& task) {
    if(count==0)return;
    if(count==1) {task(0);return;}
    // Never destroyed: the workers wait for jobs until the process ends.
    static Pool* pool=new Pool;
    pool->run(count,task);
}
}
#endif
