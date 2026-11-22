# Example programs. They link the static library; TuneUI propagates the
# public include root (tuneui headers + third-party headers such as GLFW).

add_executable(tuneui_platform_test platform_test/main.cpp)
add_executable(tuneui_render_test render_test/main.cpp)
add_executable(tuneui_page_test page_test/main.cpp)

foreach (example tuneui_platform_test tuneui_render_test tuneui_page_test)
    target_link_libraries(${example} PRIVATE TuneUI)

    if (MSVC)
        target_compile_options(${example} PRIVATE /W4)
    else ()
        target_compile_options(${example} PRIVATE -Wall -Wextra -Wpedantic)
    endif ()
endforeach ()

if (TUNEUI_BUILD_TESTS AND BUILD_TESTING)
    add_test(NAME tuneui_platform_test COMMAND tuneui_platform_test)
    set_tests_properties(tuneui_platform_test PROPERTIES
            LABELS requires-display TIMEOUT 180)

    add_test(NAME tuneui_render_test COMMAND tuneui_render_test)
    set_tests_properties(tuneui_render_test PROPERTIES
            LABELS requires-display TIMEOUT 180)

    # tuneui_page_test is an interactive demo; --frames keeps it finite for CTest.
    add_test(NAME tuneui_page_test_smoke COMMAND tuneui_page_test --frames 30)
    set_tests_properties(tuneui_page_test_smoke PROPERTIES
            LABELS requires-display TIMEOUT 60)
endif ()
