# GoogleTest is provided by FetchContent in the top-level CMakeLists.txt.
# GTest::gtest_main brings both the library and its include directories.

add_executable(tuneui_unit_tests unit_tests.cpp)
target_link_libraries(tuneui_unit_tests PRIVATE TuneUI GTest::gtest_main)

if (MSVC)
    target_compile_options(tuneui_unit_tests PRIVATE /W4)
else ()
    target_compile_options(tuneui_unit_tests PRIVATE -Wall -Wextra -Wpedantic)
endif ()

# One CTest entry per TEST(), with gtest's --gtest_filter support.
include(GoogleTest)
gtest_discover_tests(tuneui_unit_tests
        PROPERTIES LABELS unit)
