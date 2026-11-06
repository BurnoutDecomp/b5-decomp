#include <new>
#include "rw/math/vpu/types.h"
#include "SDKs/EATech/include/rw/math/vpu/matrix44.h"
#include "rw/math/vpu/matrix44_operation.h"
using namespace rw::math::vpu;
typedef char m44_size[(sizeof(Matrix44)==64 && __alignof(Matrix44)==16)?1:-1];
typedef char ma_size[(sizeof(Matrix44Affine)==64 && __alignof(Matrix44Affine)==16)?1:-1];
typedef char m44_rows[(offsetof(Matrix44,xAxis)==0 && offsetof(Matrix44,yAxis)==16 && offsetof(Matrix44,zAxis)==32 && offsetof(Matrix44,wAxis)==48)?1:-1];
typedef char ma_rows[(offsetof(Matrix44Affine,xAxis)==0 && offsetof(Matrix44Affine,yAxis)==16 && offsetof(Matrix44Affine,zAxis)==32 && offsetof(Matrix44Affine,wAxis)==48)?1:-1];
__declspec(noinline) Matrix44 matrix_product(const Matrix44Affine& a,const Matrix44& b) { return Mult(a,b); }
__declspec(noinline) Matrix44 copy_full(Matrix44 m) { return m; }
__declspec(noinline) Matrix44Affine copy_affine(Matrix44Affine m) { return m; }
__declspec(noinline) void default_full(Matrix44* p) { new (p) Matrix44(); }
__declspec(noinline) void default_affine(Matrix44Affine* p) { new (p) Matrix44Affine(); }
