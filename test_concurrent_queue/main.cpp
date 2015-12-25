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
#include "concurrent_queue.h"




void testq()
{
    CONC::conc_queue<int> lftest;
    
    lftest.push(1);
    lftest.push(2);
    lftest.push(3);
    
    int val = 0;
    while ( lftest.pop(&val) )
    {
        std::cout << "got number " << val << std::endl;
    }
    
    CONC::conc_queue<std::string> lfs;
    
    lfs.push("123");
    
    return;
}

int main(int argc, char** argv)
{
    testq();

    return 0;
}

