# Compile-time test for Wt::Dbo C++26 reflection infrastructure
add_executable(dbo-reflect-test reflect_test.C)
target_link_libraries(dbo-reflect-test PRIVATE wtdbo)
target_include_directories(dbo-reflect-test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
