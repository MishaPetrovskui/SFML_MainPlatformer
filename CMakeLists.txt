cmake_minimum_required(VERSION 3.5)
project(SFML_MainPlatformer)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sources
set(SOURCES
    SFML_MainPlatformer/main.cpp
    SFML_MainPlatformer/Player.cpp
    SFML_MainPlatformer/Enemy.cpp
)

find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)
if(NOT SFML_FOUND)
    message(FATAL_ERROR "SFML not found. Install SFML and set CMAKE_PREFIX_PATH or SFML_DIR.")
endif()

add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE SFML_MainPlatformer)
target_link_libraries(${PROJECT_NAME} PRIVATE sfml-graphics sfml-window sfml-system)
