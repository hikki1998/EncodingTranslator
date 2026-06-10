cmake_minimum_required(VERSION 3.16)
project(EncodingTranslator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 5.15 REQUIRED COMPONENTS Core Widgets Concurrent)

add_executable(EncodingTranslator WIN32
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/EncodingUtils.h
    src/EncodingUtils.cpp
    src/SearchWorker.h
    src/SearchWorker.cpp
    src/ConvertWorker.h
    src/ConvertWorker.cpp
    resources/app.qrc
    resources/app.rc
)

target_link_libraries(EncodingTranslator PRIVATE Qt5::Core Qt5::Widgets Qt5::Concurrent)

target_compile_definitions(EncodingTranslator PRIVATE UNICODE _UNICODE)

if(MSVC)
    target_compile_options(EncodingTranslator PRIVATE /W4 /utf-8 /permissive-)
endif()

add_executable(test_search
    src/EncodingUtils.h
    src/EncodingUtils.cpp
    src/SearchWorker.h
    src/SearchWorker.cpp
    tests/test_search.cpp
)
target_link_libraries(test_search PRIVATE Qt5::Core Qt5::Concurrent)
target_compile_definitions(test_search PRIVATE UNICODE _UNICODE)
if(MSVC)
    target_compile_options(test_search PRIVATE /W4 /utf-8 /permissive-)
endif()

if(WIN32)
    get_target_property(QT_QMAKE_EXECUTABLE Qt5::qmake IMPORTED_LOCATION)
    if(QT_QMAKE_EXECUTABLE)
        get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
        set(WINDEPLOYQT_EXECUTABLE "${QT_BIN_DIR}/windeployqt.exe")
        if(EXISTS "${WINDEPLOYQT_EXECUTABLE}")
            add_custom_command(TARGET EncodingTranslator POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                    --no-translations
                    --compiler-runtime
                    "$<TARGET_FILE:EncodingTranslator>"
                WORKING_DIRECTORY "$<TARGET_FILE_DIR:EncodingTranslator>"
                COMMENT "Copying required Qt runtime dependencies with windeployqt"
            )
        endif()
    endif()
endif()
