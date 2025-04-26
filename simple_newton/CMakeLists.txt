set(SRCS main.cpp)
set(INCL include/main.hpp include/BS_thread_pool.hpp)

add_executable(simple_newton ${SRCS} ${INCL})
target_include_directories(simple_newton PRIVATE include)
target_link_libraries(simple_newton PRIVATE SDL3::SDL3 glm::glm)
