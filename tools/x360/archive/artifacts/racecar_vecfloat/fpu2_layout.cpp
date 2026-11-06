#include "GameShared/GameClasses/RenderWare/Math/RwMathVectorTemplates.h"
typedef char FpuVector2Size[(sizeof(rw::math::fpu::Vector2Template<float>)==8)?1:-1];
typedef char FpuVector2Alignment[(__alignof(rw::math::fpu::Vector2Template<float>)==4)?1:-1];
extern "C" float GlassScaleSum(const rw::math::fpu::Vector2Template<float>& v) { return v.X()+v.Y(); }
