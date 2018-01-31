cmake_minimum_required (VERSION 3.2)
project(trading-base C CXX)

SET (MAIN_SOURCES
    MarketEventHandler.cpp
    MarketEventHandler.h
    TradingBase.h
    TradingPlatform.cpp
    TradingPlatform.h
    SingleList.h
)

SET (SOURCES ${MAIN_SOURCES})

# define project's build target with project's source files
add_library(${PROJECT_NAME} SHARED ${SOURCES})
target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_definitions(-DEXPORT_TRADING_BASE)

if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
    SET (PLATFORM x64)
else( CMAKE_SIZEOF_VOID_P EQUAL 8 )
    SET (PLATFORM x86)
endif( CMAKE_SIZEOF_VOID_P EQUAL 8 )

SET (INSTALL_DEST ${CMAKE_BINARY_DIR}/bin/${PLATFORM}/)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Debug/${PROJECT_NAME}.dll DESTINATION ${INSTALL_DEST}/debug CONFIGURATIONS Debug)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Release/${PROJECT_NAME}.dll DESTINATION ${INSTALL_DEST}/release CONFIGURATIONS Release)