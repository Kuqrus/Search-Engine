cmake_minimum_required(VERSION 3.20)

project(CppDiplomProject)

set (CMAKE_CXX_STANDARD 20) 

set (OPENSSL_SSL_LIBRARIES "C:\\_CPP\\OpenSSL-Win64\\lib\\VC")

set (BOOST_ROOT "C:\\_CPP\\boost_1_84_0")
set (Boost_USE_STATIC_LIBS ON)

set(libpqxx_DIR "C:\\_CPP\\libpqxx-7.7.5")
add_subdirectory(${libpqxx_DIR} libpqxx-build) 

set(PostgreSQL_ADDITIONAL_VERSIONS "15")

add_compile_definitions(_WIN32_WINNT=0x0A01)

find_package(Boost 1.80.0 REQUIRED COMPONENTS system thread locale)
if (NOT Boost_FOUND)
	message(STATUS "Boost Not Found")  
endif()

find_package(OpenSSL REQUIRED)
if (NOT OPENSSL_FOUND)
	message(STATUS "OpenSSL Not Found")  
endif()

find_package(LibXml2 REQUIRED)
if(NOT LIBXML2_FOUND)
	message(SEND_ERROR "Failed to find LibXml2")
endif()

include_directories (SYSTEM ${Boost_INCLUDE_DIR})


add_subdirectory(spider)
add_subdirectory(http_server)
add_subdirectory(parser)
