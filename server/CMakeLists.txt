cmake_minimum_required(VERSION 2.8)
find_package(Boost 1.56 COMPONENTS system log	 REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})
add_executable(server alloc.cpp game.cpp network.cpp userdb.cpp main.cpp snake_generated.h)
add_custom_command(
    OUTPUT snake_generated.h
    DEPENDS ../schema/snake.fbs
    COMMAND "flatc" -c --gen-mutable ../schema/snake.fbs
    PRE_BUILD)
target_link_libraries(server ${Boost_LIBRARIES})
set_property(TARGET server PROPERTY CXX_STANDARD 11)
add_definitions(-DBOOST_LOG_DYN_LINK)
