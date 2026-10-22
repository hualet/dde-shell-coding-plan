cmake_minimum_required(VERSION 3.16)

project(dde-coding-plan VERSION 1.0.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

include(GNUInstallDirs)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Network Quick QuickControls2 Test LinguistTools)
find_package(Dtk6 REQUIRED COMPONENTS Core Gui Widget)
find_package(DDEShell REQUIRED)

add_library(coding-plan-core STATIC
    src/codingplanmodel.h
    src/codingplanmodel.cpp
    src/providerregistry.h
    src/providerregistry.cpp
    src/credential_store.h
    src/credential_store.cpp
    src/quota_parsers.h
    src/quota_parsers.cpp
    src/direct_quota_provider.h
    src/direct_quota_provider.cpp
)

target_include_directories(coding-plan-core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(coding-plan-core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Network
)

add_library(ds-coding-plan-applet SHARED
    src/codingplanapplet.h
    src/codingplanapplet.cpp
    src/codingplansettings.h
    src/codingplansettings.cpp
)

target_include_directories(ds-coding-plan-applet PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ds-coding-plan-applet PRIVATE
    coding-plan-core
    Dde::Shell
    Dtk6::Core
    Dtk6::Widget
    Qt6::Core
    Qt6::Network
    Qt6::Quick
    Qt6::QuickControls2
)

set(DS_CODING_PLAN_PACKAGE_ID org.deepin.ds.coding-plan)

ds_install_package(
    PACKAGE ${DS_CODING_PLAN_PACKAGE_ID}
    TARGET ds-coding-plan-applet
)

# hicolor icons + desktop file: the UOS app store package check requires a
# .desktop file whose Icon= entry resolves inside the package. NoDisplay
# keeps this taskbar plugin out of the app launcher.
install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/hicolor/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor
)

install(
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/package/dde-shell-coding-plan.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

# lupdate/lrelease the plugin's user-visible strings (package QML + applet
# and core sources) and install the .qm catalogs into the package
# translations dir; dde-shell loads <pluginId>_<locale>.qm from there at
# runtime.
ds_handle_package_translation(
    PACKAGE ${DS_CODING_PLAN_PACKAGE_ID}
    QML_FILES package/main.qml
    SOURCE_FILES src/codingplanapplet.cpp
                src/codingplansettings.cpp
                src/codingplanmodel.cpp
                src/direct_quota_provider.cpp
                src/quota_parsers.cpp
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
    # QTEST_MAIN links Qt6::Test (→ Qt6::Gui) and therefore needs a platform
    # plugin; force offscreen so headless runners (dh_auto_test, CI) work.
    set_tests_properties(providerregistry_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()
