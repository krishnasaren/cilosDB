cmake_minimum_required(VERSION 3.10)
project(HybridDB VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform detection
if(WIN32)
    add_definitions(-DPLATFORM_WINDOWS)
    set(PLATFORM_LIBS ws2_32)
else()
    add_definitions(-DPLATFORM_UNIX)
    set(PLATFORM_LIBS pthread)
endif()

# Compiler flags
if(MSVC)
    add_compile_options(/W4 /O2)
else()
    add_compile_options(-Wall -Wextra -O3 -march=native)
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files
file(GLOB_RECURSE SOURCES 
    "src/server/*.cpp"
    "src/storage/*.cpp"
    "src/query/*.cpp"
    "src/network/*.cpp"
)

# Main executable
add_executable(hybriddb-server ${SOURCES})
target_link_libraries(hybriddb-server ${PLATFORM_LIBS})

# Installation
install(TARGETS hybriddb-server DESTINATION bin)
install(DIRECTORY web/admin DESTINATION share/hybriddb)
install(DIRECTORY web/user DESTINATION share/hybriddb)
install(DIRECTORY client-libs DESTINATION share/hybriddb)

# Print configuration
message(STATUS "===========================================")
message(STATUS "HybridDB Database Server v${PROJECT_VERSION}")
message(STATUS "===========================================")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "===========================================")