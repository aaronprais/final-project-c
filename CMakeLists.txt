cmake_minimum_required(VERSION 3.31)
project(final_project_c C)

set(CMAKE_C_STANDARD 90)

add_executable(final_project_c main.c
        table.c
        table.h
        ordering_into_table.c
        ordering_into_table.h
        labels.c
        labels.h
        util.c
        util.h
        pre_assembly.c
        pre_assembly.h
        file_formating.c
        file_formating.h
        binary_table_parsing.c
        binary_table_parsing.h
)
