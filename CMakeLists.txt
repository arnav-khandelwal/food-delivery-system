cmake_minimum_required(VERSION 3.10)
project(SimpleDelivery)

set(CMAKE_CXX_STANDARD 17)

# Add necessary compiler options for MSVC
if(MSVC)
    add_compile_options(/utf-8)
    add_compile_definitions(_WIN32_WINNT=0x0601)
endif()

# Find SQLite3
find_package(SQLite3 REQUIRED)

# Add the executable
add_executable(delivery_system main.cpp)

# Link the libraries
target_link_libraries(delivery_system PRIVATE SQLite::SQLite3)

# On Windows, link the WinSock2 library
if(WIN32)
    target_link_libraries(delivery_system PRIVATE ws2_32)
endif()

# Copy frontend files to build directory
file(COPY 
    ${CMAKE_CURRENT_SOURCE_DIR}/index.html
    ${CMAKE_CURRENT_SOURCE_DIR}/style.css
    ${CMAKE_CURRENT_SOURCE_DIR}/script.js
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
)

message(STATUS "Configuration complete - run 'cmake --build . --config Release' to build")