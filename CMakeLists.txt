cmake_minimum_required(VERSION 3.20)
project(so_projekt C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pthread")

include_directories(include)

# Główny executable
add_executable(so_projekt
    src/main.c
)

# Linkowanie z bibliotekami systemowymi
target_link_libraries(so_projekt pthread rt)
