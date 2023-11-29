cmake_minimum_required(VERSION 3.26)
project(hello_circle)

set(CMAKE_CXX_STANDARD 17)

add_executable(hello_circle main.cpp)

include_directories(circle/include circle/lib)

add_subdirectory(circle/sample/01-gpiosimple)
add_subdirectory(circle/sample/33-syslog)
add_subdirectory(circle/sample/34-sounddevices)

add_subdirectory(src)
