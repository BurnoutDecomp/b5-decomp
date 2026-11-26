#pragma once

// rwmath rw/math/vpu/vec_float_operation.h: the VecFloat operations. The SDK header declares the named
// operations and the operator set and includes their bodies from detail/vec_float_operation_inline.h
// (LegacySrc rwmath 1.02.00 vec_float_operation.h:374); the platform scalar header includes this one
// (LegacySrc ps3/ppu/scalar_operation_platform_inline.h:6), so every rwmath user sees the operators.

#include "SDKs/EATech/include/rw/math/vpu/vec_float.h"
#include "SDKs/EATech/include/rw/math/vpu/detail/vec_float_operation_inline.h"
