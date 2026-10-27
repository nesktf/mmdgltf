cmake_minimum_required(VERSION 3.25)
include(FetchContent)

set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "Don't update dependencies")

project(mmdgltf_test C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} $ENV{CFLAGS} -Wall -Wextra -Wpedantic -Wno-psabi")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS} -O0 -g2 -ggdb -fsanitize=address,leak")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS} -O3")

#set(TESTFILES "${CMAKE_CURRENT_SOURCE_DIR}/files")
file(GLOB MMDGLTF_TEST_SRC "${CMAKE_CURRENT_SOURCE_DIR}/*.c")

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/munit/munit.c")
  set(munit_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/munit")
else()
  message(STATUS "mmdgltf: Fetching munit contents...")
  FetchContent_Declare(
    munit
    GIT_REPOSITORY https://github.com/nemequ/munit.git
    GIT_TAG fbbdf14
  )
  FetchContent_MakeAvailable(munit)
endif()

add_executable(${PROJECT_NAME} ${MMDGLTF_TEST_SRC} ${munit_SOURCE_DIR}/munit.c)
target_include_directories(${PROJECT_NAME}
  PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../src"
  "${munit_SOURCE_DIR}"
)
target_link_libraries(${PROJECT_NAME} PRIVATE mmdgltf)
#target_compile_definitions(${PROJECT_NAME} PRIVATE -DTESTFILES=\"${TESTFILES}\")
enable_testing()

include(CheckLanguage)
check_language(CXX)
if (CMAKE_CXX_COMPILER)
  enable_language(CXX)
  add_executable(mmdgltf_cpp_test cpp_test.cpp)
  set_target_properties(mmdgltf_cpp_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
  target_include_directories(mmdgltf_cpp_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
  target_link_libraries(mmdgltf_cpp_test PRIVATE mmdgltf)
  add_test(NAME cpp_test COMMAND mmdgltf_cpp_test)
endif()
