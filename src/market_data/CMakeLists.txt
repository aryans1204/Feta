add_library(orderbooklib
    fix_order_book.cpp
)


target_include_directories(orderbooklib PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>)

target_compile_options(orderbooklib PUBLIC
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wpedantic -Wextra -Wformat=2>    
)

set_target_properties(orderbooklib PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)



    

