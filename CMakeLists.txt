cmake_minimum_required(VERSION 3.16)

project(dde-coding-plan VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

include(GNUInstallDirs)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Test WebEngineWidgets)
find_package(Dtk6 REQUIRED COMPONENTS Core Gui Widget)
find_package(DDEShell REQUIRED)

add_library(coding-plan-core STATIC
    src/codingplanmodel.h
    src/codingplanmodel.cpp
    src/providerregistry.h
    src/providerregistry.cpp
)

target_include_directories(coding-plan-core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(coding-plan-core PUBLIC
    Qt6::Core
    Qt6::Gui
)

add_executable(dde-coding-plan
    app/main.cpp
    app/webbridge.h
    app/webbridge.cpp
)

target_include_directories(dde-coding-plan PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/app
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(dde-coding-plan PRIVATE
    coding-plan-core
    Dtk6::Widget
    Qt6::Core
    Qt6::Gui
    Qt6::WebEngineWidgets
)

target_compile_definitions(dde-coding-plan PRIVATE
    CODING_PLAN_WEB_DIR="${CMAKE_INSTALL_FULL_DATADIR}/dde-coding-plan/web"
)

add_custom_target(dde-coding-plan-web ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/web/dist
        $<TARGET_FILE_DIR:dde-coding-plan>/web
    COMMENT "Copy React frontend to build output"
)
add_dependencies(dde-coding-plan-web dde-coding-plan)

install(TARGETS dde-coding-plan DESTINATION ${CMAKE_INSTALL_BINDIR})

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/web/dist/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/dde-coding-plan/web
)

add_library(ds-coding-plan-applet SHARED
    src/codingplanapplet.h
    src/codingplanapplet.cpp
)

target_include_directories(ds-coding-plan-applet PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ds-coding-plan-applet PRIVATE
    coding-plan-core
    Dde::Shell
    Qt6::Core
    Qt6::Quick
    Qt6::QuickControls2
)

set(DS_CODING_PLAN_PACKAGE_ID org.deepin.ds.coding-plan)

ds_install_package(
    PACKAGE ${DS_CODING_PLAN_PACKAGE_ID}
    TARGET ds-coding-plan-applet
)

include(CTest)

if(BUILD_TESTING)
    add_executable(providerregistry_tests
        tests/providerregistry_test.cpp
    )

    target_link_libraries(providerregistry_tests PRIVATE
        coding-plan-core
        Qt6::Test
    )
    target_compile_definitions(providerregistry_tests PRIVATE
        SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )

    add_test(NAME providerregistry_tests COMMAND providerregistry_tests)
endif()
