#pragma once
#include <boost/asio/io_service.hpp>
#include <boost/thread.hpp>

// http://www.cplusplus.com/forum/beginner/219516/
// https://gist.github.com/autosquid/c5e5b3964524130d1c4d

namespace thread_pool
{
    class thread_pool
    {
        boost::asio::io_service ioService_;
        std::unique_ptr<boost::asio::io_service::work> pwork_;
        boost::thread_group threadpool_;
    public:
        thread_pool( std::size_t nthreads )
        :
          pwork_(std::make_unique<boost::asio::io_service::work>(ioService_))
        {
            for( std::size_t i = 0; i < nthreads; i ++)
                threadpool_.create_thread([this](){ioService_.run();});
        }

        void run_task( const boost::function<void ()>& task )
        {
            ioService_.post(task);
        }

        ~thread_pool()
        {
            pwork_.reset();
            threadpool_.join_all();
            ioService_.stop();
        }
    };
}
