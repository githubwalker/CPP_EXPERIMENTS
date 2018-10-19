#include "sysutils.h"
#include <unistd.h>

#include <sstream>
#include <string>

#include <iostream>
#include <fstream>

#include <sys/types.h>
#include <sys/sysinfo.h>
#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>

namespace sysutils
{
    int64_t GetFreeRam()
    {
        struct sysinfo memInfo;

        sysinfo (&memInfo);

        return (memInfo.freeram + memInfo.freeswap) * memInfo.mem_unit;
    }

    // ^MemAvailable:\s*([0-9]+)\s*kB
    static const std::string memavailable_mask = "^MemAvailable:\\s*([0-9]+)\\s*kB";
    static const boost::regex re_available( memavailable_mask );

    int64_t GetMemAvailable()
    {
        std::ifstream meminfostm;
        meminfostm.open("/proc/meminfo");
        std::string line;

        while (std::getline(meminfostm, line))
        {
            boost::smatch matches;
            if (boost::regex_search(line, matches, re_available))
                return boost::lexical_cast<int64_t>(matches[1]) * 1024;
        }

        throw std::runtime_error("GetMemAvailable: MemAvailable is not found in /proc/meminfo");
    }
}

