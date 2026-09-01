cmake_minimum_required(VERSION 3.10)
project(FileSyncer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_CXX_COMPILER "g++")

find_package(libssh CONFIG REQUIRED)
find_package(xxhash CONFIG REQUIRED)
find_package(zlib CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(doctest REQUIRED)

add_executable(FileSyncer src/main.cpp src/cli.cpp src/sync.cpp)
target_include_directories(FileSyncer PRIVATE include)
target_link_libraries(FileSyncer PRIVATE ssh xxHash::xxhash doctest::doctest)

enable_testing()

add_executable(test_ssh tests/test_ssh.cpp)
target_include_directories(test_ssh PRIVATE include)
target_link_libraries(test_ssh PRIVATE ssh)
add_test(NAME test_ssh COMMAND test_ssh)

add_executable(test_protocol tests/test_protocol.cpp)
target_include_directories(test_protocol PRIVATE include)
target_link_libraries(test_protocol PRIVATE doctest::doctest xxHash::xxhash)
add_test(NAME test_protocol COMMAND test_protocol)

add_executable(test_read tests/test_read.cpp)
target_include_directories(test_read PRIVATE include)
add_test(NAME test_read COMMAND test_read)

add_executable(test_sync tests/test_sync.cpp src/sync.cpp)
target_include_directories(test_sync PRIVATE include)
target_link_libraries(test_sync PRIVATE ssh xxHash::xxhash)
add_test(NAME test_sync COMMAND test_sync)