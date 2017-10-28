cmake_minimum_required(VERSION 3.7)
project(ASM)

set(CMAKE_CXX_STANDARD 14)
SET( CMAKE_CXX_FLAGS "-m32 -mssse3")
include(FindOpenMP)
if(OPENMP_FOUND)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()

add_executable(ASM1 asm_copy.cpp)
add_executable(ASM2 word_counter.cpp)