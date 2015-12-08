//============================================================================
// Name        : testBoost.cpp
// Author      : andrew
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <boost/shared_ptr.hpp>
#include <memory>
#include <iostream>
#include <boost/asio.hpp>
#include <boost/thread/mutex.hpp>
#include <vector>
#include <boost/enable_shared_from_this.hpp>
#include <inttypes.h>
#include <set>
#include <map>
#include <string>
#include <boost/thread.hpp>
#include <malloc.h>


// www.sql.ru/forum/1071761/sokety-i-biblioteka-boost-asio
// http://www.boost.org/doc/libs/1_49_0/doc/html/boost_asio/example/chat/chat_server.cpp


const int portname = 4444;

typedef boost::shared_ptr< boost::asio::ip::tcp::socket > socket_ptr;
typedef boost::shared_ptr< boost::asio::ip::tcp::endpoint > endpoint_ptr;
typedef boost::shared_ptr< boost::asio::io_service > service_ptr;
typedef boost::shared_ptr< boost::asio::ip::tcp::acceptor > acceptor_ptr;

struct CStatisctics
{

    struct statItem
    {
        uint64_t m_number;
        // std::set< std::string > m_fnames;
        std::set< int > m_fnames;
        int m_count;

        // statItem(uint64_t number, const std::string& fname)
        statItem(uint64_t number)
        : m_number(number), m_count(0)
        {
        }

        // void oneMoreEntry(const std::string& fileName)
        void oneMoreEntry(int fileName)
        {
            m_fnames.insert(fileName);
            m_count++;
        }
    };
    
    class Dictionary
    {
    protected:
        int m_nCurIndex;
        std::map< std::string, int > m_str2int;
        // std::map< int, std::string > m_int2str;
        std::map< int, std::map< std::string, int >::iterator > m_int2str;
    public:
        Dictionary()
        {
            m_nCurIndex = 0;
        }
        
        int getIndex4str( const std::string& strng )
        {
            auto itFound = m_str2int.find( strng );
            if (itFound != m_str2int.end())
                return itFound->second;
            
            m_nCurIndex ++;
            auto insertResult_s2i = m_str2int.insert( std::pair<std::string, int>( strng, m_nCurIndex ) );
            assert( insertResult_s2i.second );
            auto insertResult_i2s = m_int2str.insert( std::pair<int,std::map< std::string, int >::iterator>( m_nCurIndex, insertResult_s2i.first ) );
            assert( insertResult_i2s.second );
            
            return m_nCurIndex;
        }

        bool getStr4Index( int index, std::string& strFound ) const 
        {
            auto itFound = m_int2str.find( index );
            if ( itFound != m_int2str.end() )
            {
                strFound = itFound->second->first;
                return true;
            }
            
            return false;
        }
        
        std::string getStr4Index( int index ) const 
        {
            auto itFound = m_int2str.find( index );
            if ( itFound == m_int2str.end() )
                throw std::exception();
            
            return itFound->second->first;
        }
        
        void swap( Dictionary& otherDict )
        {
            m_str2int.swap(otherDict.m_str2int);
            m_int2str.swap(otherDict.m_int2str);
            decltype(m_nCurIndex) tmp = m_nCurIndex; 
            m_nCurIndex = otherDict.m_nCurIndex; 
            otherDict.m_nCurIndex = tmp;
        }
        
        std::size_t getNumOfItems() const { return this->m_str2int.size(); }  
    };

public:
    boost::mutex m_mutex;
    std::map< uint64_t, statItem > m_items;
    Dictionary m_dict;
public:
    
    CStatisctics()
    {
    }
    
    ~CStatisctics()
    {
        std::size_t nItems = m_items.size();
        m_items.clear();
        return;
    }
    
    int getNumberOfFiles() const 
    {
        return m_dict.getNumOfItems();
    }
    
    int getIndex4str( const std::string& str )
    {
        return m_dict.getIndex4str( str );
    }
    
