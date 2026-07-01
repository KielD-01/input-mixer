set(RUST_TARGET_DIR "${CMAKE_SOURCE_DIR}/target")
set(RUST_PROFILE "release" CACHE STRING "Cargo profile (debug/release)")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(RUST_PROFILE "debug")
    set(RUST_BUILD_FLAG "")
else()
    set(RUST_PROFILE "release")
    set(RUST_BUILD_FLAG "--release")
endif()

if(APPLE)
    set(RUST_LIB_NAME "libinput_mixer_core.a")
elseif(WIN32)
    set(RUST_LIB_NAME "input_mixer_core.lib")
else()
    set(RUST_LIB_NAME "libinput_mixer_core.a")
endif()

set(RUST_LIB_PATH "${RUST_TARGET_DIR}/${RUST_PROFILE}/${RUST_LIB_NAME}")

function(collect_rust_native_libs OUT_VAR PROFILE)
    file(GLOB_RECURSE _rust_libs
        "${RUST_TARGET_DIR}/${PROFILE}/build/input-mixer-core-*/out/*.a"
        "${RUST_TARGET_DIR}/${PROFILE}/build/cxx-*/out/*.a"
        "${RUST_TARGET_DIR}/${PROFILE}/build/link-cplusplus-*/out/*.a"
    )
    set(${OUT_VAR} ${_rust_libs} PARENT_SCOPE)
endfunction()

function(build_rust_core)
    add_custom_target(
        input_mixer_core_rust
        ALL
        COMMAND ${CMAKE_COMMAND} -E env
            CARGO_TARGET_DIR=${RUST_TARGET_DIR}
            cargo build ${RUST_BUILD_FLAG}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Building input-mixer-core (Rust)"
        VERBATIM
    )

    collect_rust_native_libs(RUST_NATIVE_LIBS ${RUST_PROFILE})

    add_library(input_mixer_core STATIC IMPORTED GLOBAL)
    add_dependencies(input_mixer_core input_mixer_core_rust)
    set_target_properties(input_mixer_core PROPERTIES
        IMPORTED_LOCATION "${RUST_LIB_PATH}"
        INTERFACE_LINK_LIBRARIES "${RUST_NATIVE_LIBS}"
    )
endfunction()

build_rust_core()
