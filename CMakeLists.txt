cmake_minimum_required(VERSION 3.5)

project(xiaozhi_qt VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 添加调试信息
set(CMAKE_BUILD_TYPE Debug)
add_compile_options(-g)

# 添加编译选项
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(QT_DEBUG)
endif()

# 查找所需的 Qt 组件
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Multimedia WebSockets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Multimedia WebSockets Network)

# 查找 Opus 库
find_package(PkgConfig REQUIRED)
pkg_check_modules(OPUS REQUIRED opus)

# 查找 Ogg 库
pkg_check_modules(OGG REQUIRED ogg)

# 查找 WebRTC VAD 库
pkg_check_modules(FVAD REQUIRED libfvad)

set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    websocket_client.cpp
    websocket_client.h
    microphone_manager.cpp
    microphone_manager.h
    speaker_manager.cpp
    speaker_manager.h
    opus_encoder.cpp
    opus_encoder.h
    opus_decoder.cpp
    opus_decoder.h
    webrtcvad.cpp
    webrtcvad.h
    vad_processor.cpp
    vad_processor.h
)

# 主程序
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(xiaozhi_qt
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    add_executable(xiaozhi_qt
        ${PROJECT_SOURCES}
    )
endif()

# # 测试程序
# add_executable(test_opus_encoder
#     test_opus_encoder.cpp
#     opus_encoder.cpp
#     opus_decoder.cpp
#     speaker_manager.cpp
#     microphone_manager.cpp
# )

# add_executable(test_speaker_manager
#     test_speaker_manager.cpp
#     speaker_manager.cpp
# )

# # 添加正弦波测试程序
# add_executable(test_sine_wave
#     test_sine_wave.cpp
#     opus_encoder.cpp
#     opus_decoder.cpp
#     speaker_manager.cpp
# )

# 链接 Qt 库 - 主程序
target_link_libraries(xiaozhi_qt PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Multimedia
    Qt${QT_VERSION_MAJOR}::WebSockets
    Qt${QT_VERSION_MAJOR}::Network
    ${OPUS_LIBRARIES}
    ${OGG_LIBRARIES}
    ${FVAD_LIBRARIES}
)

# 链接 Qt 库 - 测试程序
# target_link_libraries(test_opus_encoder PRIVATE
#     Qt${QT_VERSION_MAJOR}::Core
#     Qt${QT_VERSION_MAJOR}::Multimedia
#     ${OPUS_LIBRARIES}
#     ${OGG_LIBRARIES}
# )

# target_link_libraries(test_speaker_manager PRIVATE
#     Qt${QT_VERSION_MAJOR}::Core
#     Qt${QT_VERSION_MAJOR}::Multimedia
# )

# target_link_libraries(test_sine_wave PRIVATE
#     Qt${QT_VERSION_MAJOR}::Core
#     Qt${QT_VERSION_MAJOR}::Multimedia
#     ${OPUS_LIBRARIES}
#     ${OGG_LIBRARIES}
# )

# 设置包含目录 - 主程序
target_include_directories(xiaozhi_qt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${OPUS_INCLUDE_DIRS}
    ${OGG_INCLUDE_DIRS}
    ${FVAD_INCLUDE_DIRS}
)

# 设置包含目录 - 测试程序
# target_include_directories(test_opus_encoder PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ${CMAKE_CURRENT_BINARY_DIR}
#     ${OPUS_INCLUDE_DIRS}
#     ${OGG_INCLUDE_DIRS}
# )

# target_include_directories(test_speaker_manager PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ${CMAKE_CURRENT_BINARY_DIR}
# )

# target_include_directories(test_sine_wave PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ${CMAKE_CURRENT_BINARY_DIR}
#     ${OPUS_INCLUDE_DIRS}
#     ${OGG_INCLUDE_DIRS}
# )

# 设置链接目录 - 主程序
target_link_directories(xiaozhi_qt PRIVATE
    ${OPUS_LIBRARY_DIRS}
    ${OGG_LIBRARY_DIRS}
    ${FVAD_LIBRARY_DIRS}
)

# 设置链接目录 - 测试程序
# target_link_directories(test_opus_encoder PRIVATE
#     ${OPUS_LIBRARY_DIRS}
#     ${OGG_LIBRARY_DIRS}
# )

# target_link_directories(test_sine_wave PRIVATE
#     ${OPUS_LIBRARY_DIRS}
#     ${OGG_LIBRARY_DIRS}
# )

# 安装目标
install(TARGETS xiaozhi_qt
    RUNTIME DESTINATION bin
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(xiaozhi_qt)
endif() 