# CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(TCPServer)

# Set output directories for binaries and libraries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Find required packages
find_package(OpenSSL REQUIRED)

# Include directories
include_directories(include ${OPENSSL_INCLUDE_DIR})

# Add the executable
add_executable(server 
    src/server.c 
    src/socket_utils.c 
    src/html_serve.c 
    src/request_handler.c 
    src/template.c
    src/file_watcher.c
    src/websocket.c
)

# Link with required libraries
target_link_libraries(server ${OPENSSL_LIBRARIES} pthread)
