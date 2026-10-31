#pragma once

#include "SDKs/EATech/include/rw/math/vpu/vector3.h"

namespace rw
{
namespace math
{
namespace vpu
{
    // DecFIGS vector3plus.h:34/59..78/130..174. One real register; self-copy
    // and assignment stay implicit for ARTIST's by-value v1/free-return contract.
    class __declspec(passinreg) Vector3Plus
    {
    public:
        typedef const Vector3Plus InParam;
        typedef Vector3Plus& OutParam;
        typedef Vector3Plus& InOutParam;

        Vector3Plus() {}
        Vector3Plus(float x, float y, float z, float plus)
        {
            VectorIntrinsic value;
            value.v[0] = x; value.v[1] = y;
            value.v[2] = z; value.v[3] = plus;
            mV = value;
        }
        explicit Vector3Plus(const float xyzplus[4])
        {
            VectorIntrinsic value;
            value.v[0] = xyzplus[0]; value.v[1] = xyzplus[1];
            value.v[2] = xyzplus[2]; value.v[3] = xyzplus[3];
            mV = value;
        }
        Vector3Plus(VectorIntrinsicInParam value) : mV(value) {}
        Vector3Plus(VecFloat::InParam x, VecFloat::InParam y,
                    VecFloat::InParam z, VecFloat::InParam plus)
        { Set(x, y, z, plus); }
        Vector3Plus(VecFloatRefIndex::InParam x, VecFloatRefIndex::InParam y,
                    VecFloatRefIndex::InParam z, VecFloatRefIndex::InParam plus)
        { Set(x, y, z, plus); }
        template<VectorAxis X, VectorAxis Y, VectorAxis Z, VectorAxis W>
        Vector3Plus(VecFloatRef<X> x, VecFloatRef<Y> y,
                    VecFloatRef<Z> z, VecFloatRef<W> plus)
        { Set(x, y, z, plus); }

        Vector3Plus& operator=(VectorIntrinsicInParam value) { mV = value; return *this; }
        operator VectorIntrinsic&() { return mV; }
        operator const VectorIntrinsic&() const { return mV; }
        void SetVector(VectorIntrinsicInParam value) { mV = value; }
        VectorIntrinsic& GetVector() { return mV; }
        const VectorIntrinsic& GetVector() const { return mV; }

        void Set(float x, float y, float z, float plus)
        {
            VectorIntrinsic value;
            value.v[0] = x; value.v[1] = y;
            value.v[2] = z; value.v[3] = plus;
            mV = value;
        }
        void Set(VecFloat::InParam x, VecFloat::InParam y,
                 VecFloat::InParam z, VecFloat::InParam plus)
        {
            VectorIntrinsic value;
            value.v[0] = x.mV.v[VectorAxisX]; value.v[1] = y.mV.v[VectorAxisY];
            value.v[2] = z.mV.v[VectorAxisZ]; value.v[3] = plus.mV.v[VectorAxisW];
            mV = value;
        }
        void Set(VecFloatRefIndex::InParam x, VecFloatRefIndex::InParam y,
                 VecFloatRefIndex::InParam z, VecFloatRefIndex::InParam plus)
        {
            VectorIntrinsic value;
            value.v[0] = x.mV.v[x.mIndex]; value.v[1] = y.mV.v[y.mIndex];
            value.v[2] = z.mV.v[z.mIndex]; value.v[3] = plus.mV.v[plus.mIndex];
            mV = value;
        }
        template<VectorAxis X, VectorAxis Y, VectorAxis Z, VectorAxis W>
        void Set(VecFloatRef<X> x, VecFloatRef<Y> y,
                 VecFloatRef<Z> z, VecFloatRef<W> plus)
        {
            VectorIntrinsic value;
            value.v[0] = x.mV.v[X]; value.v[1] = y.mV.v[Y];
            value.v[2] = z.mV.v[Z]; value.v[3] = plus.mV.v[W];
            mV = value;
        }
        void SetComponent(int i, float value) { mV.v[i] = value; }
        void SetComponent(int i, VecFloat::InParam value) { mV.v[i] = value.mV.v[VectorAxisX]; }
        void SetComponent(int i, VecFloatRefIndex::InParam value) { mV.v[i] = value.mV.v[value.mIndex]; }
        template<VectorAxis I>
        void SetComponent(int i, VecFloatRef<I> value) { mV.v[i] = value.mV.v[I]; }

