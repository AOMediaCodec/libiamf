cmake_minimum_required(VERSION 3.1)

project (iamfplayer)

message(status,"+++++++++++iamfplayer+++++++++++++")
add_definitions (-O2)

include_directories (iamfplayer)
include_directories (include)
aux_source_directory (player DIR_IAMFPLAY_PLAYER)
aux_source_directory (src DIR_IAMFPLAY_SRC)

set(LIB_DIR  "${CMAKE_INSTALL_PREFIX}/lib")
set(INCLUDE_IAMF_DIR  "${CMAKE_INSTALL_PREFIX}/include/iamf")
#add_definitions (-O2)
set(CMAKE_CXX_FLAGS "-fpermissive -std=c++0x")

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

