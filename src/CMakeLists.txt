cmake_minimum_required(VERSION 2.8)
PROJECT(UpgradeServer)
cmake_policy(SET CMP0015 NEW)

include_directories(./include)
AUX_SOURCE_DIRECTORY(./ SRC)
AUX_SOURCE_DIRECTORY(./tinyxml  TINYXML_DIR_SRCS)

#设置build类型 
#SET(CMAKE_BUILD_TYPE Debug)
SET(CMAKE_BUILD_TYPE Release)

message(STATUS "build " ${CMAKE_BUILD_TYPE})

#设置编译器
if(CMAKE_COMPILER_IS_GNUCC)
    message(STATUS "optional:gnu gcc")   
	SET(CMAKE_C_COMPILER  /usr/bin/gcc)
	SET(CMAKE_C_FLAGS_DEBUG -g)
endif(CMAKE_COMPILER_IS_GNUCC)


if(CMAKE_COMPILER_IS_GNUCXX)
    #message(STATUS "optional:-std=c++0x")   
	SET(CMAKE_CXX_COMPILER /usr/bin/g++)
	#C++0x是C++11标准成为正式标准之前的草案临时名字
	set(CMAKE_CXX_FLAGS "-std=c++0x ${CMAKE_CXX_FLAGS}")
	SET (CMAKE_CXX_FLAGS_DEBUG  -g)
endif(CMAKE_COMPILER_IS_GNUCXX)



set(CMAKE_CURRENT_SOURCE_DIR  ./..)


link_directories(./lib)


ADD_EXECUTABLE(UpgradeServer ${SRC} ${TINYXML_DIR_SRCS})

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)


target_link_libraries(UpgradeServer ev  pthread)
