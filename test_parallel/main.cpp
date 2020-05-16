#include <iostream>
#include "SRC/thread_pool.h"

#include <algorithm>
#include <random>
#include <future>
#include <chrono>
#include <queue>



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
    const std::size_t min_step = 100000000;
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

// try to make it cache friendly
template <typename _ittype>
int parallel_sum3(_ittype itbeg, _ittype itend)
{
    const std::size_t nBunchSize = 10000000; // 1024 * 1024;
    const auto thcount = boost::thread::hardware_concurrency() * 2;
    const std::size_t nItems = std::distance(itbeg, itend);

    std::vector<std::future<int>> futs;
    futs.reserve(thcount);

    for(std::size_t iThread = 0; iThread < thcount; ++iThread)
    {
        futs.emplace_back(std::async( [itbeg, itend, nItems, thcount, iThread]()
            {
                int sum = 0;

                for(std::size_t iLeft = iThread * nBunchSize; iLeft < nItems; iLeft += nBunchSize * thcount)
                {
                    std::size_t iRight = (std::min)(iLeft + nBunchSize, nItems);
                    sum += std::accumulate(itbeg+iLeft, itbeg+iRight, 0);
                }

                return sum;
            }));
    }

    return std::accumulate(futs.begin(), futs.end(), 0, [](auto a, auto& b) { return a + b.get(); });
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

    const std::size_t nrepeats = 100;
    const std::size_t nitems = 4000000000;

    std::vector<std::int16_t> vc;
    vc.reserve(nitems);

    std::cout << "allocating " << nitems << " items " << std::endl;

    for(std::size_t i = 0; i < nitems; i ++)
        vc.emplace_back((std::int16_t)dist(rng));

    std::cout << "Starting measurements" << std::endl;

    int val = 0;

    // single thread
    MeasureScope ms0;
    for(std::size_t i=0; i < nrepeats; i++)
        val = std::accumulate(vc.begin(), vc.end(), 0);

    std::cout << "Time spent in single thread (micros): " << ms0.GetMicros() << ", result is: " << val << std::endl;

    // parallel_sum1
    MeasureScope ms1;
    for(std::size_t i=0; i < nrepeats; i++)
        val = parallel_sum(vc.begin(), vc.end());

    std::cout << "Time spent in parallel (micros): " << ms1.GetMicros() << ", result is: " << val << std::endl;

    // parallel_sum2
    thread_pool::thread_pool tp(boost::thread::hardware_concurrency() * 2);
    MeasureScope ms2;
    for(std::size_t i=0; i < nrepeats; i++)
        val = parallel_sum2(vc.begin(), vc.end(), tp);
       
    std::cout << "Time spent in thread pool (micros): " << ms2.GetMicros() << ", result is: " << val << std::endl;

    // parallel_sum3
    MeasureScope ms3;
    for(std::size_t i=0; i < nrepeats; i++)
        val = parallel_sum3(vc.begin(), vc.end());

    std::cout << "Time spent in cache-friendly parallel sum (micros): " << ms3.GetMicros() << ", result is: " << val << std::endl;
}


int main(int argc, char** argv)
{
    std::cout << "size of std::size_t: " << sizeof(std::size_t) << std::endl;
    promisses_and_futures();
    return 0;
}
