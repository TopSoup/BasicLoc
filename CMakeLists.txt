cmake_minimum_required(VERSION 3.5)
project(BasicLoc)

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -W -fexceptions")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wall -O2 -DNDEBUG")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/Lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/Lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/BasicLoc)

add_definitions(
        -DWIN32
        -D_DEBUG
        -D_WINDOWS
        -D_MBCS
        -D_USRDLL
        -DAEE_SIMULATOR
)

include_directories(./BasicLoc
                    ./BasicLoc/Location
                    d:/brew/sdk/inc
                    d:/brew/sdk/sdk/inc)
set(SOURCE_FILES
    BasicLoc/Location/Location.c
    BasicLoc/Location/Location.h
    BasicLoc/AEEAppGen.c
    BasicLoc/AEEModGen.c
    BasicLoc/basicloc.bid
    BasicLoc/BasicLoc.c)

add_library(BasicLoc SHARED ${SOURCE_FILES})

#去掉lib前缀
set_target_properties(BasicLoc PROPERTIES PREFIX "")