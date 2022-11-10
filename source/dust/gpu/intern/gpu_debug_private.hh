/* This interface allow GPU to manage GL objects for multiple context and threads. */

#pragma once

#include "LIB_string_ref.hh"
#include "LIB_vector.hh"

namespace dust::gpu {

typedef Vector<StringRef> DebugStack;

}  // namespace dust::gpu
