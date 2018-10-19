cmake_minimum_required(VERSION 3.5)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(sorthugenumbers)

#list(APPEND sorthugenumbers SOURCE_DIR ./SRC)
#TARGET_SOURCES(sorthugenumbers PRIVATE "${CMAKE_CURRENT_LIST_DIR}/SRC/sorter.cpp)

find_package(Boost COMPONENTS thread REQUIRED)
find_package(Boost COMPONENTS filesystem REQUIRED)
find_PACKAGE(Boost COMPONENTS program_options REQUIRED )
find_package(Boost COMPONENTS regex REQUIRED)


file(GLOB_RECURSE my_srcs
       ${PROJECT_SOURCE_DIR}/SRC/*.cpp
       ${PROJECT_SOURCE_DIR}/main.cpp
)


file(GLOB my_srcs SRC/*.cpp ./main.cpp)
file(GLOB my_headers SRC/*.h)

include_directories(SRC)

#include_directories(.);

#set(CMAKE_INCLUDE_PATH /usr/local/lib)
#include_directories(SRC)

#target_include_directories(sorthugenumbers PRIVATE ${YOUR_DIRECTORY})

#set(sorthugenumbers_srcs ./SRC/*, ./main.cpp)
# "main.cpp" "./SRC/sorter.cpp"

add_executable(${PROJECT_NAME} ${my_srcs} ${my_headers})

target_link_libraries(
    ${PROJECT_NAME}
    Boost::thread
    Boost::filesystem
    Boost::program_options
    Boost::regex
  )
