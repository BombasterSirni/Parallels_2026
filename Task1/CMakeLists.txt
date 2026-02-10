cmake_minimum_required(VERSION 3.10)
project(SinSum)

option(USE_DOUBLE "Use double precision for the array (default if float)" OFF)

add_executable(sin_sum array_fill.cpp)

if(USE_DOUBLE)
    target_compile_definitions(sin_sum PRIVATE USE_DOUBLE)
endif()

target_link_libraries(sin_sum PRIVATE m)