cmake_minimum_required(VERSION 3.0)

# project
set(CMAKE_CXX_STANDARD 11)
add_definitions(-Wall -g)
PROJECT(test_asio)

# source && bin
set(PROJECT_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(PROJECT_BIN ${CMAKE_CURRENT_SOURCE_DIR}/bin)

# client
set(test_asio
${PROJECT_SRC}/test.cpp
)
set(test_acceptor
${PROJECT_SRC}/test_acceptor.cpp
)

# execute
add_executable(${PROJECT_NAME} ${test_asio})
target_link_libraries(${PROJECT_NAME} pthread)
add_executable(test_acceptor ${test_acceptor})
target_link_libraries(test_acceptor pthread)
