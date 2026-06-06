cmake_minimum_required(VERSION 3.14)

set(PROJECT_NAME netto-brutto-rechner)
project(${PROJECT_NAME} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# yaml-cpp: fetched automatically at configure time so no manual install is
# needed.  The library is used to read config_harsha.yaml (employee/month
# data) and constants_20XX.yaml (tax rates, BBG limits) without converting
# them to a different format.
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG        0.8.0
)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS  OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(yaml-cpp)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/tax.cpp
    src/sozialversicherung.cpp
    src/lohnsteuer.cpp
    src/payslip.cpp
    src/annual_summary.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE src)
target_link_libraries(${PROJECT_NAME} PRIVATE yaml-cpp::yaml-cpp)

# Place all build configurations under bin/
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY           "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG     "${CMAKE_SOURCE_DIR}/bin/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE   "${CMAKE_SOURCE_DIR}/bin/Release"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}/bin/RelWithDebInfo"
)
