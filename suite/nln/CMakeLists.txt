# Compile the experimental suite, but never run it from CI.
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


# Capture-mode SPRE22 emits its generated header in the build root, not the
# source tree. Replay is offered only once the snapshot exists.
foreach(target IN ITEMS SPRE22 arch_suite)
    target_compile_definitions(${target} PRIVATE
    SPRE22_CAPTURE_HEADER_PATH=\"${CMAKE_BINARY_DIR}/spre22_plot_data.generated.hpp\")
endforeach()

if(EXISTS "${CMAKE_BINARY_DIR}/spre22_plot_data.generated.hpp")
    add_executable(SPRE22_plot_replay SPRE22_plot_replay.cpp)
    target_include_directories(SPRE22_plot_replay PRIVATE "${CMAKE_BINARY_DIR}")
    target_link_libraries(SPRE22_plot_replay PRIVATE ariadne)
endif()

# Build-only smoke check: generate a small representative header containing
# both one-sided (+/- infinity) and bounded constraints, then compile it.
# No SPRE22 evolution or plotting is executed by CI.
add_executable(SPRE22_plot_capture_smoke SPRE22_plot_capture_smoke.cpp)
target_link_libraries(SPRE22_plot_capture_smoke PRIVATE ariadne)
set(SPRE22_SMOKE_HEADER "${CMAKE_BINARY_DIR}/spre22_plot_smoke_data.generated.hpp")
add_custom_command(
    OUTPUT "${SPRE22_SMOKE_HEADER}"
    COMMAND $<TARGET_FILE:SPRE22_plot_capture_smoke> "${SPRE22_SMOKE_HEADER}"
    DEPENDS SPRE22_plot_capture_smoke
    COMMENT "Generating SPRE22 constrained-image capture smoke fixture"
    VERBATIM
)
add_executable(SPRE22_plot_replay_smoke SPRE22_plot_replay_smoke.cpp "${SPRE22_SMOKE_HEADER}")
target_include_directories(SPRE22_plot_replay_smoke PRIVATE "${CMAKE_BINARY_DIR}")
target_link_libraries(SPRE22_plot_replay_smoke PRIVATE ariadne)
