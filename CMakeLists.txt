cmake_minimum_required(VERSION 3.31)
project(Protei_ITSchool2025_Network)

set(CMAKE_CXX_STANDARD 17)

add_library(calculator STATIC
        calculator.cpp
        calculator.h
)

add_executable(server server.cpp
)
target_link_libraries(server PRIVATE calculator)

add_executable(client client.cpp
)
target_link_libraries(client PRIVATE calculator)