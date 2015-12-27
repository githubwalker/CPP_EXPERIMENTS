/* 
 * File:   main.cpp
 * Author: andrew
 *
 * Created on 26 декабря 2015 г., 2:39
 */

#include <cstdlib>
#include <iostream>
#include <string>
#include <exception>
#include <stdexcept>
#include <condition_variable>
#include <thread>
#include <unordered_set>
#include <vector>
#include <future>
#include <chrono>
#include <set>
#include <boost/program_options.hpp>
#include "concurrent_queue.h"
#include <boost/algorithm/string/case_conv.hpp>




class ThreadLatch
{
private:
    struct auto_thids
    {
        std::set<std::thread::id>& ids_;
        
        auto_thids( std::set<std::thread::id>& ids )
        :
        ids_(ids)
        {
            ids_.insert( std::this_thread::get_id() );
        }
        
        ~auto_thids()
        {
            ids_.erase( std::this_thread::get_id() );
        }
        
        std::size_t size() const { return ids_.size(); }
    };
private:
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<int> flag_;
    
    std::set<std::thread::id> threads_;
    std::size_t expected_thread_count_;
public:
    ThreadLatch( const ThreadLatch& other ) = delete;
    ThreadLatch( const ThreadLatch&& other ) = delete;
    const ThreadLatch& operator = ( const ThreadLatch& other ) = delete;
    ThreadLatch( std::size_t expected_thread_count ) 
    { 
        expected_thread_count_ = expected_thread_count;
        flag_ = 0; 
    }
    
    void wait()
    {
        if ( flag_.load() > 0 )
            return;
        
        {
            std::unique_lock<std::mutex> lock(mutex_);
            
            auto_thids auto_threads_accounting(threads_);
            if ( auto_threads_accounting.size() >= expected_thread_count_ )
            {
                start();
            }
            
            while ( flag_.load() == 0 )
                cv_.wait(lock);
        }
    }
    
    void start()
    {
        flag_.store(1);
        cv_.notify_all();
    }
};

void thrd_pusher(
    std::shared_ptr< CONC::conc_queue< int64_t > > queue,
    std::shared_ptr< ThreadLatch > latch,
    std::shared_ptr< std::atomic< int64_t > > counter,
    int64_t maxcount
)
{
    latch->wait();
    for( int64_t i = 0; i < maxcount; i ++ )
        queue->push( (*counter)++ );
}


typedef std::shared_ptr< std::unordered_set< int64_t > > thrd_popper_result;

void thrd_popper(
    std::shared_ptr< CONC::conc_queue< int64_t > > queue,
    std::shared_ptr< ThreadLatch > latch,
    thrd_popper_result results,
    bool bCheckOutput,
    int64_t maxcount
)
{
    latch->wait();
    
    if ( bCheckOutput )
    {
        int64_t val = 0;
        for( int64_t i = 0; i < maxcount; i ++ )
            if ( queue->pop( &val ) )
                results->insert( val );
    }
    else
    {
        for( int64_t i = 0; i < maxcount; i ++ )
            queue->pop();
    }
}

struct parsed_params
{
    int nPushThreads;
    int nPopThreads;
    int nIterations;
    bool bCheckOutput;
    bool bHelp;
    
    parsed_params()
    {
        nPushThreads = 0;
        nPopThreads = 0;
        nIterations = 0;
        bCheckOutput = false;
        bHelp = true;
    }

    parsed_params( int nPushThreads, int nPopThreads, int nIterations, bool bCheckOutput )
    {
        this->nPushThreads = nPushThreads;
        this->nPopThreads = nPopThreads;
        this->nIterations = nIterations;
        this->bCheckOutput = bCheckOutput;
        this->bHelp = false;
    }

    bool isHelp() const 
    {
        return this->bHelp;
    } 
};

// http://marknelson.us/2012/05/23/c11-threading-made-easy/

