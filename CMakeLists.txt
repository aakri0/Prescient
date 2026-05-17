cmake_minimum_required(VERSION 3.20)
# C is enabled because LLVMConfig.cmake runs C-language compiler checks
# (FindFFI / FindTerminfo) during find_package(LLVM).
project(IRComplexityEstimator LANGUAGES C CXX)

# --- C++ standard ---------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- options --------------------------------------------------------------
option(ENABLE_ASSERTIONS "Enable assertions in the built passes" OFF)

# --- locate LLVM 17 (out-of-tree build) -----------------------------------
find_package(LLVM 17 REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")

# LLVM_DEFINITIONS holds -D flags (e.g. -D__STDC_CONSTANT_MACROS); add_definitions
# accepts the flag form, whereas add_compile_definitions expects bare names.
add_definitions(${LLVM_DEFINITIONS})

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# LLVM is normally built without RTTI; match it to avoid link errors.
if(NOT LLVM_ENABLE_RTTI)
  add_compile_options(-fno-rtti)
endif()

if(ENABLE_ASSERTIONS)
  add_compile_definitions(IRCE_ENABLE_ASSERTIONS)
endif()

# --- the loadable plugin --------------------------------------------------
add_library(IRComplexityEstimator MODULE
  src/passes/IRComplexityPass.cpp
  src/passes/AdaptivePipeline.cpp
  src/timing/PassTimingWrapper.cpp)

# A -load-pass-plugin plugin is dlopen'd into opt, which already provides
# every LLVM symbol. The plugin must therefore NOT link the LLVM component
# libraries: doing so links LLVM's command-line options into the plugin a
# second time and aborts opt with "Option registered more than once".
# Symbols resolve from the host process at load time.
#
# The component set the implementation depends on (resolved at runtime):
#   Core Support Analysis TransformUtils Passes
llvm_map_components_to_libnames(LLVM_COMPONENT_LIBS
  Core Support Analysis TransformUtils Passes)

# Produce exactly "IRComplexityEstimator.so" (no "lib" prefix).
set_target_properties(IRComplexityEstimator PROPERTIES
  PREFIX ""
  SUFFIX ".so")
