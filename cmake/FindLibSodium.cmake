find_path(LIBSODIUM_INCLUDE_DIRS 
    NAMES sodium.h
    HINTS 
        /usr/local/include
        /usr/include
        /opt/local/include
        $ENV{QUICKFIX_ROOT}/include
        ${QUICKFIX_ROOT}/include
    PATH_SUFFIXES
        sodium
)

find_library(LIBSODIUM_LIBRARIES
    NAMES sodium libsodium
    HINTS
        /usr/local/lib64
        /usr/lib64
        /usr/local/lib
        /usr/lib
        $ENV{QUICKFIX_ROOT}/lib
        ${QUICKFIX_ROOT}/lib
    PATH_SUFFIXES
        x86_64-linux-gnu
        lib64
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibSodium
    REQUIRED_VARS
        LIBSODIUM_LIBRARIES
        LIBSODIUM_INCLUDE_DIRS
)

if(LibSodium_FOUND)
    set(LIBSODIUM_LIBRARIES ${LIBSODIUM_LIBRARIES})
    set(LIBSODIUM_INCLUDE_DIRS ${LIBSODIUM_INCLUDE_DIRS})
endif()

if(NOT TARGET LibSodium::sodium)
    add_library(LibSodium::sodium UNKNOWN IMPORTED)
endif()

set_target_properties(LibSodium::sodium PROPERTIES
    IMPORTED_LOCATION "${LIBSODIUM_LIBRARIES}"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBSODIUM_INCLUDE_DIRS}"
)

message(STATUS "Libsodium found at: ${LIBSODIUM_LIBRARIES}")