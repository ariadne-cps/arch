# Compile the experimental suite, but never run it from CI.
set(ARCH_EXAMPLES
    ROBE25
    CVDP23
    LALO20
    LOVO25
    SPRE22
    SUTR21
    arch_suite
)
foreach(example IN LISTS ARCH_EXAMPLES)
    add_executable(${example} ${example}.cpp)
    target_link_libraries(${example} PRIVATE ariadne)
endforeach()

# QUAD20.cpp is preserved but not buildable: QUAD20.hpp is missing upstream.
