cmake_minimum_required(VERSION 3.10)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(cos-hash)

file(GLOB SRC "src/*.cpp")

add_executable(${PROJECT_NAME} ${SRC})