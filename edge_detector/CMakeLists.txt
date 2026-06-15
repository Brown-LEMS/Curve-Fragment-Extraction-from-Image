cmake_minimum_required(VERSION 3.10)

project(EdgeDetect)
set(EXECUTABLE edt)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS_DEBUG   "-Wall -Wextra -Wpedantic -g" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG" CACHE STRING "" FORCE)

set(SRC src/main.cpp src/detect.cpp)

find_package(OpenCV 4 REQUIRED)

add_executable(${EXECUTABLE} ${SRC})
target_include_directories(${EXECUTABLE} PRIVATE src)
target_include_directories(${EXECUTABLE} PUBLIC include)
target_link_libraries(${EXECUTABLE} ${OpenCV_LIBS})
