cmake_minimum_required(VERSION 2.8.8)
cmake_policy(SET CMP0048 NEW)
project(le-das-tm_relay VERSION 2.0.0)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include( ../../../oui_setup.cmake )

OUI_FILE(tm_relay.oui)
add_executable(tm_relay tm_relay.cc tm_relayoui.cc)
target_link_libraries(tm_relay dasio nl pthread)

install(TARGETS tm_relay RUNTIME DESTINATION bin)