    std::string getStr4Index( int fname_index ) const 
    {
        return m_dict.getStr4Index( fname_index );
    }

    void appendNumber(const std::string& fname, uint64_t number)
    {
        boost::unique_lock<boost::mutex> lock(m_mutex);
        
        int fnameindex = m_dict.getIndex4str(fname);
        
        auto itfound = m_items.find(number);
        if (itfound == m_items.end())
            itfound = m_items.insert(std::make_pair(number, statItem(number))).first;

        itfound->second.oneMoreEntry(fnameindex);
    }
    
    void appendNumber(int fname_index, uint64_t number)
    {
        boost::unique_lock<boost::mutex> lock(m_mutex);
        
        auto itfound = m_items.find(number);
        if (itfound == m_items.end())
            itfound = m_items.insert(std::make_pair(number, statItem(number))).first;

        itfound->second.oneMoreEntry(fname_index);
    }
    
    void protectedSwap( CStatisctics& other )
    {
        boost::unique_lock<boost::mutex> lock(m_mutex);
        boost::unique_lock<boost::mutex> lockOther(other.m_mutex);
        
        m_items.swap(other.m_items);
        m_dict.swap(other.m_dict);
    }
};

struct NumberBuffer
{
    char _buffer[sizeof (uint64_t)];
    int _pos;

    NumberBuffer()
    : _pos(0)
    {
    }

    bool appendBuffer(const char * pinBuffer, int& inpos, int nItems, uint64_t& formedNumber)
    {
        while (_pos < (int) sizeof (uint64_t) && inpos < nItems)
            _buffer[_pos++] = pinBuffer[inpos++];

        bool bComplete = false;

        if (_pos == sizeof (uint64_t)) {
            bComplete = true;
            formedNumber = *(const uint64_t*) _buffer;
            _pos = 0;
        }

        return bComplete;
    }

    bool getRemainings(uint64_t& formedNumber)
    {
        bool bSomethingFound = _pos > 0;
        if (_pos > 0) 
        {
            uint64_t rv = 0;
            for (int i = 0; i < _pos; i++)
                ((char *) &rv)[ i ] = _buffer[i];

            formedNumber = rv;
            _pos = 0;
        }

        return bSomethingFound;
    }
};

// format number in way its expected
static std::string formatInt64( uint64_t val )
{
    std::stringstream stm;
    const unsigned char * pVal = (const unsigned char*)&val;
    for(int i = 0 ; i < 4; i ++)
        stm << std::hex << std::setfill('0') << std::setw(2) << (unsigned short)pVal[i];
    stm << " ";
    for(int i = 4 ; i < 8; i ++)
        stm << std::hex << std::setfill('0') << std::setw(2) << (unsigned short)pVal[i];

    return stm.str();
}

std::string getCurrentTime()
{
    time_t rawtime;
    tm * ti;

    time (&rawtime);
    ti = localtime (&rawtime);
    std::stringstream stm;
    
    stm 
        << ti->tm_year << "-" << ti->tm_mon + 1 << "-" << ti->tm_mday 
        << " "
        << ti->tm_hour << ":" << ti->tm_min << ":"  << ti->tm_sec;
    
    return stm.str();
}

static void printStats( CStatisctics& stats )
{
    std::cout << "Starting dump at :" << getCurrentTime() << "number of items found :" << stats.m_items.size() << " in " << stats.getNumberOfFiles() << " files" << std::endl;
    std::cout << "===============================" << std::endl;
    
    for( auto item : stats.m_items )
    {
        std::cout 
            << formatInt64 ( item.first )
            << "    " << std::setw(0) << std::dec << item.second.m_count ;

        std::cout << "  [";

        bool bFirstFileName = true;
        for ( auto fnameIndex : item.second.m_fnames )
        {
            if (bFirstFileName)
                bFirstFileName = false;
            else
                std::cout << " ";

            std::cout << stats.getStr4Index( fnameIndex );
        }

        std::cout << "]" << std::endl;
    }
    
    std::cout << "Ending dump at :" << getCurrentTime() << std::endl;
    std::cout << "===============================" << std::endl;
}

