#pragma once

// ===================================================================================
// BrnGui::SatNavComponent -- owning header
//   b5-decomp/src/GameSource/Gui/SatNav/BrnSatNavComponent.h
//
// The sat-nav (minimap) HUD component: owns the render-event payload the sat-nav
// custom renderer consumes, the north-indicator + static-burst sub-components, and
// the MapIconManager ownership handshake. Class shape / enums / member names / method
// set from the DecFIGS DWARF (BrnSatNav.h:95/:99/:207/:233-:270), gated on the X360
// ledger. HUD H3a (2026-08-25): the full class replaces the old SetCachePointer-only
// shell (whose ledger "done" flag was a documented phantom).
//
// X360 member pins (this-relative, from the Construct/Update/RecvEvent asm --
// scratch h3_dump.txt): macSatNavIconParentNameBase @+0x8C, mRenderSatNavEvent
// payload @+0x100 (position; orientation +0x110, speed +0x114, zoom +0x118, map
// +0x11C, mask +0x120, route +0x124, rotate +0x128, trajectory +0x129), mpPlayerInfo
// @+0x130, mbUseNorthIndicator @+0x134, mNorthIndicatorComponent @+0x138, mStatic
// @+0x1C4, mpStateInterface @+0x250, mpIconManager @+0x254, mIconManagerId @+0x258,
// mpGuiCache @+0x25C, mbRotateMap @+0x260, mbUseTrajectory @+0x261, meMode @+0x264,
// meRoamingState @+0x268, mfRoamTime @+0x26C, mfRoamTimeElapsed @+0x270. Access is
// BY NAME (semantic parity per the x64 gate).
// ===================================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                            // Vector3 / Vector4
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"    // CgsGui::GuiComponent (base)
#include "GameSource/GameState/BrnGameStateSharedIO.h"                 // GameStateModuleIO::EGameModeType
#include "GameSource/Gui/Flow/HUD/Components/BrnNorthIndicator.h"      // BrnGui::NorthIndicatorComponent
#include "GameSource/Gui/SatNav/BrnMapIconManager.h"                   // BrnGui::MapIconManager (OwnerId)

namespace CgsModule { struct Event; }

namespace BrnGui
{
    class GuiCache;

    // DWARF BrnSatNavStatic.h:43 -- the sat-nav "static" (interference burst) overlay,
    // a plain GuiComponent with the one TriggerStatic entry point. Declared here (its
    // single consumer); TriggerStatic is its own ledger function (declaration-only).
    // The X360 RecvEvent compares apt trigger names against this component's resolved
    // name buffer (the GuiComponent name storage the base Construct fills).
    struct SatNavStatic : public CgsGui::GuiComponent
    {
        void TriggerStatic();   // DWARF BrnSatNavStatic.h:56 (declaration-only this slice)
    };

    // The cache's player-info block the sat-nav reads (the X360 binds mpPlayerInfo =
    // GuiCache+0x4AE0 -- the SAME block GetWorldCameraPosition exposes the head of).
    // X360-attested interior: +0x00 the world position lane, +0x28 the car speed as a
    // WORD (s32 mph; SetViewParamsFromPlayerCar `lwz 0x28` + fcfid), +0x34 the car
    // orientation in radians (f32; `lfs 0x34`). FLAG: the fields between are not yet
    // read by anything reconstructed and stay reserved.
    struct GuiPlayerInfo
    {
        Vector4 mv4Position;         // +0x00 (== GuiCache::mv4WorldCameraPosition)
        u8      mauReserved10[0x18]; // +0x10..+0x27 (unread interior)
        s32     miSpeedMph;          // +0x28
        u8      mauReserved2C[0x08]; // +0x2C..+0x33 (unread interior)
        f32     mfOrientation;       // +0x34 (radians)
    };

