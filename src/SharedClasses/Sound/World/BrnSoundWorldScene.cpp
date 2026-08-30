#include "SharedClasses/Sound/World/BrnSoundWorldScene.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Sound/Module/BrnRootSoundModuleIo.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"

#include <cstdio>
#include <cmath>

namespace BrnSound
{
namespace Logic
{
namespace World
{

void StaticSoundMapZone::Construct(u16 lu16Zone)
{
    mHandle.Clear();
    mPackedMinAndMax.SetZero();
    mu16Zone = lu16Zone;
    mbPackedMinAndMax = false;
}

bool StaticSoundMapZone::Prepare(const CgsResource::ResourceHandle& lrHandle)
{
    mHandle = lrHandle;
    mPackedMinAndMax.SetZero();
    mbPackedMinAndMax = false;
    return true;
}

void StaticSoundMapZone::SetPackedMinAndMax(const Vector4& lrBounds)
{
    CGS_ASSERT(!HasPackedMinAndMax(), "!HasPackedMinAndMax()");
    mPackedMinAndMax = lrBounds;
    mbPackedMinAndMax = true;
}

SoundWorldScene::SoundWorldScene()
{
    Construct();
}

void SoundWorldScene::Construct()
{
    miNumZonesInUse = 0;
    mpLogicModule = 0;
    mpcResourceExt = 0;
    ResetLoader();
    for (s32 liZone = 0; liZone < KI_MAX_WORLD_ZONES_LOADED; ++liZone)
        maSoundMapZones[liZone].Construct(0);
}

bool SoundWorldScene::Prepare(BrnSound::Module::SoundLogicModule* lpLogicModule,
                              const char* lpcResourceExt)
{
    CGS_ASSERT(lpLogicModule && lpcResourceExt,
               "( lpLogicModule ) && ( lpcResourceExt )");
    mpcResourceExt = lpcResourceExt;
    mpLogicModule = lpLogicModule;
    return true;
}

bool SoundWorldScene::Release()
{
    GetResourceRegistrar().RemoveRequests(this);
    Construct();
    return true;
}

void SoundWorldScene::Destruct()
{
    Release();
}

void SoundWorldScene::ResetLoader()
{
    mu16AcquiringZone = static_cast<u16>(-1);
    mbAcquireInProgress = false;
}

BrnSound::Logic::ResourceRegistrar& SoundWorldScene::GetResourceRegistrar()
{
    CGS_ASSERT(mpLogicModule != 0, "mpLogicModule");
    return mpLogicModule->GetResourceRegistrar();
}

void SoundWorldScene::HandleWorldZoneLoad(u16 lu16Zone)
{
    for (s32 liZone = 0; liZone < miNumZonesInUse; ++liZone)
    {
        if (maSoundMapZones[liZone].GetZone() == lu16Zone)
            return;
    }
    CGS_ASSERT(miNumZonesInUse < KI_MAX_WORLD_ZONES_LOADED,
               "miNumZonesInUse < KI_MAX_WORLD_ZONES_LOADED");
    if (miNumZonesInUse < KI_MAX_WORLD_ZONES_LOADED)
        maSoundMapZones[miNumZonesInUse++].Construct(lu16Zone);
}

void SoundWorldScene::HandleWorldZoneUnload(u16 lu16Zone)
{
    s32 liFound = -1;
    for (s32 liZone = 0; liZone < miNumZonesInUse; ++liZone)
    {
        if (maSoundMapZones[liZone].GetZone() == lu16Zone)
        {
            liFound = liZone;
            break;
        }
    }
    CGS_ASSERT(liFound >= 0, "SoundWorldScene : unload for an unknown zone");
    if (liFound < 0)
        return;
    StaticSoundMapZone& lrZone = maSoundMapZones[liFound];
    if (lrZone.IsPrepared() ||
        (mbAcquireInProgress && mu16AcquiringZone == lu16Zone))
    {
        char lacResourceName[32];
        std::snprintf(lacResourceName, sizeof(lacResourceName), "TRK_UNIT%u%s",
                      static_cast<unsigned>(lrZone.GetZone()), mpcResourceExt);
        GetResourceRegistrar().RemoveRefsToResource(this, lacResourceName);
        if (mbAcquireInProgress && mu16AcquiringZone == lu16Zone)
            ResetLoader();
    }
    --miNumZonesInUse;
    for (s32 liZone = liFound; liZone < miNumZonesInUse; ++liZone)
        maSoundMapZones[liZone] = maSoundMapZones[liZone + 1];
    maSoundMapZones[miNumZonesInUse].Construct(0);
}

void SoundWorldScene::Update()
{
    CGS_ASSERT(mpLogicModule != 0, "mpLogicModule");
    BrnSound::Module::Io::LogicInputBuffer* lpInput =
        mpLogicModule->GetBrnInputStructure();
    const BrnSound::Module::Io::RootInputBuffer::SoundWorldLoadInterface* lpEvents =
        lpInput->GetWorldLoadInterface();
    for (s32 liEvent = 0; liEvent < lpEvents->GetLength(); ++liEvent)
    {
        const BrnSound::Module::Io::SoundWorldLoadEvent& lrEvent =
            lpEvents->GetEvent(liEvent);
        if (lrEvent.GetEvent() ==
            BrnSound::Module::Io::SoundWorldLoadEvent::E_LOAD_EVENT_PASSBY_MAP_LOADED)
            HandleWorldZoneLoad(lrEvent.GetZone());
        else if (lrEvent.GetEvent() ==
                 BrnSound::Module::Io::SoundWorldLoadEvent::E_LOAD_EVENT_PASSBY_MAP_UNLOAD)
            HandleWorldZoneUnload(lrEvent.GetZone());
        else
            CGS_ASSERT(false, "SoundWorldScene : unexpected world load event.");
    }

    if (!mbAcquireInProgress)
    {
        for (s32 liZone = 0; liZone < miNumZonesInUse; ++liZone)
        {
            if (maSoundMapZones[liZone].IsPrepared())
                continue;
            mbAcquireInProgress = true;
            mu16AcquiringZone = maSoundMapZones[liZone].GetZone();
            char lacResourceName[32];
            std::snprintf(lacResourceName, sizeof(lacResourceName), "TRK_UNIT%u%s",
                          static_cast<unsigned>(mu16AcquiringZone), mpcResourceExt);
            // ARTIST constructs a resource-only QueuedResource directly here:
            // resource name in r4, null bundle filename in r5, requester in r6,
            // pool id 3 in r7, E_DATA in r8.  LoadAsset is the bundle-oriented
            // helper and asserts its filename, so it is deliberately not used.
            BrnSound::Logic::ResourceRegistrar::QueuedResource lRequest(
                lacResourceName, 0, this, 3,
                BrnSound::Logic::ResourceRegistrar::E_DATA);
            GetResourceRegistrar().AddRequest(lRequest);
            break;
        }
    }

    const BrnSound::Module::Io::RootInputBuffer::UpdateInfo* lpUpdateInfo =
        lpInput->GetUpdateInfo();
    if (lpUpdateInfo->mData[0] == 0)
    {
        for (s32 liZone = 0; liZone < miNumZonesInUse; ++liZone)
        {
            StaticSoundMapZone& lrZone = maSoundMapZones[liZone];
            if (!lrZone.IsPrepared() || lrZone.HasPackedMinAndMax())
                continue;
            const BrnSound::World::StaticSoundMap* lpMap = GetZoneMap(liZone);
            CGS_ASSERT(lpMap != 0, "lpMap");
            if (lpMap)
            {
                const Vector2& lrMin = lpMap->GetMin();
                const Vector2& lrMax = lpMap->GetMax();
                Vector4 lBounds = { lrMin.x, lrMin.y, lrMax.x, lrMax.y };
                lrZone.SetPackedMinAndMax(lBounds);
            }
        }
    }
}

void SoundWorldScene::ResourcesAreReady()
{
    bool lbFound = false;
    char lacResourceName[32];
    std::snprintf(lacResourceName, sizeof(lacResourceName), "TRK_UNIT%u%s",
                  static_cast<unsigned>(mu16AcquiringZone), mpcResourceExt);
    CgsResource::ResourceHandle* lpHandle =
        GetResourceRegistrar().GetResource(0, lacResourceName);
    for (s32 liZone = 0; liZone < miNumZonesInUse; ++liZone)
    {
        if (maSoundMapZones[liZone].GetZone() == mu16AcquiringZone)
        {
            CGS_ASSERT(lpHandle != 0, "lpHandle");
            if (lpHandle)
                maSoundMapZones[liZone].Prepare(*lpHandle);
            lbFound = true;
            break;
        }
    }
    CGS_ASSERT(lbFound, "Mis-matched track streamer messages.");
    ResetLoader();
}

const BrnSound::World::StaticSoundMap* SoundWorldScene::GetZoneMap(s32 liIndex) const
{
    CGS_ASSERT(liIndex >= 0 && liIndex < miNumZonesInUse &&
               maSoundMapZones[liIndex].IsPrepared(),
               "( liIndex < miNumZonesInUse ) && ( maSoundMapZones[ liIndex ].IsPrepared() )");
    const CgsResource::ResourceHandle& lrHandle =
        maSoundMapZones[liIndex].GetResourceHandle();
    return lrHandle.mpResourceMemory
        ? *reinterpret_cast<const BrnSound::World::StaticSoundMap* const*>(
              lrHandle.mpResourceMemory)
        : 0;
}

s32 SoundWorldScene::Query(const Vector3& lrPosition, f32 lfRadius,
                           BrnSound::World::StaticSoundEntity* lpEntitiesOut,
                           s32 liMaxEntities, bool lbDrawDebug) const
{
    CGS_ASSERT(lpEntitiesOut && liMaxEntities > 0,
               "( lpEntitiesOut ) && ( liMaxEntities > 0 )");
    CGS_ASSERT(lfRadius > 0.0f,
               "SoundWorldScene : You must supply a positive radius.");
    QueryInfo lQuery(lrPosition, lfRadius, lpEntitiesOut, liMaxEntities, lbDrawDebug);
    const BrnSound::Module::Io::RootInputBuffer::UpdateInfo* lpUpdateInfo =
        mpLogicModule->GetBrnInputStructure()->GetUpdateInfo();
    if (lpUpdateInfo->mData[0] != 0)
        return 0;
    for (s32 liZone = 0; liZone < miNumZonesInUse; ++liZone)
    {
        const StaticSoundMapZone& lrZone = maSoundMapZones[liZone];
        if (!lrZone.IsPrepared())
            continue;
        CGS_ASSERT(lrZone.HasPackedMinAndMax(), "HasPackedMinAndMax()");
        const BrnSound::World::StaticSoundMap* lpMap = GetZoneMap(liZone);
        if (lpMap && lpMap->IsInRange(lrPosition, lfRadius,
                                      lrZone.GetPackedMinAndMax()))
            QuerySoundMap(lQuery, lpMap);
    }
    return lQuery.miEntitiesWritten;
}

void SoundWorldScene::QuerySoundMap(QueryInfo& lrQuery,
                                    const BrnSound::World::StaticSoundMap* lpMap) const
{
    CGS_ASSERT(lpMap != 0, "lpMap");
    const f32 lfSubRegionSize = lpMap->GetSubRegionSize();
    const s32 liRegionRadius =
        static_cast<s32>(std::ceil(lrQuery.mfRadius / lfSubRegionSize));
    const s32 liRegionSpan = 2 * liRegionRadius + 1;
    const f32 lfFirstOffset =
        -static_cast<f32>(liRegionRadius) * lfSubRegionSize;

    for (s32 liZ = 0; liZ < liRegionSpan; ++liZ)
    {
        for (s32 liX = 0; liX < liRegionSpan; ++liX)
        {
            Vector3 lCellPosition = lrQuery.mQueryPos;
            lCellPosition.x += lfFirstOffset + liX * lfSubRegionSize;
            lCellPosition.z += lfFirstOffset + liZ * lfSubRegionSize;
            const BrnSound::World::SubRegionDescriptor* lpRegion =
                lpMap->GetSubRegionDescrip(lCellPosition);
            if (!lpRegion)
                continue;

            const s32 liFirst = static_cast<s16>(lpRegion->GetFirstEntity());
            const s32 liCount = static_cast<s16>(lpRegion->GetNumEntities());
            for (s32 liEntity = 0; liEntity < liCount; ++liEntity)
            {
                if (lrQuery.miEntitiesWritten >= lrQuery.miMaxEntities)
                    return;
                const BrnSound::World::StaticSoundEntity& lrEntity =
                    lpMap->GetEntity(liFirst + liEntity);
                const Vector3 lPos = lrEntity.GetPos();
                const f32 lfDx = lPos.x - lrQuery.mQueryPos.x;
                const f32 lfDy = lPos.y - lrQuery.mQueryPos.y;
                const f32 lfDz = lPos.z - lrQuery.mQueryPos.z;
                const f32 lfRadius = lrEntity.GetRadius();
                if (lfDx * lfDx + lfDy * lfDy + lfDz * lfDz <=
                    lfRadius * lfRadius)
                {
                    lrQuery.mpEntitiesOut[lrQuery.miEntitiesWritten++] = lrEntity;
                }
            }
        }
    }
}

} // namespace World
} // namespace Logic
} // namespace BrnSound
