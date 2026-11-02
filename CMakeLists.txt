cmake_minimum_required(VERSION 3.21)
project(dryclean LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_library(core STATIC
    src/types.cpp
)
target_include_directories(core PUBLIC include)

add_executable(dryclean
    src/main.cpp
)
target_link_libraries(dryclean PRIVATE core Qt6::Widgets)

if(WIN32)
    set_target_properties(dryclean PROPERTIES WIN32_EXECUTABLE ON)

    find_program(WINDEPLOYQT windeployqt HINTS "${CMAKE_PREFIX_PATH}/bin")
    if(WINDEPLOYQT)
        add_custom_command(TARGET dryclean POST_BUILD
            COMMAND "${WINDEPLOYQT}" "$<TARGET_FILE:dryclean>")
    endif()
endif()
