cmake_minimum_required(VERSION 3.15)

# CPU-only Third-Order Edge Detector (TOED), ported from the standalone TOED
# repo (makefile.cpu / mac-build.sh). Builds:
#   toed_cpu - static library with ThirdOrderEdgeDetectionCPU, used by
#              toed_compute_curve_frags
#   TOED     - the standalone detector (image -> TOED_edges.{txt,edg})
project(ThirdOrderEdgeDetector CXX)

# C++17 and C++20 both supported; override with -DCMAKE_CXX_STANDARD=20
set(CMAKE_CXX_STANDARD 17 CACHE STRING "C++ standard (17 or 20)")
set_property(CACHE CMAKE_CXX_STANDARD PROPERTY STRINGS 17 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV 4 REQUIRED)

# OpenMP is optional (toed_omp.hpp falls back to serial). Apple Clang has no
# bundled OpenMP runtime, so point FindOpenMP at Homebrew's libomp if present.
# Using Homebrew's libomp (rather than e.g. LLVM's bundled copy) also keeps us
# on the same runtime as a Homebrew OpenCV/OpenBLAS, avoiding
# "OMP: Error #15" from two OpenMP runtimes being loaded at once.
if(APPLE AND NOT DEFINED OpenMP_ROOT AND NOT DEFINED ENV{OpenMP_ROOT})
    find_program(BREW_EXECUTABLE brew)
    if(BREW_EXECUTABLE)
        execute_process(COMMAND ${BREW_EXECUTABLE} --prefix libomp
                        OUTPUT_VARIABLE TOED_LIBOMP_PREFIX
                        OUTPUT_STRIP_TRAILING_WHITESPACE
                        RESULT_VARIABLE TOED_BREW_RESULT
                        ERROR_QUIET)
        if(TOED_BREW_RESULT EQUAL 0 AND EXISTS "${TOED_LIBOMP_PREFIX}/include/omp.h")
            set(OpenMP_ROOT "${TOED_LIBOMP_PREFIX}")
        endif()
    endif()
endif()

find_package(OpenMP COMPONENTS CXX)
if(OpenMP_CXX_FOUND)
    message(STATUS "TOED: building with OpenMP ${OpenMP_CXX_VERSION}")
else()
    message(WARNING "TOED: OpenMP not found, the third-order edge detector "
                    "will run single-threaded (on macOS: brew install libomp)")
endif()

add_library(toed_cpu STATIC cpu_toed.cpp)
target_include_directories(toed_cpu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
target_link_libraries(toed_cpu PUBLIC ${OpenCV_LIBS})
if(OpenMP_CXX_FOUND)
    # PUBLIC so consumers see _OPENMP and get the real omp.h via toed_omp.hpp
    target_link_libraries(toed_cpu PUBLIC OpenMP::OpenMP_CXX)
endif()

# The convolution is far too slow unoptimized; the original makefile always
# used -O3, so do the same when no build type was chosen.
if(NOT MSVC)
    target_compile_options(toed_cpu PRIVATE $<$<CONFIG:>:-O3>)
endif()

add_executable(TOED main_cpu_DP.cpp)
target_link_libraries(TOED PRIVATE toed_cpu)
if(NOT MSVC)
    target_compile_options(TOED PRIVATE $<$<CONFIG:>:-O3>)
endif()
