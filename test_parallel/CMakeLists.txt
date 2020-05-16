cmake_minimum_required(VERSION 3.5)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(test_parallel)

find_package(Boost COMPONENTS thread REQUIRED)

file(GLOB_RECURSE my_srcs
       ${PROJECT_SOURCE_DIR}/main.cpp
)


file(GLOB my_srcs SRC/*.cpp ./main.cpp)
file(GLOB my_headers SRC/*.h)

include_directories(SRC)

add_executable(${PROJECT_NAME} ${my_srcs} ${my_headers})

target_link_libraries(
    ${PROJECT_NAME}
    Boost::thread
  )
