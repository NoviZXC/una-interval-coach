# Interval Coach host tests (QA.md section A).
# Reuses the SDK's GoogleTest bootstrap and kernel test doubles by path; the
# SDK itself is not modified.
cmake_minimum_required(VERSION 3.21)
project(interval-coach-host-tests CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED ENV{UNA_SDK})
    message(FATAL_ERROR "UNA_SDK environment variable not set")
endif()
file(TO_CMAKE_PATH "$ENV{UNA_SDK}" UNA_SDK_ROOT)
set(APP_LIBS "${CMAKE_CURRENT_LIST_DIR}/../../Software/Libs")

include(${UNA_SDK_ROOT}/Tests/Host/cmake/FetchGoogleTest.cmake)

# SDK code the settings tests need: kernel test doubles, logger, JSON.
# Built without -Werror: SDK sources are not ours to fix.
set(COREJSON_DIR "${UNA_SDK_ROOT}/ThirdParty/coreJSON/source")
add_library(sdk_support STATIC
    ${UNA_SDK_ROOT}/Tests/Host/support/KernelTestDoubles.cpp
    ${UNA_SDK_ROOT}/Libs/Source/UnaLogger/Logger.cpp
    ${UNA_SDK_ROOT}/Libs/Source/JSON/JsonStreamReader.cpp
    ${UNA_SDK_ROOT}/Libs/Source/JSON/JsonStreamWriter.cpp
    ${UNA_SDK_ROOT}/Libs/Source/Wrappers/StdLibWrappers.c
    ${COREJSON_DIR}/core_json.c
)
target_include_directories(sdk_support PUBLIC
    ${UNA_SDK_ROOT}/Tests/Host/support
    ${UNA_SDK_ROOT}/Libs/Header
    ${COREJSON_DIR}/include
)

add_executable(ic-host-tests
    IntervalEngine_test.cpp
    SettingsStore_test.cpp
    ${APP_LIBS}/Sources/IntervalEngine.cpp
    ${APP_LIBS}/Sources/SettingsStore.cpp
)
target_include_directories(ic-host-tests PRIVATE ${APP_LIBS}/Header)
target_link_libraries(ic-host-tests PRIVATE sdk_support GTest::gtest_main)

if(MSVC)
    target_compile_options(ic-host-tests PRIVATE /W4 /WX /utf-8)
else()
    target_compile_options(ic-host-tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# TouchGFX's bundled MinGW (i686, msvcrt) ships a libmingw32 whose UCRT exit
# wrappers import quick_exit/_Exit, which msvcrt lacks; every GoogleTest
# binary fails to link without this shim. Test-only, never in the .uapp.
if(MINGW AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    target_sources(ic-host-tests PRIVATE support/mingw_crt_shim.c)
endif()

enable_testing()
add_test(NAME ic-host-tests COMMAND ic-host-tests)
