# CMake 最低版本号要求
cmake_minimum_required(VERSION 3.10.2)
project("kcp")

# support C++11
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")

set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)
set (LIB_NAME kcpp)

if(UNIX)
    if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
        # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-long-long -pedantic")
        # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")  # Inhibit all warning messages.
        set(CXX_FLAGS
                -g
                # -DVALGRIND
                # -DMUDUO_STD_STRING
                -DCHECK_PTHREAD_RETURN_VALUE
                -D_FILE_OFFSET_BITS=64
                -Wall
                -Wextra
                -Werror
                # -Wconversion
                -Wno-unused-parameter
                # -Wold-style-cast
                -Wno-multichar
                -Wno-unused-function
                -Wno-reorder
                -Woverloaded-virtual
                -Wpointer-arith
                # -Wshadow
                -Wwrite-strings
                -march=native
                # -MMD
                -std=c++0x
                -rdynamic
                )
        string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CXX_FLAGS}")
    endif()
endif()

set(kcp_SRCS ./kcpp-master/ikcp.c)

add_library(${LIB_NAME} ${kcp_SRCS})
target_link_libraries(${LIB_NAME})

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test)
    message(STATUS  "has test subdirectory.")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/test)
else()
    message(STATUS  "has no test subdirectory.")
endif()

add_library( # Sets the name of the library.
             native-lib

             # Sets the library as a shared library.
             SHARED

             # Provides a relative path to your source file(s).
        TestKcppServer.cpp)

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library( # Sets the name of the path variable.
              log-lib

              # Specifies the name of the NDK library that
              # you want CMake to locate.
              log )

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target library.
                       native-lib

                       # Links the target library to the log library
                       # included in the NDK.
                       ${log-lib} )

target_link_libraries(native-lib ${LIB_NAME})

add_library(
        kcppClient
        SHARED
        TestKcppClient.cpp)

target_link_libraries(
        kcppClient
        ${LIB_NAME}
        ${log-lib} )

