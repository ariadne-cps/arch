# Compile the experimental suite, but never run SPRE22 from CI.
set(ARCH_EXAMPLES
    ROBE25
    CVDP23
    LALO20
    LOVO25
    SPRE22
    arch_suite
)
foreach(example IN LISTS ARCH_EXAMPLES)
    add_executable(${example} ${example}.cpp)
    target_link_libraries(${example} PRIVATE ariadne)
endforeach()

# Capture and replay agree on the snapshot path in any build directory.
set(SPRE22_SNAPSHOT_PATH "${CMAKE_BINARY_DIR}/spre22_plot_data.bin")
add_executable(SPRE22_plot_replay SPRE22_plot_replay.cpp)
target_link_libraries(SPRE22_plot_replay PRIVATE ariadne)
foreach(target IN ITEMS SPRE22 arch_suite SPRE22_plot_replay)
    target_compile_definitions(${target} PRIVATE
        SPRE22_CAPTURE_FILE_PATH=\"${SPRE22_SNAPSHOT_PATH}\")
endforeach()

# CI compiles and executes a fast synthetic capture/readback fixture.
add_executable(SPRE22_plot_capture_smoke SPRE22_plot_capture_smoke.cpp)
target_link_libraries(SPRE22_plot_capture_smoke PRIVATE ariadne)
set(SPRE22_SMOKE_STAMP "${CMAKE_BINARY_DIR}/spre22_plot_roundtrip_smoke.stamp")
add_custom_command(
    OUTPUT "${SPRE22_SMOKE_STAMP}"
    COMMAND $<TARGET_FILE:SPRE22_plot_capture_smoke> "${CMAKE_BINARY_DIR}/spre22_plot_smoke.bin"
    COMMAND "${CMAKE_COMMAND}" -E touch "${SPRE22_SMOKE_STAMP}"
    DEPENDS SPRE22_plot_capture_smoke
    COMMENT "Checking SPRE22 binary capture round-trip"
    VERBATIM
)
add_custom_target(SPRE22_plot_roundtrip_smoke ALL DEPENDS "${SPRE22_SMOKE_STAMP}")
