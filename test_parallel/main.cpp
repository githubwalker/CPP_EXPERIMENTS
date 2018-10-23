#include <iostream>
#include "SRC/thread_pool.h"

#include <algorithm>
#include <random>
#include <iostream>
#include <functional>
#include <future>
#include <thread>
#include <chrono>

#include <random>
#include <queue>

#include "thread_pool.h"


template <typename _ittype>
int parallel_sum(_ittype itbeg, _ittype itend)
{
    auto dist = itend - itbeg;
    if (dist < 10000000)
        return std::accumulate(itbeg, itend, 0);
    auto mid = dist >> 1;

    auto f1 = std::async( [itbeg, itend, mid](){ return parallel_sum(itbeg, itbeg + mid); } );
    auto val2 = parallel_sum(itbeg + mid, itend);
    return f1.get() + val2;
}

template <typename _ittype>
int parallel_sum2(_ittype itbeg, _ittype itend, thread_pool::thread_pool& tp)
{
    const std::size_t min_step = 10000000;
    auto dist = itend - itbeg;
    if (dist < min_step)
        return std::accumulate(itbeg, itend, 0);

    std::queue<std::shared_future<int>> futures;

    int total = 0;

    for(std::size_t i = 0; i < dist; i += min_step)
    {
        std::size_t right_bound = std::min(i + min_step, (std::size_t)dist);
        futures.push( tp.run_future<int>([itbeg, i, right_bound]() {return std::accumulate(itbeg+i, itbeg+right_bound, 0);}));

        while (!futures.empty() && futures.size() >= tp.GetThreadsCount())
        {
            total += futures.front().get();
            futures.pop();
        }
    }

    while (!futures.empty())
    {
        total += futures.front().get();
        futures.pop();
    }

    return total;
}

struct MeasureScope
{
    std::chrono::system_clock::time_point beg_;

    MeasureScope()
        : beg_ (std::chrono::system_clock::now())
    {}

    std::size_t GetMicros() const
    {
        return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - beg_).count();
    }
};

void promisses_and_futures()
{
    std::mt19937 rng;
    rng.seed(std::random_device()());
    std::uniform_int_distribution<std::mt19937::result_type> dist(1,100000000);

    const std::size_t nitems = 4000000000;

    std::vector<std::int16_t> vc;
    vc.reserve(nitems);

    for(std::size_t i = 0; i < nitems; i ++)
        vc.emplace_back((std::int16_t)dist(rng));

    std::cout << "Starting meqsurements" << std::endl;

    MeasureScope ms0;
    auto val0 = parallel_sum(vc.begin(), vc.end());
    std::cout << "Time spent in parallel (micros): " << ms0.GetMicros() << ", result is: " << val0 << std::endl;

    MeasureScope ms1;
    auto val1 = std::accumulate(vc.begin(), vc.end(), 0);
    std::cout << "Time spent in single thread (micros): " << ms1.GetMicros() << ", result is: " << val1 << std::endl;

    auto thcount = boost::thread::hardware_concurrency() * 4;
    std::cout << "thcount = " << thcount << std::endl;

    thread_pool::thread_pool tp(thcount);

    MeasureScope ms2;
    auto val2 = parallel_sum2(vc.begin(), vc.end(), tp);
    std::cout << "Time spent in thread pool (micros): " << ms2.GetMicros() << ", result is: " << val2 << std::endl;

    return;
}


int main(int argc, char** argv)
{
    promisses_and_futures();
    return 0;
}
