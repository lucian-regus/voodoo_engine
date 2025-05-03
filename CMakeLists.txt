cmake_minimum_required(VERSION 3.28)
project(voodoo_av C)

add_subdirectory(domain)
add_subdirectory(infrastructure)
add_subdirectory(presentation)

add_executable(voodoo_av
        presentation/src/main.c
)

set_target_properties(voodoo_av PROPERTIES LINK_FLAGS "-Wl,-export-dynamic")

install(DIRECTORY
        infrastructure/include/infrastructure
        domain/include/domain

        DESTINATION include/voodoo-av
)

target_link_libraries(voodoo_av
        -Wl,--whole-archive domain -Wl,--no-whole-archive
        infrastructure
        presentation
)