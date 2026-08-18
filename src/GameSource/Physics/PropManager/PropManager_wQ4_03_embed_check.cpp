// PropManager_wQ4_03_embed_check.cpp -- the layout oracle for PropManager_wQ4_03.cpp's opaque
// rw::collision::AABBox image (that TU cannot include AABBox.hpp: SDK-vs-vendor rw::math::vpu::
// Vector3 double definition, see its banner). This TU CAN, and pins the three facts the reader
// depends on. If AABBox ever changes shape, this breaks the compile gate rather than the reader.
#include "vendor/renderware/collision/AABBox.hpp"
#include <cstddef>
static_assert( sizeof( rw::collision::AABBox ) == 32, "AABBox is two 16-byte rows (mMin, mMax)" );
static_assert( offsetof( rw::collision::AABBox, mMin ) == 0,  "AABBox::mMin @+0x00" );
static_assert( offsetof( rw::collision::AABBox, mMax ) == 16, "AABBox::mMax @+0x10" );
static_assert( sizeof( rw::collision::AABBox::mMin ) == 16, "each row is one 16-byte float4 lane" );
