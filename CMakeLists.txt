cmake_minimum_required(VERSION 3.10)
project(PrefetchBench)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG -g -fno-omit-frame-pointer")
set(CMAKE_BUILD_TYPE Release)

include_directories(include include/structures/btree/stx-btree-0.9/include)

add_executable(prefetch_bench src/main.cpp)

find_package(Threads REQUIRED)
target_link_libraries(prefetch_bench Threads::Threads)