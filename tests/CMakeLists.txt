# Every test links Core and Mock only -- never the MEGA SDK -- so the suite
# builds and runs without the SDK or a network.
function(megadirstat_add_test name)
    qt_add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE
        MegaDirStatCore MegaDirStatMock Qt6::Test MegaDirStatWarnings)
    target_compile_definitions(${name} PRIVATE
        MEGADIRSTAT_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
    add_test(NAME ${name} COMMAND ${name})
    if(WIN32)
        # Qt's DLLs are not next to the test binaries outside Qt Creator.
        set_tests_properties(${name} PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
    endif()
endfunction()

megadirstat_add_test(tst_sizenode tst_sizenode.cpp)
megadirstat_add_test(tst_treemaplayout tst_treemaplayout.cpp)
megadirstat_add_test(tst_mock tst_mock.cpp)
megadirstat_add_test(tst_runcachedir tst_runcachedir.cpp)
