cmake_minimum_required (VERSION 3.2)
project(common C CXX)

SET (MAIN_SOURCES
    ../common/utility.cpp
    ../common/utility.h
    ../common/CPPRESTBaseTradingPlatform.cpp
    ../common/CPPRESTBaseTradingPlatform.h
    ../common/CPPRESTBaseTradingPlatformThreadSafe.cpp
    ../common/CPPRESTBaseTradingPlatformThreadSafe.h
)

SET (SOURCES ${MAIN_SOURCES})

# define project's build target with project's source files
add_library(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${PROJECT_NAME} PUBLIC trading-base cpprestsdk)