cmake_minimum_required(VERSION 3.16)
project(teqp)
enable_testing()

####  SETUP
set(CMAKE_CXX_STANDARD 17)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/externals/mcx/pymcx/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/externals/Eigen")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/externals/nlohmann_json")

set(BUILD_TESTING FALSE CACHE BOOL "Turn off Eigen tests")
set(Eigen3_DIR "${CMAKE_CURRENT_SOURCE_DIR}/externals/Eigen" CACHE INTERNAL "Path to Eigen, for autodiff")

set(AUTODIFF_BUILD_TESTS FALSE CACHE BOOL "No autodiff tests")
set(AUTODIFF_BUILD_PYTHON FALSE CACHE BOOL "No autodiff python")
set(AUTODIFF_BUILD_EXAMPLES FALSE CACHE BOOL "No autodiff examples")

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/externals/Eigen")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/externals/autodiff")

set(TEQP_NO_PYTHON false)
if (NOT TEQP_NO_PYTHON)
    add_definitions(-DUSE_AUTODIFF)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/externals/pybind11" "pybind11")
    file(GLOB_RECURSE pybind11_files "${CMAKE_CURRENT_SOURCE_DIR}/interface/*.cpp")
    pybind11_add_module(teqp "${pybind11_files}")
    target_include_directories(teqp PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/externals/pybind11_json/include")
    target_link_libraries(teqp PRIVATE autodiff)
endif()

add_executable(catch_tests "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/catch_tests.cxx")
if (MSVC)
    target_sources(catch_tests PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/externals/Eigen/debug/msvc/eigen.natvis")
endif()
target_link_libraries(catch_tests autodiff)
add_test(catch catch_tests)

### TARGETS from src folder
if (TEQP_SNIPPETS)
  # Collect all the snippets
  file(GLOB_RECURSE snippets "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

  message(STATUS "snippets found = ${snippets}")
  foreach (snippet ${snippets})

    get_filename_component(snippet_name ${snippet} NAME)
    get_filename_component(snippet_exe ${snippet} NAME_WE)
    message(STATUS "snippet_name = ${snippet_name}")

    add_executable(${snippet_exe} ${snippet})
    if (MSVC)
	    target_sources(${snippet_exe} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/externals/Eigen/debug/msvc/eigen.natvis")
	endif()
	target_link_libraries(${snippet_exe} autodiff)
  endforeach()
else()
  message(STATUS "No snippets will be compiled, pass -DTEQP_SNIPPETS=ON to build them")
endif()
