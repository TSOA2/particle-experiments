set(SRCS main.cpp)
set(INCL include/main.hpp)

add_executable(gpu_newton ${SRCS} ${INCL})
target_include_directories(gpu_newton PRIVATE include)
target_link_libraries(gpu_newton PRIVATE SDL3::SDL3 glm::glm)
