cmake_minimum_required(VERSION 3.16)
project(singlecycle_axi4_sim CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(ICACHE_MISS_LATENCY_VALUE 8 CACHE STRING "SimDDR latency cycles")
set(SINGLE_CYCLE_EXE single_cycle_axi4.out)
set(DEMO_STATIC_EXE demo_api_static.out)
set(DEMO_SHARED_EXE demo_api_shared.out)

set(CORE_SOURCES
    src/sc_axi4_sim_api.cpp
    src/cpu/single_cycle_cpu.cpp
    src/axi/AXI_Interconnect.cpp
)

set(COMMON_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/cpu/include
    ${CMAKE_SOURCE_DIR}/src/axi/include
    ${CMAKE_SOURCE_DIR}/src/simddr/include
)

set(COMMON_COMPILE_DEFS
    USE_SIM_DDR
    USE_SIM_DDR_AXI4
    ICACHE_MISS_LATENCY=${ICACHE_MISS_LATENCY_VALUE}
)

set(COMMON_COMPILE_OPTIONS
    -O3
    -march=native
    -funroll-loops
    -mtune=native
)

add_library(single_cycle_axi4_static STATIC ${CORE_SOURCES})
add_library(single_cycle_axi4_shared SHARED ${CORE_SOURCES})

foreach(target single_cycle_axi4_static single_cycle_axi4_shared)
  target_include_directories(${target} PUBLIC ${COMMON_INCLUDE_DIRS})
  target_compile_definitions(${target} PRIVATE ${COMMON_COMPILE_DEFS})
  target_compile_options(${target} PRIVATE ${COMMON_COMPILE_OPTIONS})
  set_target_properties(${target} PROPERTIES OUTPUT_NAME single_cycle_axi4)
endforeach()

target_link_libraries(single_cycle_axi4_static PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/softfloat/softfloat.a
    z
    stdc++fs
)

# softfloat.a is not PIC in this repository, so shared library keeps
# softfloat symbols unresolved and requires host-side symbol provision.
target_link_libraries(single_cycle_axi4_shared PRIVATE
    z
    stdc++fs
)
target_link_options(single_cycle_axi4_shared PRIVATE
    -Wl,--allow-shlib-undefined
)

add_executable(${SINGLE_CYCLE_EXE}
    src/main.cpp
    src/simddr/SimDDR.cpp
)

target_include_directories(${SINGLE_CYCLE_EXE} PRIVATE ${COMMON_INCLUDE_DIRS})
target_compile_definitions(${SINGLE_CYCLE_EXE} PRIVATE ${COMMON_COMPILE_DEFS})
target_compile_options(${SINGLE_CYCLE_EXE} PRIVATE ${COMMON_COMPILE_OPTIONS})
target_link_libraries(${SINGLE_CYCLE_EXE} PRIVATE
    single_cycle_axi4_static
    ${CMAKE_SOURCE_DIR}/third_party/softfloat/softfloat.a
    z
    stdc++fs
)

add_executable(${DEMO_STATIC_EXE}
    examples/demo_api_with_simddr.cpp
    src/simddr/SimDDR.cpp
)
target_include_directories(${DEMO_STATIC_EXE} PRIVATE ${COMMON_INCLUDE_DIRS})
target_compile_definitions(${DEMO_STATIC_EXE} PRIVATE ${COMMON_COMPILE_DEFS})
target_compile_options(${DEMO_STATIC_EXE} PRIVATE ${COMMON_COMPILE_OPTIONS})
target_link_libraries(${DEMO_STATIC_EXE} PRIVATE
    single_cycle_axi4_static
    ${CMAKE_SOURCE_DIR}/third_party/softfloat/softfloat.a
    z
    stdc++fs
)
set_target_properties(${DEMO_STATIC_EXE} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
)

add_executable(${DEMO_SHARED_EXE}
    examples/demo_api_with_simddr.cpp
    src/simddr/SimDDR.cpp
)
target_include_directories(${DEMO_SHARED_EXE} PRIVATE ${COMMON_INCLUDE_DIRS})
target_compile_definitions(${DEMO_SHARED_EXE} PRIVATE ${COMMON_COMPILE_DEFS})
target_compile_options(${DEMO_SHARED_EXE} PRIVATE ${COMMON_COMPILE_OPTIONS})
target_link_libraries(${DEMO_SHARED_EXE} PRIVATE
    single_cycle_axi4_shared
    ${CMAKE_SOURCE_DIR}/third_party/softfloat/softfloat.a
    z
    stdc++fs
)
set_target_properties(${DEMO_SHARED_EXE} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
    BUILD_RPATH "$ORIGIN/.."
)

add_custom_target(run_dhrystone
    COMMAND ${CMAKE_BINARY_DIR}/${SINGLE_CYCLE_EXE} ${CMAKE_SOURCE_DIR}/bin/dhrystone.bin
    DEPENDS ${SINGLE_CYCLE_EXE}
)

add_custom_target(run_coremark
    COMMAND ${CMAKE_BINARY_DIR}/${SINGLE_CYCLE_EXE} ${CMAKE_SOURCE_DIR}/bin/coremark.bin
    DEPENDS ${SINGLE_CYCLE_EXE}
)

add_custom_target(run_linux
    COMMAND ${CMAKE_BINARY_DIR}/${SINGLE_CYCLE_EXE} ${CMAKE_SOURCE_DIR}/bin/linux.bin
    DEPENDS ${SINGLE_CYCLE_EXE}
)
