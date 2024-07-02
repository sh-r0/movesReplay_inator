cmake_minimum_required(VERSION 3.26.0)

set (CMAKE_CXX_STANDARD 20)

project(movesReplay_inator)

set(CMAKE_SOURCE_DIR, "${MY_ROOT}/src")
list(APPEND sourcesList "src/main.cpp")
add_executable(${PROJECT_NAME} ${sourcesList})
