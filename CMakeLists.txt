cmake_minimum_required(VERSION 3.22)

project(cod2map LANGUAGES C)

option(COD2MAP_WASM "Build the Emscripten/WebAssembly browser target" OFF)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(COD2MAP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cod2src)

set(COD2MAP_SOURCES
  ${COD2MAP_DIR}/aabbtree.c
  ${COD2MAP_DIR}/brush.c
  ${COD2MAP_DIR}/bsp.c
  ${COD2MAP_DIR}/collision.c
  ${COD2MAP_DIR}/common/bspfile.c
  ${COD2MAP_DIR}/common/polylib.c
  ${COD2MAP_DIR}/common/targetplatform.c
  ${COD2MAP_DIR}/common/texturevecs.c
  ${COD2MAP_DIR}/facebsp.c
  ${COD2MAP_DIR}/leakfile.c
  ${COD2MAP_DIR}/libs/cmdlib.c
  ${COD2MAP_DIR}/libs/minizip/ioapi.c
  ${COD2MAP_DIR}/libs/minizip/unzip.c
  ${COD2MAP_DIR}/libs/zlib/adler32.c
  ${COD2MAP_DIR}/libs/zlib/compress.c
  ${COD2MAP_DIR}/libs/zlib/crc32.c
  ${COD2MAP_DIR}/libs/zlib/deflate.c
  ${COD2MAP_DIR}/libs/zlib/gzclose.c
  ${COD2MAP_DIR}/libs/zlib/gzlib.c
  ${COD2MAP_DIR}/libs/zlib/gzread.c
  ${COD2MAP_DIR}/libs/zlib/gzwrite.c
  ${COD2MAP_DIR}/libs/zlib/infback.c
  ${COD2MAP_DIR}/libs/zlib/inffast.c
  ${COD2MAP_DIR}/libs/zlib/inflate.c
  ${COD2MAP_DIR}/libs/zlib/inftrees.c
  ${COD2MAP_DIR}/libs/zlib/trees.c
  ${COD2MAP_DIR}/libs/zlib/uncompr.c
  ${COD2MAP_DIR}/libs/zlib/zutil.c
  ${COD2MAP_DIR}/lightmaps.c
  ${COD2MAP_DIR}/map.c
  ${COD2MAP_DIR}/materials.c
  ${COD2MAP_DIR}/mesh.c
  ${COD2MAP_DIR}/patch.c
  ${COD2MAP_DIR}/platform_compat.c
  ${COD2MAP_DIR}/portals.c
  ${COD2MAP_DIR}/shadows.c
  ${COD2MAP_DIR}/shadows_midpoint.c
  ${COD2MAP_DIR}/surface.c
  ${COD2MAP_DIR}/threads.c
  ${COD2MAP_DIR}/tree.c
  ${COD2MAP_DIR}/tris.c
  ${COD2MAP_DIR}/tris_coalesce.c
  ${COD2MAP_DIR}/tris_gridtree.c
  ${COD2MAP_DIR}/tris_mergeconcave.c
  ${COD2MAP_DIR}/tris_mergeverts.c
  ${COD2MAP_DIR}/tris_tesselate.c
  ${COD2MAP_DIR}/tris_tjunc.c
  ${COD2MAP_DIR}/universal/assertive.c
  ${COD2MAP_DIR}/universal/com_files.c
  ${COD2MAP_DIR}/universal/com_math.c
  ${COD2MAP_DIR}/universal/com_memory.c
  ${COD2MAP_DIR}/universal/com_shared.c
  ${COD2MAP_DIR}/universal/dvar.c
  ${COD2MAP_DIR}/universal/q_parse.c
  ${COD2MAP_DIR}/universal/q_shared.c
  ${COD2MAP_DIR}/universal/tangentspace.c
  ${COD2MAP_DIR}/vis.c
  ${COD2MAP_DIR}/win_shared.c
  ${COD2MAP_DIR}/writebsp.c
)

if(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  enable_language(ASM_MASM)
  list(APPEND COD2MAP_SOURCES ${COD2MAP_DIR}/asm/x87_trig.asm)
endif()

add_executable(cod2map ${COD2MAP_SOURCES})

target_include_directories(cod2map PRIVATE
  ${COD2MAP_DIR}
  ${COD2MAP_DIR}/libs/zlib
  ${COD2MAP_DIR}/libs/minizip
)

target_compile_definitions(cod2map PRIVATE
  _CRT_SECURE_NO_WARNINGS
  _CRT_NONSTDC_NO_DEPRECATE
)

if(MSVC)
  target_compile_options(cod2map PRIVATE /W3 /wd4244 /wd4267 /wd4305 /wd4996)
else()
  target_compile_definitions(cod2map PRIVATE HAVE_UNISTD_H)
  target_compile_options(cod2map PRIVATE -fcommon -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-sign-compare -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function -Wno-format-truncation -Wno-format-overflow -Wno-incompatible-pointer-types -Wno-error=incompatible-pointer-types)
  target_link_libraries(cod2map PRIVATE m)
endif()

if(UNIX AND NOT EMSCRIPTEN)
  find_package(Threads REQUIRED)
  target_link_libraries(cod2map PRIVATE Threads::Threads)
endif()

if(COD2MAP_WASM OR EMSCRIPTEN)
  set_target_properties(cod2map PROPERTIES OUTPUT_NAME "cod2map" SUFFIX ".js")
  target_compile_definitions(cod2map PRIVATE COD2MAP_WASM=1)
  target_link_options(cod2map PRIVATE
    "-sMODULARIZE=1"
    "-sEXPORT_NAME=createCod2Map"
    "-sEXPORTED_RUNTIME_METHODS=['FS','callMain']"
    "-sFORCE_FILESYSTEM=1"
    "-sENVIRONMENT=worker"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sEXIT_RUNTIME=1"
    "-sINVOKE_RUN=0"
  )
endif()

install(TARGETS cod2map RUNTIME DESTINATION bin)
