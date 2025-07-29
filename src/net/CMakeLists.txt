add_library(netlib
    fix_engine.cpp
    ed25519_signer.cpp
    fix_parser.cpp
)

target_include_directories(netlib PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>)

#Link LibSoidum and QuickFIX
target_link_libraries(netlib
    QuickFIX::QuickFIX
#    LibSodium::sodium
    OpenSSL::SSL
)

find_package(QuickFIX REQUIRED)
#find_package(LibSodium REQUIRED)
find_package(OpenSSL REQUIRED)

if(NOT TARGET QuickFIX::QuickFIX)
    target_link_libraries(netlib PUBLIC ${QUICKFIX_LIBRARIES})
    target_include_directories(netlib PUBLIC ${QUICKFIX_INCLUDE_DIRS})
endif()

#if(NOT TARGET LibSodium::sodium)
#    target_link_libraries(netlib PUBLIC ${LIBSODIUM_LIBRARIES})
#    target_include_directories(netlib PUBLIC ${LIBSODIUM_INCLUDE_DIRS})
#endif()

if(NOT TARGET OpenSSL::SSL)
    target_link_libraries(netlib PUBLIC ${OPENSSL_SSL_LIBRARIES})
    target_include_directories(netlib PUBLIC ${OPENSSL_INCLUDE_DIR})
endif()

target_compile_options(netlib PUBLIC
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wpedantic -Wextra -Wformat=2>    
)

set_target_properties(netlib PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

message(STATUS "========================================")
message(STATUS "NetLib Configuration Summary")
message(STATUS "========================================")
message(STATUS "QuickFIX found: ${QuickFIX_FOUND}")
#message(STATUS "LibSodium found: ${LibSodium_FOUND}")
message(STATUS "OpenSSL found: ${OpenSSL_FOUND}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "========================================")