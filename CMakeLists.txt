cmake_minimum_required(VERSION 4.3.3)

project(ChatRoom)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Dependencias
include(FetchContent)
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git)
FetchContent_MakeAvailable(nlohmann_json)

# Ejecutables

include_directories(src/libs)

add_executable(servidor src/modelado/servidor.cpp)

add_executable(cliente src/modelado/cliente.cpp)
include_directories(src/libs)

target_include_directories(cliente PRIVATE ${CMAKE_SOURCE_DIR}/libs)
