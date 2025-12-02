cmake_minimum_required(VERSION 3.10)
project(PrefetchBench)

#使用C++17标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 性能优化选项 
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG -g -fno-omit-frame-pointer")
set(CMAKE_BUILD_TYPE Release)

include_directories(include)

add_executable(prefetch_bench src/main.cpp)