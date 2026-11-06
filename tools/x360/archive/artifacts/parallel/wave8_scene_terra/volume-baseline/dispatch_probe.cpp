#include "SDKs/EATech/rwcollision/volume_debug_access.h"
extern "C" int dispatch(const rw::collision::Volume* v, const Matrix44Affine* t, rw::collision::AABBox& b) { return v->GetBBox(t,1,b); }
