cmake_minimum_required(VERSION 3.24)

project(carrier_sync_lab VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

add_executable(carrier_sync_lab WIN32
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/simulation/carrier_sync_simulator.cpp
    src/simulation/carrier_sync_simulator.h
    src/widgets/history_plot_widget.cpp
    src/widgets/history_plot_widget.h
    src/widgets/phase_ring_widget.cpp
    src/widgets/phase_ring_widget.h
)

target_include_directories(carrier_sync_lab PRIVATE src)

target_link_libraries(carrier_sync_lab PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

if(WIN32)
    add_custom_command(TARGET carrier_sync_lab POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:carrier_sync_lab>
            $<TARGET_FILE_DIR:carrier_sync_lab>
        COMMAND_EXPAND_LISTS
    )
endif()
