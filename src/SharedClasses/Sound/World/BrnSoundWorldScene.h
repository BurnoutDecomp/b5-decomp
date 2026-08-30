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
    const Vector4& GetPackedMinAndMax() const { return mPackedMinAndMax; }
    u16 GetZone() const { return mu16Zone; }
    void SetPackedMinAndMax(const Vector4& lrBounds);
    bool IsPrepared() const { return mHandle.mpResourceMemory != 0; }
    bool HasPackedMinAndMax() const { return mbPackedMinAndMax; }

private:
    CgsResource::ResourceHandle mHandle;
    Vector4 mPackedMinAndMax;
    u16 mu16Zone;
    bool mbPackedMinAndMax;
};

struct SoundWorldScene : public BrnSound::Logic::IResourceRequester
{
    static const s32 KI_MAX_WORLD_ZONES_LOADED = 32;

    struct QueryInfo
    {
        Vector3 mQueryPos;
        f32 mfRadius;
        BrnSound::World::StaticSoundEntity* mpEntitiesOut;
        s32 miMaxEntities;
        s32 miEntitiesWritten;
        bool mbDrawDebug;

        QueryInfo(const Vector3& lrPos, f32 lfRadius,
                  BrnSound::World::StaticSoundEntity* lpEntities, s32 liMax,
                  bool lbDebug)
            : mQueryPos(lrPos), mfRadius(lfRadius), mpEntitiesOut(lpEntities),
              miMaxEntities(liMax), miEntitiesWritten(0), mbDrawDebug(lbDebug) {}
    };

    SoundWorldScene();
    void Construct();
    bool Prepare(BrnSound::Module::SoundLogicModule* lpLogicModule,
                 const char* lpcResourceExt);
    bool Release();
    void Destruct();
    void Update();
    s32 Query(const Vector3& lrPosition, f32 lfRadius,
              BrnSound::World::StaticSoundEntity* lpEntitiesOut,
              s32 liMaxEntities, bool lbDrawDebug) const;

    virtual void ResourcesAreReady() override;
    virtual BrnSound::Logic::ResourceRegistrar& GetResourceRegistrar() override;

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
