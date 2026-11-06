#include "BrnCommonTypes.h"
#include "SDKs/EATech/include/rw/math/vpu/vec_float.h"
struct ShadowProbeInput { Vector4 boxMin, boxMax; Vector3 axles[4]; Vector3 bodyForward; Vector3 groundPos; Vector3 groundNormal; float distance; Vector3 rigidOffset; Vector3 dimensions; };
struct ShadowProbeOutput { Matrix44Affine transform; Vector4 extents; rw::math::vpu::VecFloat width, height; };
extern "C" __declspec(noinline) void ShadowProbe(const ShadowProbeInput* in, ShadowProbeOutput* out)
{
 static const __declspec(align(16)) unsigned char abAxleMask[16]={8,9,10,11,24,25,26,27,0,1,2,3,0,1,2,3};
 static const __declspec(align(16)) unsigned char abBoxMask[16]={0,1,2,3,0,1,2,3,8,9,10,11,24,25,26,27};
 const __vector4 lvBox=__vperm(in->boxMax.mV,in->boxMin.mV,__lvx(abBoxMask,0));
 const __vector4 lvAxles=__vperm(in->axles[3].mV,in->axles[0].mV,__lvx(abAxleMask,0));
 out->extents.mV=__vsldoi(lvBox,lvAxles,8);
 const __vector4 lvNormal=in->groundNormal.mV;
 const __vector4 lvNormalPerm=__vpermwi(lvNormal,0x63);
 const __vector4 lvBodyForward=in->bodyForward.mV;
 const __vector4 lvRight=__vpermwi(__vnmsubfp(__vpermwi(lvBodyForward,0x63),lvNormal,__vmulfp(lvBodyForward,lvNormalPerm)),0x63);
 out->transform.xAxis.mV=lvRight;
 out->transform.yAxis.mV=lvNormal;
 out->transform.zAxis.mV=__vpermwi(__vnmsubfp(lvNormalPerm,lvRight,__vmulfp(lvNormal,__vpermwi(lvRight,0x63))),0x63);
 out->transform.wAxis.mV=in->groundPos.mV;
 out->height.mV=__vsubfp(__vspltw(__lvlx(&in->distance,0),0),__vspltw(in->rigidOffset.mV,1));
 out->width.mV=__vspltw(in->dimensions.mV,0);
}
