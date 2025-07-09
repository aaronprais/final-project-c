cmake_minimum_required(VERSION 3.31)
project(final_project_c C)

set(CMAKE_C_STANDARD 90)

add_executable(final_project_c main.c logger.c pre_assembly.c
        pre_assembly.h
)