    // DWARF BrnSatNav.h:72 -- the render payload the sat-nav custom renderer consumes
    // (PS3 GuiEvent<210>; the X360 Update posts it as the 48-byte id-212 record on the
    // view-state channel: AddEvent {48, 212, 16}, ch41). X360-DIVERGENCE: the X360
    // record carries a ZOOM word after the speed (payload +0x18 == the cache's
    // miSatNavZoomLevel) that the PS3 DWARF does not list -- pinned by Update /
    // UpdateFreeRoaming storing cache+0x803C there. Pointers are native-width on this
    // host (the PlayAptMovie precedent: a hand-rolled 32-bit record truncates on x64).
    struct GuiEventRenderSatNav
    {
        Vector3   mv3CarPosition;          // :76  (X360 payload +0x00)
        f32       mfCarOrientation;        // :77  (X360 payload +0x10)
        f32       mfCarSpeedMph;           // :78  (X360 payload +0x14)
        s32       miZoomLevel;             // (X360-only word, payload +0x18)
        void*     mpMapTexture;            // :79  (X360 payload +0x1C; Texture2D*)
        void*     mpMaskTexture;           // :80  (X360 payload +0x20; Texture2D*)
        void*     mpRouteSegmentTexture;   // :81  (X360 payload +0x24; Texture2D*)
        bool      mbRotateMap;             // :82  (X360 payload +0x28)
        bool      mbUseTrajectory;         // :83  (X360 payload +0x29)

        s32 GetEventType() const { return 212; }   // X360 id 212 (was PS3-DWARF 210)
    };

    // DWARF BrnSatNav.h:95.
    struct SatNavComponent : public CgsGui::GuiComponent
    {
        // DWARF :99.
        enum ESatNavMode
        {
            E_SAT_NAV_MODE_TRACK_PLAYER = 0,
            E_SAT_NAV_MODE_FREE_ROAMING = 1,
            E_SAT_NAV_MODE_COUNT        = 2,
        };

        // DWARF :207.
        enum ERoamingModeState
        {
            E_ROAMING_STATE_INVALID      = 0,
            E_ROAMING_STATE_PENDING_DATA = 1,
            E_ROAMING_STATE_READY        = 2,
            E_ROAMING_STATE_IN_PROGRESS  = 3,
            E_ROAMING_STATE_DONE         = 4,
            E_ROAMING_STATE_COUNT        = 5,
        };

        // ---- bodied in BrnSatNavComponent.cpp (H3a slice) -----------------------
        void Construct(CgsGui::StateInterface* lpStateInterface,
                       const char* lpacParentName, ESatNavMode leMode);   // @0x824472C0 (DWARF :112)
        void Destruct();                                                  // @0x824475C8 (DWARF :123)
        void Update();                                                    // @0x82469378 (DWARF :127)
        void RecvEvent(const CgsModule::Event* lpEvent, s32 liEventId);   // @0x82457D00 (DWARF :133)
        void LoadResources();                                             // @0x82447A90 (DWARF :150)
        void SetEventType(BrnGameState::GameStateModuleIO::EGameModeType leGameMode); // @0x82447D30 (DWARF :197)
        static f32 GetViewDistance(f32 lfSpeedMph, s32 liCurrentZoomLevel); // @0x82447C00 (DWARF :169; no `this` use -- static so the rect builder can call it)
        void SetCachePointer(GuiCache* lpGuiCache);                       // @0x82473638 (DWARF :335)

