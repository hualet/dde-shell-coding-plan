cmake_minimum_required(VERSION 3.16)

project(dde-shell-coding-plan VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Test)
find_package(DDEShell QUIET)

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

if(DDEShell_FOUND)
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

    ds_build_package(
        PACKAGE ${DS_CODING_PLAN_PACKAGE_ID}
        TARGET ds-coding-plan-applet
    )

    install(
        DIRECTORY ${PROJECT_BINARY_DIR}/packages/${DS_CODING_PLAN_PACKAGE_ID}/
        DESTINATION ${DDE_SHELL_PACKAGE_INSTALL_DIR}/${DS_CODING_PLAN_PACKAGE_ID}
        PATTERN ".git" EXCLUDE
        PATTERN ".gitignore" EXCLUDE
    )

    install(
        TARGETS ds-coding-plan-applet
        DESTINATION ${DDE_SHELL_PLUGIN_INSTALL_DIR}/
    )
else()
    message(STATUS "DDEShell not found; building testable core only.")
endif()

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
