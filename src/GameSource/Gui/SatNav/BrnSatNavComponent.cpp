// ===================================================================================
// BrnGui::SatNavComponent -- implementation (HUD H3a slice, 2026-08-25)
//   class:BrnGui::SatNavComponent
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (decompile+asm banked in scratch
// h3_dump.txt; constants h3_dump2/3.txt). Bodied here:
//   Construct          @ 0x824472C0    Destruct        @ 0x824475C8
//   Update             @ 0x82469378    RecvEvent       @ 0x82457D00
//   UpdateFreeRoaming  @ 0x82447638    LoadResources   @ 0x82447A90
//   GetViewDistance    @ 0x82447C00    SetEventType    @ 0x82447D30
//   SetCachePointer    @ 0x82473638
//
// PARKED BY NAME (H3b) -- the zoomed-view math cluster: GetZoomedCarWorldRect
// @0x8244EEC8 (XMMatrixRotationY VMX rect builder) and SetViewParamsFromPlayerCar
// @0x82457C10 (feeds it + MapTransform::SetZoomedWorldRect @0x824504E8 /
// SetZoomedViewportRect @0x82450608; the zoomed-viewport vector is the unrecovered
// runtime-init chain 82FB36A0 <- 82FB30A0). Both banked in h3_dump.txt; both are
// declaration-only here, so THIS TU STAYS UNMOUNTED until that cluster (plus the
// MapIconManager / NorthIndicator / SatNavStatic / GuiTracker TU mounts) closes.
// ===================================================================================
#include "GameSource/Gui/SatNav/BrnSatNavComponent.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                 // CgsCore::SPrintf
#include "GameShared/GameClasses/Containers/CgsHash.h"                  // CgsContainers::CgsHash::CalculateHash
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // StateInterface (access pointers / queues)
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h" // CgsGui::GuiEventAptTriggerPayload
#include "GameSource/Gui/BrnGuiCache.h"                                 // GuiCache (icon manager / tracker / zoom / resources)
#include "GameSource/Gui/SatNav/BrnGuiTracker.h"                        // GuiTracker::GetTrackerInformation
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                      // CgsGui::GuiEvent<212> (the render record head)

#include <cstring>   // strncmp / memset

namespace BrnGui
{

namespace
{
    // BrnSatNav.cpp:36..:40 (DWARF) -- the view-distance band. KF_MIN/MAX_VIEW_LERP
    // are X360 runtime statics COPIED from the dist pair by the 0x82C51FE0/0x82C51FF8
    // init thunks (h3_dump3.txt), so the four collapse to two values here.
    const f32 KF_CAR_MAX_MPH   = 120.0f;   // .rdata 0x82F258E4
    const f32 KF_MIN_VIEW_DIST = 375.0f;   // .rdata 0x82F259CC (== KF_MIN_VIEW_LERP @0x82FB3110)
    const f32 KF_MAX_VIEW_DIST = 630.0f;   // .rdata 0x82F259D0 (== KF_MAX_VIEW_LERP @0x82FB3118)

    // The per-zoom-level view-distance scale (X360 .rdata table @0x82F259D4, indexed
    // by the cache's miSatNavZoomLevel 0..1).
    const f32 KAF_ZOOM_VIEW_SCALE[2] = { 1.0f, 2.5f };

    // The sat-nav GUI event ids this component consumes (the X360 RecvEvent branch
    // table @0x82457DBC: base 21 + offsets 0/178/179/192).
    const s32 KI_EVENT_APT_TRIGGER    = 21;
    const s32 KI_EVENT_SATNAV_DATA    = 199;
    const s32 KI_EVENT_SATNAV_PARAMS  = 200;
    const s32 KI_EVENT_SHOW_HIDE      = 213;
}

// The 16 per-icon component-name hashes (X360 static @0x82FB2E20; Construct fills it).
u32 SatNavComponent::sauHashedSatNavIconNames[16];

// @ 0x824472C0 -- construct the sat-nav component: base GuiComponent init ("SatNav"),
// the north-indicator + static sub-components, the icon-manager binding out of the
// cache, the per-icon name hashes and the mode/roaming seeds.
void SatNavComponent::Construct(CgsGui::StateInterface* lpStateInterface,
                                const char* lpacParentName, ESatNavMode leMode)
{
    CGS_ASSERT(lpStateInterface != 0, "lpStateInterface");   // BrnSatNav.cpp:70 (non-gating)
    mpStateInterface = lpStateInterface;

    CgsGui::GuiComponent::Construct("SatNav", lpStateInterface, lpacParentName);

    // The render payload's texture/zoom slots + the rotate/trajectory pair (the X360
    // zero run @+0x118..+0x127 and the 0/1 byte pair @+0x128/+0x129).
    mRenderSatNavEvent.miZoomLevel           = 0;
    mRenderSatNavEvent.mpMapTexture          = 0;
    mRenderSatNavEvent.mpMaskTexture         = 0;
    mRenderSatNavEvent.mpRouteSegmentTexture = 0;
    mRenderSatNavEvent.mbRotateMap           = false;
    mRenderSatNavEvent.mbUseTrajectory       = true;

    mNorthIndicatorComponent.Construct("SatNav_mc", lpStateInterface, 0);
    mStatic.Construct("static_mc", lpStateInterface, 0);

    // The cache + icon manager, reached through the state interface's access pointers
    // (the same triple-assert chain the X360 emits).
    CgsGui::GuiAccessPointers* lpAccessPointers = lpStateInterface->GetAccessPointers();
    CGS_ASSERT(lpAccessPointers != 0, "mpAccessPointers != NULL");        // CgsGuiStateInterface.h:344 (non-gating)
    GuiCache* lpGuiCache = static_cast<GuiCache*>(lpAccessPointers->GetGuiCache());
    CGS_ASSERT(lpGuiCache != 0, "mpGuiCache");                            // CgsGuiShared.h:201 (non-gating)
    mpGuiCache = lpGuiCache;
    CGS_ASSERT(mpGuiCache != 0, "NULL != mpGuiCache");                    // :84 (non-gating)

    mpIconManager = mpGuiCache->GetMapIconManager();                      // cache +0x4060
    CGS_ASSERT(mpIconManager != 0, "NULL != mpIconManager");              // :88 (non-gating)

    mpPlayerInfo = 0;
    if (mpIconManager != 0)
        mpIconManager->miNumUsedIcons = 0;   // X360 stw 0, iconmgr+0x990 (friend grant below)
    mIconManagerId = MapIconManager::E_OWNERID_INVALID;

    // The 16 icon-name hashes: "<parent>_SatNavIcon<i>" (or bare "SatNavIcon<i>"
    // with no parent), through the shared name hasher.
    char lacIconName[32];
    for (s32 liIcon = 0; liIcon < KI_SATNAV_NUMBEROFICONS; ++liIcon)
    {
        if (lpacParentName != 0)
            CgsCore::SPrintf(lacIconName, 31, "%s_%s%d", lpacParentName, "SatNavIcon", liIcon);
        else
            CgsCore::SPrintf(lacIconName, 31, "%s%d", "SatNavIcon", liIcon);
        sauHashedSatNavIconNames[liIcon] =
            CgsContainers::CgsHash::CalculateHash(lacIconName,
                                                  static_cast<u32>(std::strlen(lacIconName)));
    }
    if (lpacParentName != 0)
        CgsCore::SPrintf(macSatNavIconParentNameBase, KI_MAX_PARENT_NAME_LENGTH - 1,
                         "%s_", lpacParentName);
    else
        std::memset(macSatNavIconParentNameBase, 0, KI_MAX_PARENT_NAME_LENGTH);

    mbRotateMap          = false;   // X360 stb 0 @+0x260
    mbUseTrajectory      = true;    // X360 stb 1 @+0x261
    mbUseNorthIndicator  = false;   // X360 stb 0 @+0x134
    meMode               = leMode;
    meRoamingState       = (leMode == E_SAT_NAV_MODE_FREE_ROAMING)
                               ? E_ROAMING_STATE_PENDING_DATA
                               : E_ROAMING_STATE_INVALID;
    mfRoamTime           = -1.0f;
    mfRoamTimeElapsed    = 0.0f;
}

// @ 0x824475C8 -- give the icon set back on teardown (a no-op with no bound manager).
void SatNavComponent::Destruct()
{
    if (mpIconManager != 0)
    {
        // The X360 also streams "MAPICONMANAGER: SatNav is calling ReleaseResources.\n"
        // through the map-icon debug channel when its verbose bit is set; the debug
        // stream is not reconstructed (debug-only, gated off by default).
        mpIconManager->ReleaseResources(mpStateInterface, mIconManagerId);
    }
}

// @ 0x82447A90 -- adopt the two sat-nav textures out of the cache's loaded-resource
// set (ids 199 == SatNavMap, 201 == SatNavMask -- the GUITEXTURES pair verified in
// build/game by CRC). Each may only ever bind once (the ==NULL || same asserts).
void SatNavComponent::LoadResources()
{
    CGS_ASSERT(mpGuiCache != 0, "Invalid gui cache");   // :565 (non-gating)

    const void* lpMapTexture = mpGuiCache->GetLoadedResource(199u);
    CGS_ASSERT(mRenderSatNavEvent.mpMapTexture == 0
                   || lpMapTexture == mRenderSatNavEvent.mpMapTexture,
               "mRenderSatNavEvent.mpMapTexture==NULL || lpNewTexture==mRenderSatNavEvent.mpMapTexture");   // :572 (non-gating)
    mRenderSatNavEvent.mpMapTexture = const_cast<void*>(lpMapTexture);
    CGS_ASSERT(mRenderSatNavEvent.mpMapTexture != 0, "mRenderSatNavEvent.mpMapTexture!=NULL");   // :574 (non-gating)

    const void* lpMaskTexture = mpGuiCache->GetLoadedResource(201u);
    CGS_ASSERT(mRenderSatNavEvent.mpMaskTexture == 0
                   || lpMaskTexture == mRenderSatNavEvent.mpMaskTexture,
               "mRenderSatNavEvent.mpMaskTexture==NULL || lpNewTexture==mRenderSatNavEvent.mpMaskTexture");   // :579 (non-gating)
    mRenderSatNavEvent.mpMaskTexture = const_cast<void*>(lpMaskTexture);
    CGS_ASSERT(mRenderSatNavEvent.mpMaskTexture != 0, "mRenderSatNavEvent.mpMaskTexture!=NULL");   // :581 (non-gating)
}

// @ 0x82447C00 -- the camera view distance for a car speed + zoom level: lerp the
// 375..630 band by speed/120 (clamped 0..1), re-clamp into the band, then scale by
// the zoom table.
f32 SatNavComponent::GetViewDistance(f32 lfSpeedMph, s32 liCurrentZoomLevel)
{
    f32 lfRatio = lfSpeedMph / KF_CAR_MAX_MPH;
    if (lfRatio < 0.0f)
        lfRatio = 0.0f;
    if (lfRatio > 1.0f)
        lfRatio = 1.0f;

    f32 lfViewDistance = KF_MIN_VIEW_DIST + (KF_MAX_VIEW_DIST - KF_MIN_VIEW_DIST) * lfRatio;
    if (lfViewDistance < KF_MIN_VIEW_DIST)
        lfViewDistance = KF_MIN_VIEW_DIST;
    if (lfViewDistance > KF_MAX_VIEW_DIST)
        lfViewDistance = KF_MAX_VIEW_DIST;

    CGS_ASSERT(liCurrentZoomLevel >= 0, "leCurrentZoomLevel >= E_SAT_NAV_ZOOM_IN_MAX");   // :705 (non-gating)
    CGS_ASSERT(liCurrentZoomLevel <= 1, "leCurrentZoomLevel <= E_SAT_NAV_ZOOM_OUT_MAX");  // :706 (non-gating)

    return KAF_ZOOM_VIEW_SCALE[liCurrentZoomLevel] * lfViewDistance;
}

// @ 0x82447D30 -- a bare forward: restyle the embedded north indicator for the mode.
void SatNavComponent::SetEventType(BrnGameState::GameStateModuleIO::EGameModeType leGameMode)
{
    mNorthIndicatorComponent.SetEventType(leGameMode);
}

// @ 0x82457D00 -- the sat-nav event pump.
void SatNavComponent::RecvEvent(const CgsModule::Event* lpEvent, s32 liEventId)
{
    CGS_ASSERT(lpEvent != 0, "Invalid event passed to SatNavComponent::RecvEvent");   // :481 (non-gating)

    switch (liEventId)
    {
    case KI_EVENT_APT_TRIGGER:   // 21
    {
        // A type-4 apt trigger naming the static overlay's component makes the icon
        // set visible (the "static burst finished" handshake).
        const CgsGui::GuiEventAptTriggerPayload* lpTrigger =
            reinterpret_cast<const CgsGui::GuiEventAptTriggerPayload*>(lpEvent);
        if (lpTrigger->meEventType == 4
            && std::strncmp(lpTrigger->mpacComponentName, mStatic.GetName(), 128) == 0)
        {
            mpIconManager->SetIconsVisible(true);
        }
        break;
    }
    case KI_EVENT_SATNAV_DATA:   // 199
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // :489 (non-gating)
        // Bind the cache's player-info block (the X360 takes cache+0x4AE0 -- the
        // world-camera lane heads it; see the header's GuiPlayerInfo note).
        mpPlayerInfo = reinterpret_cast<const GuiPlayerInfo*>(
            &mpGuiCache->GetWorldCameraPosition());
        CGS_ASSERT(mpPlayerInfo != 0, "mpPlayerInfo");   // :494 (non-gating)
        SetViewParamsFromPlayerCar(mpPlayerInfo);
        if (mpIconManager != 0)
            mpIconManager->UpdateSatNavInfo(
                reinterpret_cast<const GuiEventUpdateSatNav*>(lpEvent));
        break;
    }
    case KI_EVENT_SATNAV_PARAMS:   // 200
    {
        CGS_ASSERT(mpIconManager != 0, "mpIconManager");   // :510 (non-gating)
        mpIconManager->UpdateSatNavParams(lpEvent);
        const u8* lpuPayload = reinterpret_cast<const u8*>(lpEvent);
        mbRotateMap     = (lpuPayload[3] != 0);   // X360 lbz +3
        mbUseTrajectory = (lpuPayload[2] != 0);   // X360 lbz +2
        break;
    }
    case KI_EVENT_SHOW_HIDE:   // 213
    {
        const s32* lpiPayload = reinterpret_cast<const s32*>(lpEvent);
        if (mpIconManager != 0 && lpiPayload[0] == 1)
            mpIconManager->SetIconsVisible(
                reinterpret_cast<const u8*>(lpEvent)[8] != 0);
        break;
    }
    default:
        break;
    }
}

// @ 0x82447638 -- the free-roaming camera path: pick the roam position for the
// state, then reset the render event's motion fields and advance the roam clock.
void SatNavComponent::UpdateFreeRoaming()
{
    switch (meRoamingState)
    {
    case E_ROAMING_STATE_INVALID:
    case E_ROAMING_STATE_PENDING_DATA:
        // Follow the player until roam data lands.
        mRenderSatNavEvent.mv3CarPosition =
            *reinterpret_cast<const Vector3*>(&mpPlayerInfo->mv4Position);
        break;

    case E_ROAMING_STATE_READY:
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // :387 (non-gating)
        GuiTracker* lpTracker = mpGuiCache->GetGuiTracker();
        CGS_ASSERT(lpTracker != 0, "mpGuiCache->GetGuiTracker()");   // :388 (non-gating)
        GuiTracker::TrackerInformation* lpFirst = lpTracker->GetTrackerInformation(0);
        CGS_ASSERT(lpFirst != 0, "mpGuiCache->GetGuiTracker()->GetTrackerInformation(0)");   // :389 (non-gating)
        mRenderSatNavEvent.mv3CarPosition = lpFirst->mv3Position;   // X360 lvx info+0x10
        break;
    }

    case E_ROAMING_STATE_IN_PROGRESS:
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // :400 (non-gating)
        GuiTracker* lpTracker = mpGuiCache->GetGuiTracker();
        CGS_ASSERT(lpTracker != 0, "mpGuiCache->GetGuiTracker()");   // :401 (non-gating)
        CGS_ASSERT(lpTracker->GetTrackerInformation(0) != 0,
                   "mpGuiCache->GetGuiTracker()->GetTrackerInformation(0)");   // :402 (non-gating)
        CGS_ASSERT(lpTracker->GetTrackerInformation(lpTracker->GetNumTracked() - 1) != 0,
                   "mpGuiCache->GetGuiTracker()->GetTrackerInformation(mpGuiCache->GetGuiTracker()->GetNumTracked()-1)");   // :403 (non-gating)
        GuiTracker::TrackerInformation* lpFirst = lpTracker->GetTrackerInformation(0);
        GuiTracker::TrackerInformation* lpLast  =
            lpTracker->GetTrackerInformation(lpTracker->GetNumTracked() - 1);

        // The X360 vmaddfp pair, lane-for-lane (op1*op3 + op2 in IDA's print order):
        //   roamTime  > 0: pos = (last - first) * (elapsed / roamTime) + first
        //   roamTime <= 0: pos = last * first + 0.5   <- transcribed as computed; the
        //                  arm is only reachable with a non-positive roam time, which
        //                  StartRoaming never installs (kept faithful, not "fixed").
        const Vector3& lv3First = lpFirst->mv3Position;
        const Vector3& lv3Last  = lpLast->mv3Position;
        Vector3 lv3Out;
        if (mfRoamTime > 0.0f)
        {
            const f32 lfT = mfRoamTimeElapsed / mfRoamTime;
            lv3Out.x = (lv3Last.x - lv3First.x) * lfT + lv3First.x;
            lv3Out.y = (lv3Last.y - lv3First.y) * lfT + lv3First.y;
            lv3Out.z = (lv3Last.z - lv3First.z) * lfT + lv3First.z;
            lv3Out.w = (lv3Last.w - lv3First.w) * lfT + lv3First.w;
        }
        else
        {
            lv3Out.x = lv3Last.x * lv3First.x + 0.5f;
            lv3Out.y = lv3Last.y * lv3First.y + 0.5f;
            lv3Out.z = lv3Last.z * lv3First.z + 0.5f;
            lv3Out.w = lv3Last.w * lv3First.w + 0.5f;
        }
        mRenderSatNavEvent.mv3CarPosition = lv3Out;
        break;
    }

    case E_ROAMING_STATE_DONE:
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // :431 (non-gating)
        GuiTracker* lpTracker = mpGuiCache->GetGuiTracker();
        CGS_ASSERT(lpTracker != 0, "mpGuiCache->GetGuiTracker()");   // :432 (non-gating)
        GuiTracker::TrackerInformation* lpLast =
            lpTracker->GetTrackerInformation(lpTracker->GetNumTracked() - 1);
        CGS_ASSERT(lpLast != 0,
                   "mpGuiCache->GetGuiTracker()->GetTrackerInformation(mpGuiCache->GetGuiTracker()->GetNumTracked()-1)");   // :433 (non-gating)
        mRenderSatNavEvent.mv3CarPosition = lpLast->mv3Position;
        break;
    }

    default:
        CGS_ASSERT(false, "Invalid roaming state : ");   // :444 (non-gating)
        break;
    }

    // The roaming camera has no car motion: zero the readouts, force the rotate /
    // trajectory flags off, refresh the zoom word and advance the roam clock by the
    // cache's frame step (GuiCache +0x00).
    mRenderSatNavEvent.mfCarOrientation = 0.0f;
    mRenderSatNavEvent.mfCarSpeedMph    = 0.0f;
    mRenderSatNavEvent.mbRotateMap      = false;
    mRenderSatNavEvent.mbUseTrajectory  = false;
    mRenderSatNavEvent.miZoomLevel      = mpGuiCache->GetSatNavZoomLevel();   // cache +0x803C

    mfRoamTimeElapsed += mpGuiCache->GetTimeStep();   // cache +0x00
    if (mfRoamTime > 0.0f && mfRoamTime <= mfRoamTimeElapsed)
    {
        mfRoamTimeElapsed = 0.0f;
        meRoamingState    = E_ROAMING_STATE_DONE;
    }
}

