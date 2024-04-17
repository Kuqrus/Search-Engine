cmake_minimum_required(VERSION 3.20)

project(HttpServerApp)

add_executable(HttpServerApp 
	main.cpp
	http_connection.h
	http_connection.cpp
	db_worker.cpp
	db_worker.h)

target_compile_features(HttpServerApp PRIVATE cxx_std_17) 

target_link_libraries(HttpServerApp pqxx)  

target_include_directories(HttpServerApp PRIVATE ${Boost_INCLUDE_DIRS})

target_link_libraries(HttpServerApp ${Boost_LIBRARIES})

target_include_directories(HttpServerApp PRIVATE ${OPENSSL_INCLUDE_DIR})
target_link_libraries(HttpServerApp OpenSSL::Crypto OpenSSL::SSL)

target_include_directories(HttpServerApp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(HttpServerApp ParserLib)


set(INI_SOURCE ../config/config.ini)
set(INI_DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/config)
file(COPY ${INI_SOURCE} DESTINATION ${INI_DESTINATION})

