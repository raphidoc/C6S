cmake_minimum_required(VERSION 3.27)
project(C6S)

set(CMAKE_CXX_STANDARD 17)

include_directories(/usr/include)

add_executable(C6S
        #main.cpp
        #main_slow.cpp
        #main_FIFO.cpp
        main_FIFO_test.cpp
        #main_threads_slow.cpp
)

# Link jsoncpp library
target_link_libraries(C6S jsoncpp netcdf)