// @ 0x82469378 -- the per-frame sat-nav tick: (re)claim the icon set, refresh the
// view params + icon manager, drive the north indicator, fill the render payload for
// the active mode, and post it to the view-state channel. (The X360 brackets the body
// with PerfMonCpu monitors -- profiling plumbing, not reconstructed.)
void SatNavComponent::Update()
{
    CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // :228 (non-gating)
    mpIconManager = mpGuiCache->GetMapIconManager();
    CGS_ASSERT(mpIconManager != 0, "mpIconManager");   // :231 (non-gating)

    const MapIconManager::OwnerId lePreviousId = mIconManagerId;
    if (lePreviousId == MapIconManager::E_OWNERID_INVALID)
    {
        // Claim the icon set: component-name base "<parent>SatNavIcon", 16 icons,
        // owner E_SATNAV_MAP, no road signs, drive-thrus unless the mode is 13, no
        // drive-thru selection, display type 5, no icon parent (X360 stack args).
        char lacIconComponentName[128];
        CgsCore::SPrintf(lacIconComponentName, 127, "%s%s",
                         macSatNavIconParentNameBase, "SatNavIcon");
        const bool lbShowDriveThrus = (mpGuiCache->GetGameMode() != 13);   // X360 lwz cache+0x9E58, != 13
        mIconManagerId = mpIconManager->SetOwnerParameters(
            mpStateInterface, lacIconComponentName,
            16, MapIconManager::E_SATNAV_MAP,
            false, lbShowDriveThrus, false,
            static_cast<GuiEventDrawEventIcons::EIconDisplayType>(5), 0);
    }
    if (lePreviousId != mIconManagerId)
    {
        // Fresh ownership: reset the event-info flag, adopt this component's rotate
        // mode and drop the icon size back to small (X360 pokes @+0xAA18/+0xAA1C/
        // +0xAA04; by name through the friend grant).
        mpIconManager->mbIsDisplayingEventInfo = false;
        mpIconManager->mbRotateSatNav          = mbRotateMap;
        mpIconManager->meIconSizeMode          = MapIconManager::E_ICONSIZE_SMALL;
    }

    mpPlayerInfo = reinterpret_cast<const GuiPlayerInfo*>(
        &mpGuiCache->GetWorldCameraPosition());   // cache +0x4AE0
    CGS_ASSERT(mpPlayerInfo != 0, "mpPlayerInfo");   // :292 (non-gating)
    SetViewParamsFromPlayerCar(mpPlayerInfo);

    if (mpIconManager != 0)
        mpIconManager->Update();

    if (mbUseNorthIndicator)
    {
        mNorthIndicatorComponent.Update(
            mbRotateMap ? mpPlayerInfo->mfOrientation : 3.1415927f);
    }

    if (meMode == E_SAT_NAV_MODE_TRACK_PLAYER)
    {
        mRenderSatNavEvent.mv3CarPosition =
            *reinterpret_cast<const Vector3*>(&mpPlayerInfo->mv4Position);
        mRenderSatNavEvent.mfCarOrientation = mpPlayerInfo->mfOrientation;
        mRenderSatNavEvent.mfCarSpeedMph    = static_cast<f32>(mpPlayerInfo->miSpeedMph);
        mRenderSatNavEvent.mbRotateMap      = mbRotateMap;
        mRenderSatNavEvent.mbUseTrajectory  = mbUseTrajectory;
        mRenderSatNavEvent.miZoomLevel      = mpGuiCache->GetSatNavZoomLevel();
    }
    else if (meMode == E_SAT_NAV_MODE_FREE_ROAMING)
    {
        UpdateFreeRoaming();
    }

    // Post the render payload on the view-state channel (X360 record {48, 212, 16},
    // ch41, 64 bytes -- native-width here per the PlayAptMovie precedent).
    CGS_ASSERT(mpStateInterface != 0, " state interface is invalid ");   // :333 (non-gating)
    {
        struct GuiEventRenderSatNavRecord : public CgsGui::GuiEvent<212>
        {
            GuiEventRenderSatNav mPayload;
            explicit GuiEventRenderSatNavRecord(const GuiEventRenderSatNav& lrPayload)
                : CgsGui::GuiEvent<212>(48, 16)
                , mPayload(lrPayload)
            {
            }
        } lRecord(mRenderSatNavEvent);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRecord),
            41 /* KI_CHANNEL_VIEW_STATE */, sizeof(lRecord));
    }
}

// @ 0x82473638 -- adopt the GUI cache pointer (the pre-H3a slice, retyped onto the
// real GuiCache now the full class exists).
void SatNavComponent::SetCachePointer(GuiCache* lpGuiCache)
{
    CGS_ASSERT(lpGuiCache != 0, "lpCache");   // BrnSatNav.h:342 (non-gating)
    mpGuiCache = lpGuiCache;
}

}
