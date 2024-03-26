cmake_minimum_required(VERSION 3.20)


add_executable(Parser
	parser.cpp
	parser.h
	)

add_library(ParserLib parser.cpp parser.h)

target_include_directories(ParserLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})