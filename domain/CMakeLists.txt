cmake_minimum_required(VERSION 3.28)
project(domain C)

find_package(PostgreSQL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)

add_library(domain STATIC
        src/database.c
        src/mappers.c
        src/logger.c
)

target_include_directories(domain PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${PostgreSQL_INCLUDE_DIRS}
        ${GLIB_INCLUDE_DIRS}
)

target_link_libraries(domain PRIVATE
        ${PostgreSQL_LIBRARIES}
        ${GLIB_LIBRARIES}
)