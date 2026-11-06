#include "rw/math/vpu/types.h"
#include "SDKs/EATech/include/rw/math/vpu/matrix44.h"
#include "rw/math/vpu/matrix44_operation.h"
typedef char full_size[(sizeof(rw::math::vpu::Matrix44)==64)?1:-1];
typedef char affine_size[(sizeof(rw::math::vpu::Matrix44Affine)==64)?1:-1];
