cmake_minimum_required (VERSION 3.2)
project(GoCrypto C CXX)

SET (COMMON_SRC
    ../common/Utility.h
    ../common/Utility.cpp
    ../common/AutoScope.cpp
    ../common/AutoScope.h
    ../common/SyncMessageQueue.hpp
)

SET (ENGINE_SRC
    Engine/NAPMarketEventHandler.cpp
    Engine/NAPMarketEventHandler.h
    Engine/Notifier.cpp
    Engine/Notifier.h
    Engine/PlatformEngine.cpp
    Engine/PlatformEngine.h
)

SET (UI_SRC
    UI/CiWidget.cpp
    UI/CiWidget.h
    UI/ImWidget.cpp
    UI/ImWidget.h
    UI/Spliter.cpp
    UI/Spliter.h
    UI/Widget.cpp
    UI/Widget.h
    UI/WxAppLog.cpp
    UI/WxAppLog.h
    UI/WxCryptoBoardInfo.cpp
    UI/WxCryptoBoardInfo.h
    UI/WxLineGraph.cpp
    UI/WxLineGraph.h
    UI/WxLineGraphLive.cpp
    UI/WxLineGraphLive.h
    UI/WxControlBoard.cpp
    UI/WxControlBoard.h
)

SET (MAIN_SOURCES
    BasicApp.cpp
    GoCrypto.h
    LogAdapter.h
    LogAdapter.cpp
    Resources/Resources.h
    Resources/Resources.rc
    ${UI_SRC}
    ${ENGINE_SRC}
    ${COMMON_SRC}
)

SET (SOURCES ${MAIN_SOURCES})

if(MSVC)
    source_group(UI FILES ${UI_SRC})
    source_group(Engine FILES ${ENGINE_SRC})
    source_group(Common FILES ${COMMON_SRC})
endif()

# define project's build target with project's source files
add_executable(${PROJECT_NAME} WIN32  ${SOURCES})

target_link_libraries(${PROJECT_NAME} trading-base cpprestsdk)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

include (${CMAKE_CURRENT_SOURCE_DIR}/../cmake/cinder.cmake)
include (${CMAKE_CURRENT_SOURCE_DIR}/../cmake/cinder-imgui.cmake)

if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
    SET (PLATFORM x64)
else( CMAKE_SIZEOF_VOID_P EQUAL 8 )
    SET (PLATFORM x86)
endif( CMAKE_SIZEOF_VOID_P EQUAL 8 )

SET (DEBUG_CPPRESTSDKDLL_PATH ${CPPRESTSDK_BIN_DIR}/debug/cpprest140d_2_9.dll)
SET (RELEASE_CPPRESTSDKDLL_PATH ${CPPRESTSDK_BIN_DIR}/release/cpprest140_2_9.dll)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} ARGS -E copy "$<$<CONFIG:debug>:${DEBUG_CPPRESTSDKDLL_PATH}>$<$<CONFIG:release>:${RELEASE_CPPRESTSDKDLL_PATH}>" ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIGURATION>/)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} ARGS -E copy $<TARGET_FILE:trading-base> ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIGURATION>/)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} ARGS -E copy $<TARGET_FILE:bitfinex> ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIGURATION>/)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} ARGS -E copy $<TARGET_FILE:hitbtc> ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIGURATION>/)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} ARGS -E copy $<TARGET_FILE:binance> ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIGURATION>/)

if(WIN32)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} ARGS -E copy "$<$<CONFIG:debug>:${CINDER_BIN_D}>$<$<CONFIG:release>:${CINDER_BIN_R}>" ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIGURATION>/)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} ARGS -E copy "$<$<CONFIG:debug>:${CINDER_IMGUI_BIN_D}>$<$<CONFIG:release>:${CINDER_IMGUI_BIN_R}>" ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIGURATION>/)
endif()

SET (INSTALL_DEST ${CMAKE_BINARY_DIR}/bin/${PLATFORM}/)
install(FILES $<TARGET_FILE:${PROJECT_NAME}> DESTINATION ${INSTALL_DEST}/debug CONFIGURATIONS Debug)
install(FILES $<TARGET_FILE:${PROJECT_NAME}> DESTINATION ${INSTALL_DEST}/release CONFIGURATIONS Release)
install(FILES ${DEBUG_CPPRESTSDKDLL_PATH} DESTINATION ${INSTALL_DEST}/debug CONFIGURATIONS Debug)
install(FILES ${RELEASE_CPPRESTSDKDLL_PATH} DESTINATION ${INSTALL_DEST}/release CONFIGURATIONS Release)

if(WIN32)
install(FILES ${CINDER_BIN_D} DESTINATION ${INSTALL_DEST}/debug CONFIGURATIONS Debug)
install(FILES ${CINDER_BIN_R} DESTINATION ${INSTALL_DEST}/release CONFIGURATIONS Release)
install(FILES ${CINDER_IMGUI_BIN_D} DESTINATION ${INSTALL_DEST}/debug CONFIGURATIONS Debug)
install(FILES ${CINDER_IMGUI_BIN_R} DESTINATION ${INSTALL_DEST}/release CONFIGURATIONS Release)
endif()