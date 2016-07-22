
add_definitions(--std=c++11)
add_executable(main main.cpp)
target_link_libraries(main pthread)

add_executable(dump dump.cpp)
target_link_libraries(dump pthread)