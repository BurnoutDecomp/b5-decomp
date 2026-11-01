#ifndef BRN_STATIC_SOUND_MAP_H
#define BRN_STATIC_SOUND_MAP_H

#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/RenderWare/Math/RwMathVectorTemplates.h"

namespace BrnSound
{
namespace World
{
// DecFIGS BrnStaticSoundMap.h:59/152. ARTIST entities occupy one raw quad.
struct StaticSoundEntity
{
    union UFloatHelper { f32 mfPlusComponent; u32 mu32Bits; };

    const Vector3Plus& GetPosPlus() const { return mPosPlus; }
    Vector3 GetPos() const { return Vector3(mPosPlus.mV); }
    rw::math::fpu::Vector3Template<float> GetPosFpu() const
    {
        rw::math::fpu::Vector3Template<float> lPosition;
        lPosition.Set(mPosPlus.mV.x, mPosPlus.mV.y, mPosPlus.mV.z);
        return lPosition;
    }
    f32 GetRadius() const { return static_cast<f32>(mPosPlus.mV.u[3] & 0xffffu); }
    u16 GetType() const { return static_cast<u16>(mPosPlus.mV.u[3] >> 16); }

protected:
    Vector3Plus mPosPlus;
};
CGS_STATIC_ASSERT((sizeof(StaticSoundEntity) == 0x10), "ARTIST sound entity stride");

// DecFIGS :190/191; ARTIST826D19E4..FC reads and sign-extends both halves.
struct SubRegionDescriptor
{
    s16 mi16First;
    s16 mi16Count;
    s16 GetFirstEntity() const { return mi16First; }
    s16 GetNumEntities() const { return mi16Count; }
};
CGS_STATIC_ASSERT((sizeof(SubRegionDescriptor) == 4), "ARTIST sound cell stride");

struct StaticSoundMap
{
    enum eRootType { E_ROOT_TYPE_PASSBY = 0, E_ROOT_TYPE_EMITTER = 1, E_ROOT_TYPE_COUNT = 2 };

    // ARTIST8267AF10: receiver r3, scalar-vector reference r4.
    const SubRegionDescriptor* GetSubRegionDescrip(
        const rw::math::fpu::Vector3Template<float>& lrPosition) const;
    const StaticSoundEntity& GetEntity(s32 liEntityIndex) const;

    // ARTIST82677570: position r3, radius f1, packed rectangle r5; no receiver.
    static bool IsInRange(const rw::math::fpu::Vector3Template<float>& lrPosition,
                         f32 lfRadius,
                         const rw::math::fpu::Vector4Template<float>& lrPackedMinAndMax);

    const Vector2& GetMin() const { return mMin; }
    const Vector2& GetMax() const { return mMax; }
    f32 GetSubRegionSize() const { return mfSubRegionSize; }
    s32 GetNumEntities() const { return miNumEntities; }
    eRootType GetRootType() const { return meRootType; }
    friend struct StaticSoundMapResourceType;

private:
    Vector2 mMin;
    Vector2 mMax;
    f32 mfSubRegionSize;
    const SubRegionDescriptor* mpSubRegions;
    s32 miNumSubRegionsX;
    s32 miNumSubRegionsZ;
    const StaticSoundEntity* mpEntities;
    s32 miNumEntities;
    eRootType meRootType;
};
CGS_STATIC_ASSERT((sizeof(StaticSoundMap) == 0x40), "ARTIST sound map header");
}
}
#endif
