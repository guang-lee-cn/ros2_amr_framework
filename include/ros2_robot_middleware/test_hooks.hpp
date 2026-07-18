/// @file   test_hooks.hpp
/// @brief  Test-only instrumentation macros.
///
/// Usage: wrap any API that should only compile in test builds.
/// Prefer normal public APIs whenever possible — this is a last resort.
///
///   AMR_TEST_ONLY( int debug_counter; )                    // member variable
///   AMR_TEST_ONLY( friend class FusionTest; )              // friend declaration
///   AMR_TEST_ONLY( void inject_fault(Fault f); )           // method declaration
///
/// Gate: AMR_TEST_HOOKS is defined by CMake for test targets only.
///       Source files compiled as part of robot_middleware_lib also need it
///       if they implement AMR_TEST_ONLY-declared methods.

#pragma once

#ifdef AMR_TEST_HOOKS
#define AMR_TEST_ONLY(code) code
#else
#define AMR_TEST_ONLY(code)
#endif
