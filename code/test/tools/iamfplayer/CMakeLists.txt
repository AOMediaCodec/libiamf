cmake_minimum_required(VERSION 3.1)

project (iamfplayer)

message(status,"+++++++++++iamfplayer+++++++++++++")

include_directories (iamfplayer)
include_directories (include)
aux_source_directory (player DIR_IAMFPLAY_PLAYER)
aux_source_directory (src DIR_IAMFPLAY_SRC)

set(LIB_DIR  "${CMAKE_INSTALL_PREFIX}/lib")
set(INCLUDE_IAMF_DIR  "${CMAKE_INSTALL_PREFIX}/include/iamf")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")

message(status,${INCLUDE_IAMF_DIR})
include_directories(
    ${INCLUDE_IAMF_DIR}
)
link_directories(
  ${LIB_DIR}
)


add_executable (iamfplayer player/iamfplayer.c
    ${DIR_IAMFPLAY_PLAYER} ${DIR_IAMFPLAY_SRC})

target_link_libraries (iamfplayer iamf m)

