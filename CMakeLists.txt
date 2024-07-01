cmake_minimum_required(VERSION 3.28)

project(proxyloader LANGUAGES CXX VERSION 1.0.0.0)

if (NOT DEFINED PROXY_DLL)
    message(FATAL_ERROR "PROXY_DLL must be defined (e.g. -DPROXY_DLL=d3d9.dll)")
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)

option(STATIC_MSVC_RUNTIME "Static link MSVC runtime" ON)

if (STATIC_MSVC_RUNTIME)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(cmake/get_cpm.cmake)

CPMAddPackage(
    NAME               zydis
    GIT_TAG            v4.0.0
    GITHUB_REPOSITORY  zyantific/zydis
    OPTIONS            "ZYDIS_MINIMAL_MODE ON" "ZYDIS_FEATURE_ENCODER OFF" "ZYDIS_FEATURE_FORMATTER OFF"
                       "ZYDIS_FEATURE_AVX512 OFF" "ZYDIS_FEATURE_KNC OFF" "ZYDIS_FEATURE_SEGMENT OFF"
)

CPMAddPackage(
    NAME               safetyhook
    GIT_TAG            v0.4.1
    GITHUB_REPOSITORY  cursey/safetyhook
)

CPMAddPackage(
    NAME              perfect-dll-proxy
    GIT_TAG           5309475d0885129778da129d3926af3387447ffe
    GITHUB_REPOSITORY mrexodia/perfect-dll-proxy
)

set(VENV_DIR ${CMAKE_BINARY_DIR}/venv)

if (NOT EXISTS ${VENV_DIR})
    execute_process(COMMAND "${Python3_EXECUTABLE}" -m venv ${VENV_DIR})
endif()

set(ENV{VIRTUAL_ENV} ${VENV_DIR})
set(Python3_FIND_VIRTUALENV FIRST)
unset(Python3_EXECUTABLE)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

if (NOT EXISTS ${VENV_DIR}/Lib/site-packages/pefile.py)
    execute_process(COMMAND "${Python3_EXECUTABLE}" -m pip install pefile)
endif()

execute_process(
    COMMAND "${Python3_EXECUTABLE}" "perfect-dll-proxy.py"
        "--output=${CMAKE_CURRENT_BINARY_DIR}/exports.cc_" "${PROXY_DLL}"
    WORKING_DIRECTORY "${perfect-dll-proxy_SOURCE_DIR}"
)

execute_process(
    COMMAND "${Python3_EXECUTABLE}" -c
        "import sys; print(sys.stdin.read().split('BOOL WINAPI DllMain')[0], end='')"
    INPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/exports.cc_"
    OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/exports.cc"
)

add_library(proxyloader SHARED
    ${CMAKE_SOURCE_DIR}/src/loader.cc
    ${CMAKE_CURRENT_BINARY_DIR}/exports.cc
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)
target_link_libraries(${PROJECT_NAME} PRIVATE safetyhook::safetyhook)

get_filename_component(PROXY_DLL_NAME ${PROXY_DLL} NAME_WE)
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${PROXY_DLL_NAME})