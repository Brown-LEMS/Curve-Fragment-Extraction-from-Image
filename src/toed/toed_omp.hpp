#ifndef TOED_OMP_HPP
#define TOED_OMP_HPP
// OpenMP is optional for the CPU TOED build. When the compiler/runtime is not
// available (e.g. Apple Clang without Homebrew libomp), fall back to serial
// stand-ins for the handful of OpenMP runtime calls used; the `#pragma omp`
// lines are then ignored by the compiler.

#ifdef _OPENMP
#include <omp.h>
#else
#include <chrono>

inline void omp_set_num_threads(int /*num_threads*/) {}
inline int omp_get_max_threads() { return 1; }
inline double omp_get_wtime()
{
    using clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}
#endif

#endif // TOED_OMP_HPP
