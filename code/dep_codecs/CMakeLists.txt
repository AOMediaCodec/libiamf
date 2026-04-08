cmake_minimum_required(VERSION 3.28)

project (dep_codecs)

# Fetch opus library
include(ExternalProject)
ExternalProject_Add(
    dep_opus
    URL https://downloads.xiph.org/releases/opus/opus-1.4.tar.gz
    CMAKE_ARGS -DBUILD_SHARED_LIBS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=${DEP_CODECS_DIR} -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
)

# Fetch fdk-aac(2.0.3) library
include(ExternalProject)
ExternalProject_Add(
    dep_aac
    GIT_REPOSITORY https://github.com/mstorsjo/fdk-aac.git
    GIT_TAG 716f4394641d53f0d79c9ddac3fa93b03a49f278
    PATCH_COMMAND ${CMAKE_COMMAND} -E make_directory <SOURCE_DIR>/libSBRdec/include/log && ${CMAKE_COMMAND} -E echo "void android_errorWriteLog(int i, const char *string){}" > <SOURCE_DIR>/libSBRdec/include/log/log.h
    CMAKE_ARGS -DBUILD_SHARED_LIBS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=${DEP_CODECS_DIR} -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
)

# Fetch flac library
include(ExternalProject)
ExternalProject_Add(
    dep_flac
    URL https://downloads.xiph.org/releases/flac/flac-1.4.2.tar.xz
    CMAKE_ARGS -DBUILD_SHARED_LIBS=OFF -DWITH_OGG=OFF -DBUILD_CXXLIBS=OFF -DINSTALL_MANPAGES=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=${DEP_CODECS_DIR} -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
)
