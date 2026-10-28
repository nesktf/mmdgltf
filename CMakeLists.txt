cmake_minimum_required(VERSION 3.25)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(MMDGLTF_VER_MAJ 0)
set(MMDGLTF_VER_MIN 0)
set(MMDGLTF_VER_REV 1)

set(MMDGLTF_VER "${MMDGLTF_VER_MAJ}.${MMDGLTF_VER_MIN}.${MMDGLTF_VER_REV}")

option(MMDGLTF_SHARED_BUILD "Shared object build" OFF)
option(MMDGLTF_BUILD_TESTS "Build tests" OFF)

set(MMDGLTF_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(MMDGLTF_SOURCE_DIR "${MMDGLTF_DIR}/src")
set(MMDGLTF_INCLUDE_DIR "${MMDGLTF_DIR}/include")

if (MMDGLTF_SHARED_BUILD)
  set(MMDGLTF_BUILD_TYPE SHARED)
else()
  set(MMDGLTF_BUILD_TYPE STATIC)
endif()

message(STATUS "mmdgltf: Setting up version ${MMDGLTF_VER_STR}")
project(mmdgltf VERSION ${MMDGLTF_VER} LANGUAGES C)

set(MMDGLTF_SOURCES
  "${MMDGLTF_SOURCE_DIR}/core.c"
  "${MMDGLTF_SOURCE_DIR}/arena.c"
  "${MMDGLTF_SOURCE_DIR}/stream.c"
  "${MMDGLTF_SOURCE_DIR}/pmx.c"
  "${MMDGLTF_SOURCE_DIR}/model.c"
  "${MMDGLTF_SOURCE_DIR}/image.c"
  "${MMDGLTF_SOURCE_DIR}/tinygltf/tiny_gltf_v3.c"
  "${MMDGLTF_SOURCE_DIR}/export.c"
  "${MMDGLTF_SOURCE_DIR}/fastpath.c"
)
list(APPEND MMDGLTF_INCLUDE ${MMDGLTF_INCLUDE_DIR})
set(MMDGLTF_HEADERS "${MMDGLTF_INCLUDE_DIR}/mmdgltf/mmdgltf.h")

add_library(${PROJECT_NAME} ${MMDGLTF_BUILD_TYPE})
target_sources(${PROJECT_NAME} PRIVATE ${MMDGLTF_SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC ${MMDGLTF_INCLUDE} PRIVATE ${MMDGLTF_SOURCE_DIR})
target_link_libraries(${PROJECT_NAME} ${MMDGLTF_LINK})
if (UNIX AND NOT APPLE)
  target_link_libraries(${PROJECT_NAME} PRIVATE m)
endif()

if (MMDGLTF_SHARED_BUILD)
  list(APPEND MMDGLTF_DEFS -DMMDGLTF_SHARED_BUILD)
endif()
list(APPEND MMDGLTF_DEFS -DTINYGLTF3_ENABLE_FS)
target_compile_definitions(${PROJECT_NAME} PUBLIC ${MMDGLTF_DEFS})

set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX "d")
set_property(TARGET mmdgltf PROPERTY C_STANDARD 11)

if (MMDGLTF_BUILD_TESTS)
  message(STATUS "mmdgltf: Building tests")
  add_subdirectory("${MMDGLTF_DIR}/test")
endif()
