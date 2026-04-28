cmake_minimum_required(VERSION 3.10)
project(PrefetchBench)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG -flto")
set(CMAKE_BUILD_TYPE Release)

# 包含头文件目录
include_directories(
    include 
    include/structures/btree/stx-btree-0.9/include
)

# 可执行文件：原始版本（slot size 测试）
add_executable(prefetch_bench src/main.cpp)

# 链接线程库
find_package(Threads REQUIRED)
target_link_libraries(prefetch_bench Threads::Threads)

# 编译选项
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(prefetch_bench PRIVATE -Wall -Wextra)
endif()

# 打印配置信息
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ flags: ${CMAKE_CXX_FLAGS_RELEASE}")