        // ---- the zoomed-view math cluster (H3b: bodied) -------------------------
        // GetZoomedCarWorldRect @0x8244EEC8 (DWARF :179) -- build the four corners of
        // the (rotated) zoomed world window around the car. ⭐ STATIC: the X360 body
        // never touches `this` (r3 is the OUT pointer); the SatNavRenderer calls it
        // with its own payload values. Writes FOUR Vector3 corners:
        //   [0] pos+T(+z)+T(+x)  [1] pos+T(-z)+T(+x)  [2] pos+T(+z)+T(-x)  [3] pos+T(-z)+T(-x)
        // where T = RotationY(orientation) in rotate-map mode, RotationY(pi) otherwise
        // (identity when the orientation is NaN -- the asm's vcmpeqfp self-test).
        static void GetZoomedCarWorldRect(Vector3* lpOutCorners, Vector3 lv3CarPosition,
                                          f32 lfSpeedMph, f32 lfOrientation,
                                          bool lbRotateMap, bool lbUseTrajectory,
                                          s32 liZoomLevel);
        // SetViewParamsFromPlayerCar @0x82457C10 (DWARF :223) -- corners from the
        // player-info block, then (icon manager bound) install the zoomed world +
        // viewport spaces on MapTransform.
        void SetViewParamsFromPlayerCar(const GuiPlayerInfo* lpPlayerInfo);

        // ---- declaration-only (their own ledger functions; not in the H3a set) --
        void UpdateTrackPlayer();                                         // DWARF :227
        void StartRoaming(f32 lfRoamTime);                                // DWARF :160
        void AppendExpectedComponentList(u32* lpuHashes, u32* lpuCount, u32 luCapacity); // DWARF :145
        void SetIconVisibility(bool lbVisible);                           // DWARF :184
        void TriggerStatic();                                             // DWARF :323
        bool IsRoamingFinished();                                         // DWARF :351
        void ClearIconInfo();                                             // DWARF :303

    private:
        // [H3b] The freeburn HUD state's per-frame pre-pass writes this component's
        // mpPlayerInfo / mpIconManager directly (X360 UpdateRunning @0x8247B660 head:
        // stw 0 -> this+0x130; the icon manager's count clear through +0x254). The
        // stores are real and inline on console; friendship is the honest exposure
        // (the OnlineGameRoomPlayerInfo / GuiCache consumer-friend rule).
        friend struct FBurnMainHudState;
        friend struct RaceMainHudState;   // 2026-08-27: the in-event HUD's UpdateSatNav pre-pass
                                          // reaches mpPlayerInfo/mpIconManager the same way

        void UpdateFreeRoaming();                                         // @0x82447638 (DWARF :231)

        // The 16 per-icon component-name hashes ("<parent>_SatNavIcon<i>"), computed
        // by Construct (X360 static @0x82FB2E20, runtime-filled the same way).
        static u32 sauHashedSatNavIconNames[16];                          // DWARF :242 (mauHashedSatNavIconNames)

        static const s32 KI_MAX_PARENT_NAME_LENGTH = 112;                 // DWARF :240
        static const s32 KI_SATNAV_NUMBEROFICONS   = 16;                  // DWARF :241

        // ---- member layout (DWARF :245-:270 order; X360 pins in the banner) -----
        char                    macSatNavIconParentNameBase[KI_MAX_PARENT_NAME_LENGTH]; // :245 (+0x8C)
        GuiEventRenderSatNav    mRenderSatNavEvent;       // :247 (+0x100 payload)
        const GuiPlayerInfo*    mpPlayerInfo;             // :249 (+0x130)
        bool                    mbUseNorthIndicator;      // :252 (+0x134)
        NorthIndicatorComponent mNorthIndicatorComponent; // :253 (+0x138)
        SatNavStatic            mStatic;                  // :256 (+0x1C4)
        CgsGui::StateInterface* mpStateInterface;         // :259 (+0x250)
        MapIconManager*         mpIconManager;            // :260 (+0x254)
        MapIconManager::OwnerId mIconManagerId;           // :261 (+0x258)
        GuiCache*               mpGuiCache;               // :263 (+0x25C)
        bool                    mbRotateMap;              // :264 (+0x260)
        bool                    mbUseTrajectory;          // :265 (+0x261)
        ESatNavMode             meMode;                   // :267 (+0x264)
        ERoamingModeState       meRoamingState;           // :268 (+0x268)
        f32                     mfRoamTime;               // :269 (+0x26C)
        f32                     mfRoamTimeElapsed;        // :270 (+0x270)
    };
}
