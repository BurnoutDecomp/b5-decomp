#ifndef BRN_SOUND_LOGIC_WORLD_SOUND_WORLD_SCENE_H
#define BRN_SOUND_LOGIC_WORLD_SOUND_WORLD_SCENE_H

#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"
#include "GameSource/Sound/BrnResourceRegistrar.h"
#include "SharedClasses/Sound/World/BrnStaticSoundMap.h"

namespace BrnSound
{
namespace Module { struct SoundLogicModule; }

namespace Logic
{
namespace World
{

struct StaticSoundMapZone
{
    StaticSoundMapZone() { Construct(0); }

    void Construct(u16 lu16Zone);
    bool Prepare(const CgsResource::ResourceHandle& lrHandle);
    const CgsResource::ResourceHandle& GetResourceHandle() const { return mHandle; }
    const rw::math::fpu::Vector4Template<float>& GetPackedMinAndMax() const { return mPackedMinAndMax; }
    u16 GetZone() const { return mu16Zone; }
    void SetPackedMinAndMax(const rw::math::fpu::Vector4Template<float>& lrBounds);
    bool IsPrepared() const { return mHandle.mpResourceMemory != 0; }
    bool HasPackedMinAndMax() const { return mbPackedMinAndMax; }

private:
    CgsResource::ResourceHandle mHandle;
    rw::math::fpu::Vector4Template<float> mPackedMinAndMax;
    u16 mu16Zone;
    bool mbPackedMinAndMax;
};

struct SoundWorldScene : public BrnSound::Logic::IResourceRequester
{
    static const s32 KI_MAX_WORLD_ZONES_LOADED = 32;

    struct QueryInfo
    {
        rw::math::fpu::Vector3Template<float> mQueryPos;
        f32 mfRadius;
        BrnSound::World::StaticSoundEntity* mpEntitiesOut;
        s32 miMaxEntities;
        s32 miEntitiesWritten;
        bool mbDrawDebug;

        void Construct(Vector3 lPos, f32 lfRadius,
                       BrnSound::World::StaticSoundEntity* lpEntities, s32 liMax,
                       bool lbDebug)
        {
            mQueryPos.Set(lPos.mV.x, lPos.mV.y, lPos.mV.z);
            mfRadius = lfRadius;
            mpEntitiesOut = lpEntities;
            miMaxEntities = liMax;
            miEntitiesWritten = 0;
            mbDrawDebug = lbDebug;
        }
    };

    CGS_STATIC_ASSERT((sizeof(QueryInfo) == 0x20), "ARTIST sound query info");
    CGS_STATIC_ASSERT((offsetof(QueryInfo, mfRadius) == 0x0c), "ARTIST query radius");
    CGS_STATIC_ASSERT((offsetof(QueryInfo, mpEntitiesOut) == 0x10), "ARTIST query output");
    CGS_STATIC_ASSERT((offsetof(QueryInfo, miMaxEntities) == 0x14), "ARTIST query capacity");
    CGS_STATIC_ASSERT((offsetof(QueryInfo, miEntitiesWritten) == 0x18), "ARTIST query count");
    CGS_STATIC_ASSERT((offsetof(QueryInfo, mbDrawDebug) == 0x1c), "ARTIST query debug flag");

    SoundWorldScene();
    void Construct();
    bool Prepare(BrnSound::Module::SoundLogicModule* lpLogicModule,
                 const char* lpcResourceExt);
    bool Release();
    void Destruct();
    void Update();
    s32 Query(Vector3 lPosition, f32 lfRadius,
              BrnSound::World::StaticSoundEntity* lpEntitiesOut,
              s32 liMaxEntities, bool lbDrawDebug) const;

    virtual void ResourcesAreReady() ;
    virtual BrnSound::Logic::ResourceRegistrar& GetResourceRegistrar() ;

protected:
    void HandleWorldZoneLoad(u16 lu16Zone);
    void HandleWorldZoneUnload(u16 lu16Zone);
    void ResetLoader();
    const BrnSound::World::StaticSoundMap* GetZoneMap(s32 liIndex) const;
    void QuerySoundMap(QueryInfo& lrQuery,
                       const BrnSound::World::StaticSoundMap* lpMap) const;

    StaticSoundMapZone maSoundMapZones[KI_MAX_WORLD_ZONES_LOADED];
    s32 miNumZonesInUse;
    BrnSound::Module::SoundLogicModule* mpLogicModule;
    const char* mpcResourceExt;
    u16 mu16AcquiringZone;
    bool mbAcquireInProgress;
};

} // namespace World
} // namespace Logic
} // namespace BrnSound

#endif
