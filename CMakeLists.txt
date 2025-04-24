# CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(Blink)

# Set output directories for binaries and libraries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Find required packages
find_package(OpenSSL REQUIRED)
find_package(SQLite3 REQUIRED)

# Include directories
include_directories(include ${OPENSSL_INCLUDE_DIR} ${SQLite3_INCLUDE_DIRS})

# Add the executable
add_executable(blink 
    src/server.c 
    src/socket_utils.c 
    src/html_serve.c 
    src/request_handler.c 
    src/template.c
    src/file_watcher.c
    src/websocket.c
    src/sqlite_handler.c
)

# Link with required libraries
target_link_libraries(blink ${OPENSSL_LIBRARIES} ${SQLite3_LIBRARIES} pthread)

# Copy www directory to build directory
add_custom_command(
    TARGET blink POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/www ${CMAKE_BINARY_DIR}/bin/www
    COMMENT "Copying www directory to build directory"
)
