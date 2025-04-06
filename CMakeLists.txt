cmake_minimum_required(VERSION 3.5)

project(xiaozhi_qt VERSION 0.1 LANGUAGES CXX C)

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

# 配置 libfvad 静态库
set(LIBFVAD_SOURCES
    third/libfvad/src/fvad.c
    third/libfvad/src/signal_processing/division_operations.c
    third/libfvad/src/signal_processing/energy.c
    third/libfvad/src/signal_processing/get_scaling_square.c
    third/libfvad/src/signal_processing/resample_48khz.c
    third/libfvad/src/signal_processing/resample_by_2_internal.c
    third/libfvad/src/signal_processing/resample_fractional.c
    third/libfvad/src/signal_processing/spl_inl.c
    third/libfvad/src/vad/vad_core.c
    third/libfvad/src/vad/vad_filterbank.c
    third/libfvad/src/vad/vad_gmm.c
    third/libfvad/src/vad/vad_sp.c
)

add_library(fvad STATIC ${LIBFVAD_SOURCES})
target_include_directories(fvad
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/third/libfvad/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third/libfvad/src
        ${CMAKE_CURRENT_SOURCE_DIR}/third/libfvad/src/signal_processing
        ${CMAKE_CURRENT_SOURCE_DIR}/third/libfvad/src/vad
)
target_compile_definitions(fvad PRIVATE WEBRTC_POSIX)

# 查找所需的 Qt 组件
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Multimedia WebSockets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Multimedia WebSockets Network)

# 查找 Opus 库
find_package(PkgConfig REQUIRED)
pkg_check_modules(OPUS REQUIRED opus)

# 查找 Ogg 库
pkg_check_modules(OGG REQUIRED ogg)

# 设置 Vosk 本地路径
set(VOSK_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third/vosk/include)
set(VOSK_LIBRARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third/vosk/lib)
set(VOSK_LIBRARY ${VOSK_LIBRARY_DIR}/libvosk.so)

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
    wake_word_detector.cpp
    wake_word_detector.h
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
    fvad
    ${VOSK_LIBRARY}
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
    ${CMAKE_CURRENT_SOURCE_DIR}/third/libfvad/include
    ${VOSK_INCLUDE_DIR}
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
    ${VOSK_LIBRARY_DIR}
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

# 在目标中添加 Vosk
target_link_libraries(xiaozhi_qt PRIVATE ${VOSK_LIBRARY})

# 设置本地库路径
set(LOCAL_INSTALL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/local)
list(APPEND CMAKE_PREFIX_PATH ${LOCAL_INSTALL_DIR})

# 设置运行时链接路径
set_target_properties(xiaozhi_qt PROPERTIES
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/third/vosk/lib"
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(xiaozhi_qt)
endif() 