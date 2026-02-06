cmake_minimum_required(VERSION 3.28)

project (iamf)
message(STATUS "Start build libiamf")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
if(NOT MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror=unused-variable")
endif()
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(DEP_CODECS_DIR        "${PROJECT_SOURCE_DIR}/dep_codecs")
set(DEP_EXTERNAL_DIR      "${PROJECT_SOURCE_DIR}/dep_external")

option(IAMF_BUILD_SHARED_LIB "Build shared library" ON)
option(IAMF_ENABLE_BINAURALIZER "Enable binaural rendering"  ON)
option(ENABLE_BUILD_CODECS "Build all codecs" ON)
option(IAMF_TEST_TOOL "IAMF test tool" OFF)
option(SUPPORT_VERIFIER "Output vlogging file" OFF)

if (SUPPORT_VERIFIER)
    message(STATUS "Output vlogging file")
    add_definitions(-DSUPPORT_VERIFIER=1)
endif()

if (CMAKE_BUILD_TYPE)
    string(TOLOWER ${CMAKE_BUILD_TYPE} BUILD_TYPE)
    if ("${BUILD_TYPE}" STREQUAL "debug")
        add_definitions(-DIA_DBG)
        message(STATUS "Debug mode")
    endif()
endif()

if (ENABLE_BUILD_CODECS)
    add_subdirectory(dep_codecs)
    add_definitions(-DCONFIG_OPUS_CODEC -DCONFIG_AAC_CODEC -DCONFIG_FLAC_CODEC)
    if(MSVC)
        add_definitions(-DFLAC__NO_DLL)
    endif()
else()
    unset(find_opus CACHE)
    find_library(find_opus NAMES opus PATHS ${DEP_CODECS_DIR}/lib NO_DEFAULT_PATH)
    if(${find_opus} STREQUAL "find_opus-NOTFOUND")
        message(STATUS "The opus library was not found")
    else()
        add_definitions(-DCONFIG_OPUS_CODEC)
    endif()

    unset(find_aac CACHE)
    find_library(find_aac NAMES fdk-aac PATHS ${DEP_CODECS_DIR}/lib NO_DEFAULT_PATH)
    if(${find_aac} STREQUAL "find_aac-NOTFOUND")
        message(STATUS "The aac library was not found")
    else()
        add_definitions(-DCONFIG_AAC_CODEC)
    endif()

    unset(find_flac CACHE)
    find_library(find_flac NAMES FLAC PATHS ${DEP_CODECS_DIR}/lib NO_DEFAULT_PATH)
    if(${find_flac} STREQUAL "find_flac-NOTFOUND")
        message(STATUS "The flac library was not found")
    else()
        add_definitions(-DCONFIG_FLAC_CODEC)
        if(MSVC)
            add_definitions(-DFLAC__NO_DLL)
        endif()
    endif()
endif()

if (IAMF_ENABLE_BINAURALIZER)
    set(OAR_ENABLE_BINAURALIZER ON)
else()
    set(OAR_ENABLE_BINAURALIZER OFF)
endif()
set(OAR_BUILD_SHARED_LIBRARY OFF)
add_subdirectory(dep_external/src/oar-private/liboar)

file(GLOB_RECURSE sources  CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/dep_external/src/wav/*.c")

include_directories(
    include
    ${DEP_CODECS_DIR}/include
    ${DEP_EXTERNAL_DIR}/include
    ${DEP_EXTERNAL_DIR}/src/oar-private/liboar/include
    ${DEP_EXTERNAL_DIR}/src/oar-private/liboar/src/common
    ${PROJECT_SOURCE_DIR}/src/iamf_dec
    ${PROJECT_SOURCE_DIR}/src/iamf_dec/codec
    ${PROJECT_SOURCE_DIR}/src/iamf_dec/codec/opus
    ${PROJECT_SOURCE_DIR}/src/iamf_dec/codec/aac
    ${PROJECT_SOURCE_DIR}/src/iamf_dec/codec/flac
    ${PROJECT_SOURCE_DIR}/src/iamf_dec/common
    ${PROJECT_SOURCE_DIR}/src/iamf_dec/obu
)

link_directories (
    ${DEP_CODECS_DIR}/lib
)

if(IAMF_BUILD_SHARED_LIB)
    add_library(${PROJECT_NAME} SHARED ${sources})
else()
    add_library(${PROJECT_NAME} STATIC ${sources})
endif()
if (ENABLE_BUILD_CODECS)
    add_dependencies(${PROJECT_NAME} dep_opus dep_aac dep_flac)
    target_link_libraries (${PROJECT_NAME} opus fdk-aac FLAC) 
else()
    if(NOT ${find_opus} STREQUAL "find_opus-NOTFOUND")
        target_link_libraries (${PROJECT_NAME} opus)
    endif()
    if(NOT ${find_aac} STREQUAL "find_aac-NOTFOUND")
        target_link_libraries (${PROJECT_NAME} fdk-aac)
    endif()
    if(NOT ${find_flac} STREQUAL "find_flac-NOTFOUND")
        target_link_libraries (${PROJECT_NAME} FLAC)
    endif()
endif()
if(NOT MSVC)
    target_link_libraries (${PROJECT_NAME} m)
endif()
target_link_libraries (${PROJECT_NAME} oar)

if(IAMF_TEST_TOOL)
    add_subdirectory(test/tools/iamfdec)
endif()

file(GLOB iamf_public_headers  CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*")
set_target_properties(iamf PROPERTIES PUBLIC_HEADER "${iamf_public_headers}")

set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix ${CMAKE_INSTALL_PREFIX})
set(libdir ${CMAKE_INSTALL_PREFIX}/lib)
set(includedir ${CMAKE_INSTALL_PREFIX}/include)
configure_file(iamf.pc.in iamf.pc)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/iamf.pc
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)

if(IAMF_BUILD_SHARED_LIB)
    install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include/iamf)
else()
    install(TARGETS ${PROJECT_NAME}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include/iamf)
endif()

