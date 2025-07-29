find_path(QUICKFIX_INCLUDE_DIRS 
    NAMES quickfix/Application.h quickfix/Message.h
    HINTS 
        /usr/local/include
        /usr/include
        /opt/local/include
        $ENV{QUICKFIX_ROOT}/include
        ${QUICKFIX_ROOT}/include
    PATH_SUFFIXES
        quickfix
)

find_library(QUICKFIX_LIBRARIES
    NAMES quickfix libquickfix
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
find_package_handle_standard_args(QuickFIX
    REQUIRED_VARS
        QUICKFIX_LIBRARIES
        QUICKFIX_INCLUDE_DIRS
)

if(QuickFIX_FOUND)
    set(QUICKFIX_LIBRARIES ${QUICKFIX_LIBRARIES})
    set(QUICKFIX_INCLUDE_DIRS ${QUICKFIX_INCLUDE_DIRS})
endif()

if(NOT TARGET QuickFIX::QuickFIX)
    add_library(QuickFIX::QuickFIX UNKNOWN IMPORTED)
endif()

set_target_properties(QuickFIX::QuickFIX PROPERTIES
    IMPORTED_LOCATION "${QUICKFIX_LIBRARIES}"
    INTERFACE_INCLUDE_DIRECTORIES "${QUICKFIX_INCLUDE_DIRS}"
)

set_property(TARGET QuickFIX::QuickFIX APPEND PROPERTY
    INTERFACE_LINK_LIBRARIES pthread
)