class Csession : public boost::enable_shared_from_this<Csession>
{
protected:
    socket_ptr m_sock;
    service_ptr m_svc;

    enum
    {
        max_length = 1024
    };
    char data_[max_length];
    boost::shared_ptr< CStatisctics > m_stat;
    NumberBuffer m_numberBuffer;
    std::stringstream m_stmFileName;
    bool m_bFileNameLoaded;
    
public:

    Csession(service_ptr svc, socket_ptr sock, boost::shared_ptr< CStatisctics > stat)
    :
    m_sock(sock)
    , m_svc(svc)
    , m_stat(stat)
    , m_bFileNameLoaded(false)
    {
    }

    void start()
    {
        do_read();
    }

    void process_data(char * pData, std::size_t length)
    {
        int iPos = 0;
            
        if (!m_bFileNameLoaded)
        {
            for ( ; iPos < (int)length && pData[iPos] != 0; iPos ++ )
                m_stmFileName << pData[iPos];

            if ( iPos < (int)length && pData[iPos] == 0 )
            {
                iPos ++;
                m_bFileNameLoaded = true;
            }
        }
        
        if (m_bFileNameLoaded)
        {
            // dont wait completion for now
            if ( m_stmFileName.str() == "//printstat" )
            {
                boost::shared_ptr<CStatisctics> tmp = boost::shared_ptr<CStatisctics>( new CStatisctics );
                m_stat->protectedSwap( *tmp );
                printStats( *tmp );
            }
            
            ::malloc_trim(0);
        }

        if (m_bFileNameLoaded)
        {
            uint64_t number = 0;
            // TODO: filename
            while (m_numberBuffer.appendBuffer(pData, iPos, length, number))
                m_stat->appendNumber(m_stmFileName.str(), number);
        }
    }

    void process_remainings()
    {
        if (m_bFileNameLoaded)
        {
            uint64_t number = 0;
            // TODO: filename
            if (m_numberBuffer.getRemainings(number))
                m_stat->appendNumber(m_stmFileName.str(), number);
        }
    }

    void do_read()
    {
        auto self(shared_from_this());
        m_sock->async_read_some(
                boost::asio::buffer(data_, max_length),
                [this, self] (boost::system::error_code ec, std::size_t length) {
                    if (!ec)
                    {
                        process_data(data_, length);
                        do_read();
                    }
                    else
                    {
                        process_remainings();
                    }
                }
        );
    }
};

class CServer
{
protected:
    service_ptr m_service;
    endpoint_ptr m_ep;
    acceptor_ptr m_acceptor;
    boost::shared_ptr< CStatisctics > m_stat;
    // boost::mutex m_mutex;
    // std::vector< boost::shared_ptr<Csession> > m_sessions;
public:

    CServer(service_ptr svc, endpoint_ptr ep)
    :
    m_service(svc)
    , m_ep(ep)
    , m_acceptor(new boost::asio::ip::tcp::acceptor(*svc, *ep))
    , m_stat(new CStatisctics)
    {
    }

    void start()
    {
        socket_ptr new_sock = socket_ptr(new boost::asio::ip::tcp::socket(*m_service));
        m_acceptor->async_accept(
                *new_sock,
                [this, new_sock](boost::system::error_code ec) {
                    if (!ec) {
                        auto shared_session = boost::shared_ptr<Csession>(new Csession(m_service, new_sock, m_stat));
                        shared_session->start();
                    }

                    start();
                }
        );
    }

    ~CServer()
    {
    }
};

int main()
{

    service_ptr svc = service_ptr(new boost::asio::io_service);
    endpoint_ptr ep(new boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), portname));

    CServer svr(svc, ep);
    svr.start();

    svc->run();

    return 0;
}

