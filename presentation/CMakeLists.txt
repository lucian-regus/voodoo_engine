cmake_minimum_required(VERSION 3.28)
project(presentation C)

add_library(presentation STATIC
        src/signal_handler.c
)

target_include_directories(presentation PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(presentation PUBLIC
        infrastructure
)