        void SetX(float value) { SetComponent(0, value); }
        void SetX(VecFloat::InParam value) { mV.v[0] = value.mV.v[0]; }
        void SetX(VecFloatRefIndex::InParam value) { SetComponent(0, value); }
        template<VectorAxis I> void SetX(VecFloatRef<I> value) { SetComponent(0, value); }
        void SetY(float value) { SetComponent(1, value); }
        void SetY(VecFloat::InParam value) { mV.v[1] = value.mV.v[1]; }
        void SetY(VecFloatRefIndex::InParam value) { SetComponent(1, value); }
        template<VectorAxis I> void SetY(VecFloatRef<I> value) { SetComponent(1, value); }
        void SetZ(float value) { SetComponent(2, value); }
        void SetZ(VecFloat::InParam value) { mV.v[2] = value.mV.v[2]; }
        void SetZ(VecFloatRefIndex::InParam value) { SetComponent(2, value); }
        template<VectorAxis I> void SetZ(VecFloatRef<I> value) { SetComponent(2, value); }
        void SetPlus(float value) { SetComponent(3, value); }
        void SetPlus(VecFloat::InParam value) { mV.v[3] = value.mV.v[3]; }
        void SetPlus(VecFloatRefIndex::InParam value) { SetComponent(3, value); }
        template<VectorAxis I> void SetPlus(VecFloatRef<I> value) { SetComponent(3, value); }
        void SetZero() { mV = __vspltisw(0); }

        // The real SDK aliases the complete register as Vector3; the plus lane is
        // retained. Reconstructing a float3 here would incorrectly clear that lane.
        const Vector3& GetVector3() const { return reinterpret_cast<const Vector3&>(*this); }
        void SetVector3(Vector3::InParam value) { mV = __vrlimi(value.mV, mV, 1, 0); }

        VecFloatRefIndex GetComponent(int i) { return VecFloatRefIndex(mV, i); }
        const VecFloatRefIndex GetComponent(int i) const { return VecFloatRefIndex(const_cast<Vector3Plus*>(this)->mV, i); }
        VecFloatRefIndex operator[](int i) { return GetComponent(i); }
        const VecFloatRefIndex operator[](int i) const { return GetComponent(i); }
        VecFloatRefX GetX() { return VecFloatRefX(mV); }
        const VecFloatRefX GetX() const { return VecFloatRefX(const_cast<Vector3Plus*>(this)->mV); }
        VecFloatRefY GetY() { return VecFloatRefY(mV); }
        const VecFloatRefY GetY() const { return VecFloatRefY(const_cast<Vector3Plus*>(this)->mV); }
        VecFloatRefZ GetZ() { return VecFloatRefZ(mV); }
        const VecFloatRefZ GetZ() const { return VecFloatRefZ(const_cast<Vector3Plus*>(this)->mV); }
        VecFloatRefW GetPlus() { return VecFloatRefW(mV); }
        const VecFloatRefW GetPlus() const { return VecFloatRefW(const_cast<Vector3Plus*>(this)->mV); }
        VecFloatRefX X() { return GetX(); }
        const VecFloatRefX X() const { return GetX(); }
        VecFloatRefY Y() { return GetY(); }
        const VecFloatRefY Y() const { return GetY(); }
        VecFloatRefZ Z() { return GetZ(); }
        const VecFloatRefZ Z() const { return GetZ(); }
        VecFloatRefW UserData() { return GetPlus(); }
        const VecFloatRefW UserData() const { return GetPlus(); }

    public:
        VectorIntrinsic mV;
    };
}
}
}
