#include "BrnCommonTypes.h"
extern "C" __declspec(noinline) void GlassMatrixProbe(const Matrix44Affine* locator,const Matrix44Affine* body,Matrix44Affine* out)
{
 __vector4 row=locator->xAxis.mV;
 __vector4 v=__vmulfp(__vspltw(row,0),body->xAxis.mV);
 v=__vmaddfp(__vspltw(row,1),body->yAxis.mV,v);
 out->xAxis.mV=__vmaddfp(__vspltw(row,2),body->zAxis.mV,v);
 row=locator->yAxis.mV;v=__vmulfp(__vspltw(row,0),body->xAxis.mV);
 v=__vmaddfp(__vspltw(row,1),body->yAxis.mV,v);
 out->yAxis.mV=__vmaddfp(__vspltw(row,2),body->zAxis.mV,v);
 row=locator->zAxis.mV;v=__vmulfp(__vspltw(row,0),body->xAxis.mV);
 v=__vmaddfp(__vspltw(row,1),body->yAxis.mV,v);
 out->zAxis.mV=__vmaddfp(__vspltw(row,2),body->zAxis.mV,v);
 row=locator->wAxis.mV;v=__vmaddfp(__vspltw(row,0),body->xAxis.mV,body->wAxis.mV);
 v=__vmaddfp(__vspltw(row,1),body->yAxis.mV,v);
 out->wAxis.mV=__vmaddfp(__vspltw(row,2),body->zAxis.mV,v);
}
