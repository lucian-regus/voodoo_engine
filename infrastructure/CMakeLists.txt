cmake_minimum_required(VERSION 3.28)
project(infrastructure C)

add_library(infrastructure STATIC
        src/context.c
        src/file_system_monitor.c
        src/fanotify_monitor.c
        src/fanotify_event_consumer.c
        src/file_system_event_consumer.c
        src/plugin_manager.c
        src/helpers.c
        src/quarantine.c
        src/unix_control_server.c
)

target_include_directories(infrastructure PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(infrastructure PUBLIC
        domain
        uuid
)