void testConcurrentQueue( parsed_params& pp )
{
    std::shared_ptr< CONC::conc_queue< int64_t > > queue = std::shared_ptr< CONC::conc_queue< int64_t > >( new CONC::conc_queue< int64_t > );
    std::shared_ptr<ThreadLatch> latch = std::shared_ptr<ThreadLatch>( new ThreadLatch( pp.nPopThreads + pp.nPushThreads ) );
    auto atomic_counter = std::shared_ptr< std::atomic< int64_t > >( new std::atomic< int64_t >(0) );
    std::vector< std::shared_ptr< std::thread > > pushers;
    std::vector< std::pair< std::shared_ptr<std::thread>, thrd_popper_result > > poppers;
    std::set< int64_t > ts;
    
    
    std::cerr << "[INFO] Number push threads to run :" << pp.nPushThreads << std::endl;
    std::cerr << "[INFO] Number pop threads to run :" << pp.nPopThreads << std::endl;
    std::cerr << "[INFO] Number interations in each thread to go :" << pp.nIterations << std::endl;
    std::cerr << "-------------------------------------" << std::endl ;
    std::cerr << std::endl;

    
    for( int i = 0; i < pp.nPopThreads; i ++ )
    {
        // poppers.push_back( thrd_n_results( queue, latch, atomic_counter, pp.bCheckOutput, pp.nIterations ) );
        thrd_popper_result output_results = std::shared_ptr< std::unordered_set< int64_t > >( new std::unordered_set< int64_t >() );
        std::shared_ptr<std::thread> thr = std::shared_ptr<std::thread>( 
                new std::thread( 
                    [queue,latch,pp,output_results] () -> void
                    { thrd_popper(queue,latch,output_results,pp.bCheckOutput,pp.nIterations); }
                    ) 
                );
        poppers.push_back( std::make_pair( thr, output_results ) );
    }
    
    for ( int i = 0; i < pp.nPushThreads; i ++ )
    {
        pushers.push_back( 
            std::shared_ptr< std::thread >( new std::thread( 
                [queue,latch,atomic_counter,&pp] () -> void 
                { thrd_pusher( queue, latch, atomic_counter, pp.nIterations ); } 
                ) 
            ) 
        );
    }
    
    // std::this_thread::sleep_for( std::chrono::microseconds(100) );
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // latch->start();
    
    for ( auto push_thr : pushers )
        push_thr->join();
    
    for ( auto pop_thr : poppers )
        pop_thr.first->join();
    
    auto endTime = std::chrono::high_resolution_clock::now();
    
    
    std::cerr << "[INFO] " << queue->size() << " items left in queue" << std::endl;
    std::cerr << "[INFO] " << pp.nPushThreads * pp.nIterations << " total numbers expected to be handled" << std::endl;
    std::cerr << "[INFO] push collision counter :" << queue->getPushCollisionCount() << std::endl;
    std::cerr << "[INFO] pop collision counter :" << queue->getPopCollisionCount() << std::endl;

    double nPushCollisionsPerc = 100.0 *  
            ((double)queue->getPushCollisionCount()) / ((double)pp.nPushThreads * (double)pp.nIterations);

    double nPopCollisionsPerc = 100.0 * 
            ((double)queue->getPopCollisionCount()) / ((double)pp.nPopThreads * (double)pp.nIterations);

    std::cerr << "[INFO] push collision percentage :" << nPushCollisionsPerc << std::endl;
    std::cerr << "[INFO] pop collision percentage :" << nPopCollisionsPerc << std::endl;


    if ( pp.bCheckOutput )
    {
        int64_t icur;
        while( queue->pop(&icur) )
        {
            if ( ts.find(icur) != ts.end() )
                std::cerr << "[FAIL] : " << icur << "already accounted" << std::endl;
            else
                ts.insert(icur);
        }

        for ( auto pop_thr : poppers )
        {
            for ( auto icur : *pop_thr.second )
            {
                if ( ts.find(icur) != ts.end() )
                    std::cerr << "[FAIL] : " << icur << "already accounted" << std::endl;
                else
                    ts.insert(icur);
            }
        }

        if ( ts.size() != pp.nPushThreads * pp.nIterations ) 
        {
            std::cerr << "[FAIL] : got size (" << ts.size() << ", but expected " << pp.nPushThreads * pp.nIterations << ")" << std::endl;
        }

        int64_t prevint = -1;
        for( auto icur : ts )
        {
            if ( prevint + 1 != icur )
            {
                std::cerr << "[FAIL] : " << prevint + 1 << " missing" << std::endl;
            }

            prevint = icur;
        }
        
        std::cerr << "Done checking" << std::endl;
    }
    else
    {
        std::cerr << "queue output check will not be performed " << std::endl;
    }
    
    std::cerr << "time spent : " 
            << std::chrono::duration_cast<std::chrono::milliseconds>( endTime - startTime ).count() 
            << " milliseconds" << std::endl;
    std::cerr << "Done running" << std::endl;
}

bool recognizeBool( const std::string str ) 
{
    std::string strLower = boost::to_lower_copy( str );
    
    if ( strLower == "yes" || strLower == "y" || strLower == "true" )
        return true;
    
    if ( strLower == "no" || strLower == "n" || strLower == "false" )
        return false;
    
    throw std::runtime_error( "failed to parse bool flag option" );
}

parsed_params parseArgs( int argc, char** argv ) 
{
    namespace po = boost::program_options;
    po::options_description mainOptions("Main options");
    po::options_description helpOptions("Help options");
    
    int npush_threads = 0;
    int npop_threads = 0;
    int iterations = 0;
    std::string strCheckOutput = "true";
    
    mainOptions.add_options()
        ("push-threads,u", po::value<int>(&npush_threads)->required(), "number of push threads to run" )
        ("pop-threads,o", po::value<int>(&npop_threads)->required(), "number of pop threads to run" )
        ("iterations,i", po::value<int>(&iterations)->required(), "number of iteration to run in each push/pop thread" )
        ("check-output,c", po::value<std::string>(&strCheckOutput), "check output" );
    
    helpOptions.add_options()
        ("help,h", "Show help");
    
    po::variables_map vm;
    
    try
    {
        po::store( po::parse_command_line(argc, argv, mainOptions ), vm );
        po::notify(vm);
    }
    catch ( std::exception& )
    {
        mainOptions.print( std::cerr );
        throw;
    }
    
    if (vm.count("help"))
    {
        mainOptions.print( std::cerr );
        return parsed_params();
    }
    
    bool bCheckOutput = recognizeBool( strCheckOutput );
    
    return parsed_params(  
        npush_threads,
        npop_threads,
        iterations,
        bCheckOutput
    );
}

int main(int argc, char** argv)
{
    // testq();
    
    parsed_params pp;
    
    try
    {
        pp = parseArgs( argc, argv );
    }
    catch ( std::exception& ex ) 
    {
        std::cerr << "failed parsing parameter: " << std::endl;
        std::cerr << ex.what() << std::endl;
        return 1;
    }
    
    if ( pp.isHelp() )
    {
        return 1;
    }
    
    testConcurrentQueue( pp );

    return 0;
}

