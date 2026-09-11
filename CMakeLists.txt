cmake_minimum_required(VERSION 3.21)

project(MegaDirStat VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# QStyleHints::setColorScheme (used for the light/dark override) is 6.8+.
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Concurrent Test)
qt_standard_project_setup()

# Warning level for our own targets only; linked PRIVATE so it never reaches
# third_party/.
add_library(MegaDirStatWarnings INTERFACE)
if(MSVC)
    # /external:W0: MSVC ignores the SYSTEM-ness of Qt's include dirs until an
    # external warning level is set, and moc output would then warn from Qt headers.
    target_compile_options(MegaDirStatWarnings INTERFACE
        /W4 /external:W0 /permissive- /utf-8 /Zc:__cplusplus)
else()
    target_compile_options(MegaDirStatWarnings INTERFACE -Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------- core
# No Widgets, no MEGA SDK: the snapshot model, the treemap layout and the
# IAccountSource port. Everything else depends on this, never the reverse.
qt_add_library(MegaDirStatCore STATIC
    src/core/AccountSnapshot.h
    src/core/IAccountSource.h
    src/core/SizeNode.h src/core/SizeNode.cpp
    src/core/TreemapLayout.h src/core/TreemapLayout.cpp
)
target_include_directories(MegaDirStatCore PUBLIC src)
target_link_libraries(MegaDirStatCore
    PUBLIC Qt6::Core
    PRIVATE MegaDirStatWarnings)

# ---------------------------------------------------------------- mock
qt_add_library(MegaDirStatMock STATIC
    src/mock/FixtureLoader.h src/mock/FixtureLoader.cpp
    src/mock/MockAccountSource.h src/mock/MockAccountSource.cpp
    src/mock/TreeGenerator.h src/mock/TreeGenerator.cpp
)
target_link_libraries(MegaDirStatMock
    PUBLIC MegaDirStatCore
    PRIVATE Qt6::Concurrent MegaDirStatWarnings)

# ---------------------------------------------------------------- app
qt_add_executable(MegaDirStat WIN32 MACOSX_BUNDLE
    main.cpp
    src/ui/MainWindow.h src/ui/MainWindow.cpp
    src/ui/PercentBarDelegate.h src/ui/PercentBarDelegate.cpp
    src/ui/SizeTreeModel.h src/ui/SizeTreeModel.cpp
    src/ui/TreemapWidget.h src/ui/TreemapWidget.cpp
)
target_link_libraries(MegaDirStat PRIVATE
    MegaDirStatCore
    MegaDirStatMock
    Qt6::Widgets
    MegaDirStatWarnings)
target_compile_definitions(MegaDirStat PRIVATE MEGADIRSTAT_VERSION="${PROJECT_VERSION}")
set_target_properties(MegaDirStat PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER io.github.tackme31.megadirstat
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})

# ---------------------------------------------------------------- tests
enable_testing()
add_subdirectory(tests)
