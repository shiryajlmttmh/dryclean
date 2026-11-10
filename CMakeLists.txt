cmake_minimum_required(VERSION 3.21)
project(dryclean LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_library(core STATIC
    src/types.cpp
    src/pricing.cpp
    src/services.cpp
    src/exceptions.cpp
    src/normalize.cpp
    src/record_io.cpp
    src/client.cpp
    src/employee.cpp
    src/material.cpp
    src/machine.cpp
    src/item.cpp
    src/validation.cpp
    src/status_record.cpp
    src/order.cpp
    src/batch.cpp
    src/storage.cpp
    src/rules.cpp
    src/batch_planner.cpp
    src/shop.cpp
    src/display.cpp
)
target_include_directories(core PUBLIC include)

add_executable(dryclean
    src/main.cpp
    src/gui/login_dialog.cpp
    src/gui/main_window.cpp
    src/gui/client_dialog.cpp
    src/gui/item_dialog.cpp
    src/gui/order_dialog.cpp
    src/gui/table_utils.cpp
    src/gui/helpers.cpp
    src/gui/planning_tab.cpp
    src/gui/batches_tab.cpp
    include/gui/login_dialog.h
    include/gui/main_window.h
    include/gui/client_dialog.h
    include/gui/item_dialog.h
    include/gui/order_dialog.h
    include/gui/table_utils.h
    include/gui/enum_combo.h
    include/gui/helpers.h
    include/gui/planning_tab.h
    include/gui/batches_tab.h
    include/gui/qt_convert.h
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
