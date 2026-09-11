cmake_minimum_required(VERSION 3.21)

project(MegaDirStat VERSION 0.1.0 LANGUAGES CXX)

# The Windows distribution is a zip you unpack and double-click: the exe, the
# DLLs and the notices all at its root. windeployqt reads this through
# QT_DEPLOY_BIN_DIR, so it flattens Qt's half too. Must precede
# qt_standard_project_setup(), whose GNUInstallDirs only fills in defaults, and
# must not be a cache variable, which an earlier configure's "bin" would beat.
if(WIN32)
    set(CMAKE_INSTALL_BINDIR ".")
endif()

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

# ---------------------------------------------------------------- MEGA SDK
# third_party/sdk (SDKlib). Its vcpkg dependencies come in through the
# CMAKE_TOOLCHAIN_FILE/VCPKG_* variables in CMakePresets.json: the SDK only
# sets vcpkg up itself when it is the top-level project. Thumbnail, preview and
# local-server features (FreeImage, FFmpeg, PDFium, libuv) are off there too --
# this app never renders a file -- which also leaves the build without DLLs.
add_subdirectory(third_party/sdk)
# Consumers see its headers as system headers (/external:I), so /W4 in our own
# targets does not report warnings from megaapi.h. Needs CMake 3.25; older ones
# ignore the property and the headers merely warn.
set_target_properties(SDKlib PROPERTIES SYSTEM TRUE)

# ---------------------------------------------------------------- core
# No Widgets, no MEGA SDK: the snapshot model, the treemap layout and the
# IAccountSource port. Everything else depends on this, never the reverse.
qt_add_library(MegaDirStatCore STATIC
    src/core/AccountSnapshot.h
    src/core/IAccountSource.h
    src/core/RunCacheDir.h src/core/RunCacheDir.cpp
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

# ---------------------------------------------------------------- mega
# The only target that links the SDK (docs/DESIGN.md §4). Its public header
# forward-declares mega::MegaApi, so nothing downstream sees <megaapi.h>.
qt_add_library(MegaDirStatMega STATIC
    src/mega/MegaAccountSource.h src/mega/MegaAccountSource.cpp
    src/mega/SdkListeners.h
)
target_link_libraries(MegaDirStatMega
    PUBLIC MegaDirStatCore
    PRIVATE MEGA::SDKlib Qt6::Concurrent MegaDirStatWarnings)

# ---------------------------------------------------------------- app
qt_add_executable(MegaDirStat WIN32 MACOSX_BUNDLE
    main.cpp
    src/ui/LoginView.h src/ui/LoginView.cpp
    src/ui/MainWindow.h src/ui/MainWindow.cpp
    src/ui/PercentBarDelegate.h src/ui/PercentBarDelegate.cpp
    src/ui/SizeTreeModel.h src/ui/SizeTreeModel.cpp
    src/ui/TreemapWidget.h src/ui/TreemapWidget.cpp
)
target_link_libraries(MegaDirStat PRIVATE
    MegaDirStatCore
    MegaDirStatMega
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

# ---------------------------------------------------------------- package
# scripts/package.ps1 drives this; a plain `cmake --build` runs none of it.
install(TARGETS MegaDirStat
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES LICENSE THIRD-PARTY-NOTICES.txt DESTINATION ${CMAKE_INSTALL_BINDIR})

# The x64-windows-mega triplet builds every vcpkg port static, so Qt is the
# only runtime dependency to deploy.
qt_generate_deploy_app_script(
    TARGET MegaDirStat
    OUTPUT_SCRIPT MEGADIRSTAT_DEPLOY_SCRIPT
    NO_TRANSLATIONS
    NO_COMPILER_RUNTIME
    NO_UNSUPPORTED_PLATFORM_ERROR)
install(SCRIPT ${MEGADIRSTAT_DEPLOY_SCRIPT})

# windeployqt finds the MSVC redistributable only through %VCINSTALLDIR%, which
# an ordinary shell does not set, and would then ship none; this module uses
# the toolchain CMake configured with. It leaves the Debug CRT out, which may
# not be redistributed.
set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ${CMAKE_INSTALL_BINDIR})
include(InstallRequiredSystemLibraries)

# CPack captures its variables at include() time, so this stays last.
set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_NAME MegaDirStat)
set(CPACK_PACKAGE_VENDOR "Takumi Yamada")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "WinDirStat for MEGA cloud storage")
if(WIN32)
    set(CPACK_PACKAGE_FILE_NAME "MegaDirStat-${PROJECT_VERSION}-win64")
endif()
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/package")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_VERBATIM_VARIABLES ON)
include(CPack)
