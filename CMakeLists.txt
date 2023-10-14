cmake_minimum_required(VERSION 3.0.0)

project(midi LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_FLAGS "-O2 -Wall -Wextra")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")

add_custom_command(
    OUTPUT  ${CMAKE_SOURCE_DIR}/src/cmdline.c
    COMMAND gengetopt --input=${CMAKE_SOURCE_DIR}/src/tapper.ggo --unnamed-opts --file-name=cmdline --output-dir=${CMAKE_SOURCE_DIR}/src
    DEPENDS ${CMAKE_SOURCE_DIR}/src/tapper.ggo
)

add_executable(midi-tapper
  ${CMAKE_SOURCE_DIR}/src/cmdline.c
  ${CMAKE_SOURCE_DIR}/src/tapper.c
)

target_link_libraries(midi-tapper rt)

install(PROGRAMS ${CMAKE_SOURCE_DIR}/src/midi-gw.py DESTINATION bin)
install(TARGETS midi-tapper
        RUNTIME DESTINATION bin)
