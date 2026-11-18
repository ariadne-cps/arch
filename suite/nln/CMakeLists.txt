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
target_compile_definitions(SPRE22 PRIVATE
    SPRE22_CAPTURE_HEADER_PATH=\"${CMAKE_BINARY_DIR}/spre22_plot_data.generated.hpp\")

if(EXISTS "${CMAKE_BINARY_DIR}/spre22_plot_data.generated.hpp")
    add_executable(SPRE22_plot_replay SPRE22_plot_replay.cpp)
    target_include_directories(SPRE22_plot_replay PRIVATE "${CMAKE_BINARY_DIR}")
    target_link_libraries(SPRE22_plot_replay PRIVATE ariadne)
endif()
