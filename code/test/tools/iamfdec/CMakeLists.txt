cmake_minimum_required(VERSION 3.28)

project (iamfdec)

message(STATUS "+++++++++++iamfdec+++++++++++++")

option(SUPPORT_VERIFIER "Output vlogging file" OFF)

if(NOT MSVC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror=unused-variable")
endif()

set(LIB_DIR                   "${CMAKE_INSTALL_PREFIX}/lib")
set(IAMF_INCLUDE_DIR          "${CMAKE_INSTALL_PREFIX}/include/iamf")
set(EXTERNAL_WAV_INCLUDE_DIR  "../../../dep_external/include/wav")

if(SUPPORT_VERIFIER)
  message(STATUS "Output vlogging file")
  add_definitions(-DSUPPORT_VERIFIER=1)
endif()


file(GLOB_RECURSE sources  CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

include_directories(
    include
    ${IAMF_INCLUDE_DIR}
    ${EXTERNAL_WAV_INCLUDE_DIR}
)

link_directories(
  ${LIB_DIR}
)
 
add_executable (iamfdec ${sources})

target_link_libraries (iamfdec iamf)
if(NOT MSVC)
  target_link_libraries (iamfdec m)
endif()
