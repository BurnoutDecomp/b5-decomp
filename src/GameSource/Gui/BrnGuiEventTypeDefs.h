// BrnGuiEventTypeDefs.h
// Home of the BrnGui GUI-event payload structs. This slice reconstructs ONLY
// BrnGui::GuiEventUpdateSatNav::SatNavIconInfo, and within it only the four
// range-checked accessors that the X360 ARTIST build emits as out-of-line
// functions (GetCounty / GetDistrict / GetActiveRaceCarIndex / GetPlayerTeam).
//
// Sources:
//   * X360 BURNOUT_X360_ARTIST.XEX  (binary, AUTHORITATIVE on layout/widths)
//       GetCounty             @ 0x823A6A20  -> lbz r,0x24(this)  (zero-extend -> u8)
//       GetDistrict           @ 0x823A6AA8  -> lbz r,0x25(this)  (zero-extend -> u8)
//       GetActiveRaceCarIndex @ 0x824B2EF8  -> lbz;extsb 0x26(this) (sign-extend -> s8)
//       GetPlayerTeam (IDA "GetPlay") @ 0x824EB190 -> lbz;extsb 0x27(this) (s8)
//   * references/DecFIGS/dwarfdump/GameSource/Gui/BrnGuiEventTypeDefs.h:303-381
//       (PS3 DWARF) names the icon-info members and the SatNavIconType enum.
//
// X360 / PS3-DWARF DRIFT (binary authoritative): the PS3 DWARF labels the four
// trailing bytes mu8County / mu8District / mi8ActiveRaceCarIndex / mi8IconType.
// The X360 ARTIST build reads 0x27 with a "lePlayerTeam" guard (range 0..8), i.e.
// the fourth trailing byte is a player-team field on X360, not the icon-type byte.
// We follow the X360 binary: the fourth byte is mi8PlayerTeam. (The icon type is
// carried by SetIconType/GetIconType, declared-only here.)
//
// LAYOUT NOTE: only the four trailing bytes (offsets 0x24..0x27) are load-bearing
// for the reconstructed accessors and are pinned by the X360 byte offsets above.
// The leading payload (position / id / rotation / speed / landmark / design /
// hidden) is reproduced from the PS3 DWARF as NAMED members but its exact
// inter-member padding is not independently verified against the X360 binary; it
// is grouped in an explicitly-reserved head so the four verified bytes sit at the
// proven offsets. This is an honest layout boundary, not a fabricated one.

#pragma once

#include "types.hpp"                                   // u8/s8/u32 widths, f32
#include "BrnCommonTypes.h"                             // Vector3, CgsID
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"  // CgsResource::ResourceHandle
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"  // CgsContainers::FastBitArray
#include "GameShared/GameClasses/Containers/CgsArray.h"  // Array<T,N> (OfflinePostEventData's DerivedCarArray)
#include "GameSource/BurnoutConstants.h"                // EActiveRaceCarIndex
#include "SharedClasses/World/BrnWorldRegion.h"         // BrnWorld::ECounty / EDistrict
#include "GameShared/GameClasses/Core/CgsAssert.h"      // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"          // CgsIDCompress
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"     // CgsGui::GuiEvent<N> (event payload base)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiPopupResource.h" // PopupStyle/PopupIcons/GuiPopupParameter
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiHudMessage.h"     // HudMessageParamTypes/HudMessageParameter
#include "GameShared/GameClasses/Fonts/CgsUnicode.h"    // CgsUnicode::CgsUtf8 (GuiHudMessage::GetParam return)
#include "GameSource/GameState/ModeManager/Scoring/BrnBurnoutSkillzData.h" // BurnoutSkillzData (by value)
#include "SharedClasses/StreetData/BrnStreetData.h"     // BrnStreetData::RoadIndex (road-rules events)
#include "SharedClasses/StreetData/BrnChallengeData.h"  // BrnStreetData::ScoreType / E_SCORE_TYPE_COUNT (road-rules events)
#include "GameSource/GameState/BrnGameStateSharedIO.h"  // BrnGameState::GameStateModuleIO::EGameModeType (GuiEventJunctionInfo)
#include "GameSource/GameState/BrnTakedownType.h"       // BrnGameState::ETakedownType (GuiTakedownEvent)
#include "GameSource/GameState/BrnGameActions.h"        // [gateui r3] BrnWorld::EPowerParkOutcome (GuiPowerParkResult) -- it has no
                                                        // fixed underlying type, so the opaque-enum idiom below cannot carry it
#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostType.h" // [gateui r3] BrnWorld::EBoostType (GuiEventBoostInfo)
#include "GameSource/GameState/BrnCgsPlayerName.h"      // CgsNetwork::PlayerName (road-rule high score / left-lobby payloads)
#include "GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoring.h" // BrnGameState::StuntInfo (GuiHUDMessageStuntPerformed)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h" // BrnNetwork::EPaybackType (dirty-trick payloads)

// Fixed-underlying-type opaque declaration (committed home BrnVehicleConstants.h:
// `enum EImpactType : s32`); keeps the vehicle-constants header out of this GUI payload
// home (same idiom as BrnScoringSystem.h).
namespace BrnPhysics { namespace Vehicle { enum EImpactType : s32; } }

// Pointer-only collaborators of GuiEventProgressionProfileData (id 350). The real homes
// are GameSource/GameState/Progression/BrnProfile.h and the progression manifest resource;
// this header only carries the two pointers, so a forward declaration is enough (and keeps
// the 118 KB progression profile out of every AddGuiEvent<T> translation unit).
namespace BrnProgression { class Profile; struct ProgressionData; }

namespace BrnGui
{

// The three GUI render/flow layers the cache tracks apt components for (DWARF:
// BrnGuiEventTypeDefs.h:883 `enum GuiFlow`). Used as the leading selector on the
// GuiCache apt-component bookkeeping (e.g. AppendExpectedAptComponent) and on the
// component AppendExpectedAptComponent helpers. ADDITIVE GROW: only adds this enum
// at its DWARF home; nothing existing is changed.
enum GuiFlow
{
    E_GUIFLOW_SCREEN  = 0,
    E_GUIFLOW_HUD     = 1,
    E_GUIFLOW_OVERLAY = 2,
    E_GUIFLOW_COUNT   = 3,
    E_GUIFLOW_FIRST   = 0,
};

// BrnGuiAudioEvent.cpp / ARTIST @0x824F6350. The native payload is exactly
// { component[32], action, label[32], movie[32] } (100 bytes). The committed PC
// GuiEvent<N> carries its 12-byte queue header explicitly, making this queued
// record 112 bytes in total.
struct GuiAudioTriggerEvent : public CgsGui::GuiEvent<201>
{
    char macComponent[32];
    s32  meAction;
    char macLabel[32];
    char macMovie[32];

    GuiAudioTriggerEvent() : CgsGui::GuiEvent<201>(0, 12), meAction(0)
    {
        macComponent[0] = 0;
        macLabel[0] = 0;
        macMovie[0] = 0;
    }

    void Construct(s32 leAction, const char* lpComponentName,
                   const char* lpLabel, const char* lpMovieName = "");
};

static_assert(sizeof(GuiAudioTriggerEvent) == 112,
              "GuiAudioTriggerEvent is a 100-byte payload plus the 12-byte PC GuiEvent header");

// BrnGuiEventTypeDefs.h:912 -- HUD FSM selector carried by GuiEventRunFsm.
enum EHUDFSMs
{
    E_GUI_HUD_INVALID = -1,
    E_GUI_HUD_BOOT    = 0,
    E_GUI_HUD_FREEBURN = 1,
    E_GUI_HUD_EVENT   = 2,
    E_GUI_HUD_ONLINE  = 3,
    E_GUI_HUD_NUMSFSMS = 4,
};

// BrnGuiEventTypeDefs.h:936 -- request to run a GUI FSM. X360 BrnGuiFsmController::RunFsm
// copies this record as three qwords and reads meFlowToUse at +0x14, proving the controller-facing
// record is 24 bytes with the ids first. The DecFIGS text lists meFsmToRun first, but the ARTIST
// assembly wins for field order.
struct GuiEventRunFsm : public CgsModule::Event
{
    CgsID    mFsmId;          // +0x00 -- FSM LuaCode resource id to load
    CgsID    mInitialStateId; // +0x08 -- initial scripted state id
    EHUDFSMs meFsmToRun;      // +0x10
    GuiFlow  meFlowToUse;     // +0x14 -- RunFsm validates this against [0, E_GUIFLOW_COUNT)

    GuiEventRunFsm()
        : mFsmId(CgsIDCompress(" "))
        , mInitialStateId(CgsIDCompress(" "))
        , meFsmToRun(E_GUI_HUD_NUMSFSMS)
        , meFlowToUse(E_GUIFLOW_COUNT)
    {
    }

    // 144 is the LIVE wire id on this build: the mounted consumer is BrnGuiModule.cpp's
    // case 144 -> GuiFsmController::RunFsm, and both producers post 144. The DecFIGS text
    // carried the dead PS3 id 142 here, which forced two hand-rolled AddEvent(...,144,...)
    // workarounds until 2026-08-27; the header is now truthful and the producers use it.
    s32 GetEventType() const { return 144; }
};

static_assert(sizeof(GuiEventRunFsm) == 24, "GuiEventRunFsm controller payload is 24 bytes");

// Player-team identity carried on the X360 sat-nav icon payload. Bounds match the
// X360 GetPlayerTeam guard (start == 0, count == 9). The canonical enum lives in the
// GameState IO layer (GsmIO::EPlayerTeam); we reference the bounds by value here to
// avoid pulling the GameState module into this GUI payload header.
enum EPlayerTeam : s8
{
    E_PLAYER_TEAM_START = 0,
    E_PLAYER_TEAM_COUNT = 9,
};

// DWARF BrnGuiEventTypeDefs.h:618 (:123) -- one overhead road-sign score marker
// (world position + owning entity index). Carried in Array<OverheadSignScore,32>
// instantiations (stride 0x20, proven by the Array `slwi ...,5` element shift).
struct OverheadSignScore
{
    Vector3 mWorldSpacePosition;   // :125  @0x00 (alignas(16) Vector3, 16B)
    u16     muEntityIndex;         // :126  @0x10
};                                 // sizeof == 0x20 (X360 stride)

static_assert(sizeof(OverheadSignScore) == 0x20,
              "OverheadSignScore stride 0x20 (Array<OverheadSignScore,32> `slwi ...,5`)");

// [hud H3b tracking slice 2026-08-25] the two raw player-telemetry records the world->GUI
// bridge posts and GuiCache::RecEvent consumes (relocated here from the demangled-shell
// header, which would otherwise ODR-clash for TUs that include both).
//   GuiEventUpdateHud (147): BridgeWorldVehicleDataToGui @0x823E5768 builds a RAW 12-byte
//   {(s32)RaceCarState::mfSpeedMPH @972, (s32)mfRPM @984, mi8Gear @1092} local (BE stores
//   +0/+4/+8, no GuiEvent header); AddGuiEvent<GuiEventUpdateHud> @0x823DA5C8 ==
//   AddEvent(&event, 147, 12). Cache case 147 -> +19208/+19212/+19216.
struct GuiEventUpdateHud
{
    s32 miSpeedMph;   // @0
    s32 miRPM;        // @4
    s8  mi8Gear;      // @8
    u8  mau8Pad[3];   // @9..@11 (the console posts 12 bytes)
    s32 GetEventType() const { return 147; }
};

//   GuiPlayerRaceCarIdEvent (376): the raw {active, global} index pair
//   (@0x823DA458 == AddEvent(&event, 376, 8)); GuiCache case 376 asserts both ranges as
//   "lpRaceCarIdEvent->mePlayer*" -- the member names are the cache's own assert strings.
struct GuiPlayerRaceCarIdEvent
{
    s32 mePlayerActiveRaceCarIndex;  // @0 (BrnGuiCache.cpp:1904/1905)
    s32 mePlayerGlobalRaceCarIndex;  // @4 (:1906/1907)
    s32 GetEventType() const { return 376; }
};

// Declaration mirror of the DWARF parent. Only the nested SatNavIconInfo is modelled.
struct GuiEventUpdateSatNav
{
    struct SatNavIconInfo
    {
        // -- BrnGuiEventTypeDefs.h:1678 (PS3 DWARF) --
        enum SatNavIconType
        {
            E_SATNAVICON_PLAYER_CAR         = 0,
            E_SATNAVICON_MARKED_MAN         = 1,
            E_SATNAVICON_NETWORKRIVAL       = 2,
            E_SATNAVICON_RIVAL              = 3,
            E_SATNAVICON_LANDMARK           = 4,
            E_SATNAVICON_JUNCTION           = 5,
            E_SATNAVICON_FREEBURN_CHALLENGE = 6,
            E_SATNAVICON_JUNKYARD           = 7,
            E_SATNAVICON_CAR_PARK           = 8,
            E_SATNAVICON_BODYSHOP           = 9,
            E_SATNAVICON_GAS_STATION        = 10,
            E_SATNAVICON_PAINT_SHOP         = 11,
            E_SATNAVICON_TIRE_SHOP          = 12,
            E_SATNAVICON_ROADSIGN           = 13,
            E_SATNAVICON_MAX                = 14,
        };

        // ---- reconstructed accessors (X360 out-of-line) ----
        BrnWorld::ECounty   GetCounty() const;              // @ 0x823A6A20  reads mu8County  @0x24
        BrnWorld::EDistrict GetDistrict() const;            // @ 0x823A6AA8  reads mu8District @0x25
        EActiveRaceCarIndex GetActiveRaceCarIndex() const;  // @ 0x824B2EF8  reads mi8ActiveRaceCarIndex @0x26
        EPlayerTeam         GetPlayerTeam() const;          // @ 0x824EB190  reads mi8PlayerTeam @0x27

        // ---- setters (DWARF :1710-1731) -- bodied 2026-08-25 (hud H3b tracking slice):
        // the 199 producer fills each icon through them; the county/district range
        // asserts are the console's own (inlined at the bridge's fill sites,
        // BrnGuiEventTypeDefs.h:1857/:1873).
        void SetCounty(BrnWorld::ECounty leCounty)
        {
            CGS_ASSERT(static_cast<s32>(leCounty) >= 0, "leCounty >= 0");        // :1857
            mu8County = static_cast<u8>(leCounty);
        }
        void SetDistrict(BrnWorld::EDistrict leDistrict)
        {
            CGS_ASSERT(static_cast<s32>(leDistrict) >= 0, "leDistrict >= 0");    // :1873
            mu8District = static_cast<u8>(leDistrict);
        }
        void SetActiveRaceCarIndex(EActiveRaceCarIndex leIndex)
        {
            mi8ActiveRaceCarIndex = static_cast<s8>(leIndex);
        }
        void SetIconType(SatNavIconType leType)
        {
            mi8IconType = static_cast<s8>(leType);
        }
        SatNavIconType GetIconType() const
        {
            return static_cast<SatNavIconType>(mi8IconType);
        }

        // [H3b FLAG consumer-named faces] the PS3 DWARF exposes these head members
        // public with no setter pair; the 199 producer writes them through named faces
        // (the GetCgsId/SetPositionLane idiom above).
        void SetCgsId(CgsID lId)              { mCgsId = lId; }          // @0x10
        void SetRotation(f32 lfRotation)      { mfRotation = lfRotation; }   // @0x18
        void SetSpeedMph(f32 lfSpeedMph)      { mfSpeedMph = lfSpeedMph; }   // @0x1C
        void SetHiddenDriveThru(bool lbHidden){ mbIsHiddenDriveThru = lbHidden; } // @0x23
        f32  GetRotation() const              { return mfRotation; }

        // FLAGGED ADDITIVE GROW (Scene-Gui-Realmc group): the X360 DoWorstCase
        // @0x823B1980 reads/writes the position vector at icon-relative +0x00
        // (`lvx128 v0,r0,<icon>`, 16 bytes), the county/district/index bytes at
        // +0x24/+0x25/+0x26 and the icon-type byte at +0x28, with a 0x30-byte stride
        // between icons. So the full SatNavIconInfo is 0x30 bytes (the previously
        // 0x28-modelled struct undersized it). This grow ONLY ADDS named fields at
        // X360-proven offsets and pins the total size to 0x30; the four committed
        // accessors and their proven 0x24..0x27 offsets are untouched.

        // ADDITIVE GROW (BrnGui::MapIconManager TU): the MapIconManager icon loops read
        // the icon-type byte @0x28, the hidden-drive-through flag @0x23 and write the
        // player-team byte @0x27. The X360 inlines these as raw byte loads/stores; expose
        // them by name so the manager TU accesses them through the type, not by offset.
        // No field is reordered/retyped/removed - mbIsHiddenDriveThru is carved out of the
        // (previously fully-reserved) head at its DWARF-documented @0x23 position.
        s8   GetIconTypeByte() const   { return mi8IconType; }              // @0x28
        s8   GetPlayerTeamByte() const { return mi8PlayerTeam; }            // @0x27
        void SetPlayerTeamByte(s8 li8Team) { mi8PlayerTeam = li8Team; }     // @0x27
        bool IsHiddenDriveThru() const { return mbIsHiddenDriveThru; }      // @0x23

        // ADDITIVE GROW (BrnSatNavRenderer TU): the sat-nav renderer's online-landmark icon path
        // (GetIconInformation display-type 2) copies the 16-byte position lane (@0x00) and the
        // signed landmark-index half-word (@0x20) out of a landmark record. Exposed by name so the
        // renderer stays off raw offsets. Returns the Vector4 position lane (the renderer narrows
        // it to its Vector3 icon position) and the sign-extended @0x20 half-word.
        const Vector4& GetPositionLane() const { return mv4Position; }      // @0x00
        s16  GetLandmarkIndexHalf() const                                   // @0x20 (within the head)
        {
            return miLandmarkIndex;   // [H3b] the head carve named it; same bytes.
        }
        // [map arm 2026-08-27] the write sides of the two head fields, attested by the
        // GuiCache landmark fills: GetLandmarkInfoFromIndex @0x82506688 (`sth r27, 0x20`
        // -- the caller's index -- and `lbz lm+0x31; stb 0x22` -- the landmark's design
        // index) and GetLandmarkInfoFromID @0x825067E0 (`lhz lm+0x28; sth 0x20`).
        void SetLandmarkIndexHalf(s16 liIndex) { miLandmarkIndex = liIndex; }   // @0x20
        void SetDesignIndex(u8 lu8Design)      { mu8DesignIndex = lu8Design; }  // @0x22

        // ADDITIVE GROW (wave-J CrashNavMap + PreRaceFlyBy TUs). Two named faces over head
        // members that are already DWARF-documented but were, until now, buried inside the
        // opaque reserved head:
        //   * mCgsId (@0x10) is the icon's identity word -- the drive-thru / landmark /
        //     rival CgsID. PreRaceFlyByState::SetEventIconResource @0x824BB5F8 loads it
        //     WHOLE (`ld r11, info+0x10` at 0x824BB704, after GetLandmarkInfoFromIndex
        //     filled the stack record) and switches on it; CrashNavMap::UpdateIconManager
        //     reads it for the hovered drive-thru / rival id.
        //   * the 16-byte position lane is WRITTEN (not just read) by
        //     CrashNavMap::UpdateIconManager / MoveCursor, which park a located icon into
        //     mLockedIconInfo with a `stvx128` of the whole lane.
        // FLAG consumer-named: the ACCESSORS are ours (the PS3 DWARF exposes mv3Position /
        // mCgsId as public members and declares no getter/setter pair); the MEMBER names
        // are the DWARF's. Same idiom as the GetIconTypeByte / GetPositionLane faces above.
        CgsID GetCgsId() const { return mCgsId; }                           // @0x10
        void  SetPositionLane(const Vector4& lv4Position)                   // @0x00
        {
            mv4Position = lv4Position;
        }

    private:
        // ---- leading payload (PS3 DWARF :1699-1707) ----
        // The DWARF-documented head order is, at byte offsets: Vector3 mv3Position
        // @0x00; CgsID mCgsId @0x10; f32 mfRotation @0x18; f32 mfSpeedMph @0x1C;
        // LandmarkIndex @0x20; u8 mu8DesignIndex; bool mbIsHiddenDriveThru. The
        // 16-byte position lane (read/written by DoWorstCase via lvx128/stvx128) and
        // the 8-byte id at @0x10 (SetEventIconResource's whole-word `ld`) are pinned by
        // the X360 binary; the remaining head bytes' exact inter-member padding is NOT
        // independently verified, so they stay an explicitly-reserved mid block. This
        // keeps the four verified trailing bytes at their proven offsets. See LAYOUT
        // NOTE at top of file.
        Vector4 mv4Position;          // @0x00 .. 0x0F  (X360-pinned; DoWorstCase lvx128)
        CgsID   mCgsId;               // @0x10 .. 0x17  (DWARF head order, X360-pinned by
                                      //   the SetEventIconResource `ld info+0x10`)
        // ⭐ [hud H3b tracking slice 2026-08-25] the reserved mid block is carved to its
        // DWARF names: the 199 producer (BridgeWorldVehicleDataToGui @0x823E5768) writes
        // mfRotation (`stfs +0x18`, the heading angle vs north) and mfSpeedMph
        // (`stfs +0x1C`, speeds[idx] * flt_830180B0) per icon; the cache's case-199
        // player store reads mfRotation back (`lfs 24(r30)` -> cache +0x4AF4). The
        // landmark half at @0x20 keeps its accessor below; mu8DesignIndex fills @0x22.
        f32     mfRotation;           // @0x18  (DWARF :1702; heading, radians 0..2pi)
        f32     mfSpeedMph;           // @0x1C  (DWARF :1703)
        s16     miLandmarkIndex;      // @0x20  (DWARF :1704; GetLandmarkIndexHalf)
        u8      mu8DesignIndex;       // @0x22  (DWARF :1705)
        bool    mbIsHiddenDriveThru;  // @0x23  (DWARF head tail; MapIconManager drive-thru skip)

        // ---- trailing bytes, X360-pinned (offsets proven by the four accessors) ----
        u8      mu8County;          // @0x24  (GetCounty / DoWorstCase write)
        u8      mu8District;        // @0x25  (GetDistrict / DoWorstCase write)
        s8      mi8ActiveRaceCarIndex; // @0x26 (GetActiveRaceCarIndex / DoWorstCase write)
        s8      mi8PlayerTeam;      // @0x27  (GetPlayerTeam, sign-extended; X360 drift vs DWARF mi8IconType)
        s8      mi8IconType;        // @0x28  (validity-loop read; DoWorstCase writes E_SATNAVICON_NETWORKRIVAL)
        u8      maTailReserved[7];  // @0x29 .. 0x2F  (pads SatNavIconInfo to its 0x30 stride)

        // DoWorstCase / SatNavI need direct access to these X360-pinned fields.
        friend struct GuiEventUpdateSatNav;
    };

    // FLAGGED ADDITIVE GROW (Scene-Gui-Realmc group): the OUTER GuiEventUpdateSatNav
    // body the X360 emitted (DoWorstCase @0x823B1980, SatNavI @0x823A6B30) treats the
    // event as an array of SatNavIconInfo entries at +0x00 (0x30-byte stride) followed
    // by an icon count at +0x900. 0x900 / 0x30 == 0x30 (48) entries of capacity. These
    // members are pinned by the X360 binary (count at byte +0x900, entry stride 0x30).
    static const s32 KI_MAX_SAT_NAV_ICONS = 0x900 / 0x30; // 48 (capacity)

    SatNavIconInfo maIconInfo[KI_MAX_SAT_NAV_ICONS]; // @0x000 .. 0x8FF
    s32            miNumIcons;                        // @0x900

    // ADDITIVE GROW (CgsGui::GuiModule::AddGuiEvent<GuiEventUpdateSatNav> @0x823D93A0):
    // the producer queues this event with the compile-time id 199 (inlined GetEventType()).
    // The type is not GuiEvent<N>-derived (flat icon array), so the id is carried by this
    // method. X360-attested by the AddEvent(&event, 199, 2320) literal.
    s32 GetEventType() const { return 199; }

    // ---- reconstructed out-of-line bodies (X360 ARTIST) ----
    // @0x823B1980 — recompute the "worst case" sat-nav icon set: validate the active
    // icons, compact the located player-position icon to the front, then synthesise a
    // ring of placeholder icons. Returns `this`.
    GuiEventUpdateSatNav* DoWorstCase();

    // @0x823A6B30 — range-check + return the leading icon's icon-type byte (a static
    // helper the CrashNavMap callers use). Reads maIconInfo[0].mi8IconType.
    static s32 SatNavI(const GuiEventUpdateSatNav* lpThis);
};

// The icon record is a pointer-free scalar run, so its host size is the X360 stride the
// binary proves (DoWorstCase indexes icons 0x30 apart; KI_MAX_SAT_NAV_ICONS above divides
// the +0x900 count offset by it). Pinned so a later head carve cannot silently resize it.
static_assert(sizeof(GuiEventUpdateSatNav::SatNavIconInfo) == 0x30,
              "SatNavIconInfo stride 0x30 (DoWorstCase icon step; 0x900/0x30 == 48 icons)");

// ===================================================================================
// BrnGui::GuiEventDrawEventIcons -- the "draw event icons" GUI event payload.
//   Home: this header (Gui/BrnGuiEventTypeDefs.h; the X360 asserts reference it:
//   Construct @0x824EB218 -> BrnGuiEventTypeDefs.h:2785; GetIgnoreIcons @0x82443518 ->
//   :2815/:2816). Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Construct populates the event: a draw/hide flag, which event set the icon pass draws,
// a fade time, and an "icons to ignore" list (up to KI_MAX_ICONS_TO_IGNORE = 10 entries)
// copied in from a caller-supplied source array. GetIgnoreIcons reads the list back out
// (count + the entries).
//
// X360 layout (stores authoritative on width / offset):
//   mauIconsToIgnore[10] @0x00..0x27  (the copied list; Construct loop / GetIgnoreIcons)
//   mfFadeTime           @0x28        (stfs f1)
//   meIconDisplayType    @0x2C        (stw r5)
//   miNumIconsToIgnore   @0x30        (stw r8 -- read by both methods)
//   mbDrawIcons          @0x34        (stb r4)
//
// CORRECTED (wave J): the previously committed shape named these
// mfDisplayTime/muIconSetId/mbFlag and gave Construct a phantom 4th parameter
// (`s32 liUnused`). Both were artifacts of reading the X360 register usage without the
// PPC float-argument rule: a float argument travels in f1 and SKIPS its GPR slot, so r6
// is dead at 0x824EB218 -- there is no 4th integer parameter. The DWARF
// (BrnGuiEventTypeDefs.h:2680/2693-2697) gives the real member names and the 5-parameter
// signature, and the store offsets above match it position-for-position. The DWARF also
// returns void from both methods; the X360 leaves `this` in r3 only by accident (both
// bodies clobber r3 via the CgsDev::Assert calls and never restore it), so the old
// `returns this` was a decompiler artifact.
//
// NOT MODELLED: the PS3 DWARF derives this event from GuiEvent<539>. The X360 Construct
// writes maIgnoreIcons at `this`+0x00, so the console base contributes no bytes; adding
// the committed 12-byte PC GuiEvent<N> header here would be a layout change no X360
// AddGuiEvent instantiation attests. Left as a plain class -- flagged, not fixed.
// ===================================================================================
class GuiEventDrawEventIcons
{
public:
    // DWARF BrnGuiEventTypeDefs.h:2662 -- which event set the icon pass draws. Nested
    // here per the DWARF; BrnGui::GuiEventEnableSatNavIcons carries an identical-valued
    // copy at ITS OWN DWARF home (:7511) -- both are real, and the DWARF
    // MapIconManager::SetOwnerParameters signature (BrnMapIconManager.h:191) types its
    // 8th parameter with THIS one.
    enum EIconDisplayType
    {
        E_ICON_DISPLAY_TYPE_OFFLINE_EVENTS       = 0,
        E_ICON_DISPLAY_TYPE_ONLINE_EVENT_STARTS  = 1,
        E_ICON_DISPLAY_TYPE_ONLINE_CHECKPOINTS   = 2,
        E_ICON_DISPLAY_TYPE_ONLINE_FINISH_POINTS = 3,
        E_ICON_DISPLAY_TYPE_ONLINE_EVENT_PRESETS = 4,
        E_ICON_DISPLAY_TYPE_COUNT                = 5,
    };

    // BrnGuiEventTypeDefs.h:2785 guard bound -- max entries in the ignore list.
    static const s32 KI_MAX_ICONS_TO_IGNORE = 10;

    // @0x824EB218 (DWARF :2680) -- populate the event. lbDrawIcons / leIconDisplayType /
    // lfFadeTime are stored verbatim; the ignore list
    // (lpuIconsToIgnore[0..liNumIconsToIgnore-1]) is copied into mauIconsToIgnore.
    // Asserts the list is empty OR (non-NULL and within bound).
    void Construct(bool lbDrawIcons,
                   EIconDisplayType leIconDisplayType,
                   f32 lfFadeTime,
                   u32* lpuIconsToIgnore,
                   s32 liNumIconsToIgnore);

    // @0x82443518 (DWARF :2688) -- copy the ignore list back out. Asserts both pointers
    // non-NULL, writes the count to *lpiNumIconsToIgnore, copies the entries to
    // lpuIconsToIgnore.
    void GetIgnoreIcons(u32* lpuIconsToIgnore, s32* lpiNumIconsToIgnore) const;

private:
    u32 mauIconsToIgnore[KI_MAX_ICONS_TO_IGNORE]; // @0x00 (DWARF :2693) -- the copied list
    f32 mfFadeTime;                               // @0x28 (DWARF :2694)
    EIconDisplayType meIconDisplayType;           // @0x2C (DWARF :2695)
    s32 miNumIconsToIgnore;                       // @0x30 (DWARF :2696) -- entry count
    bool mbDrawIcons;                             // @0x34 (DWARF :2697)
};

// ===================================================================================
// BrnGui::GuiOverlayRequest -- the "show an overlay" GUI request payload.
//   Home: this header. The X360 asserts reference it:
//     GetMessageParam @0x824EB948 -> BrnGuiEventTypeDefs.h:7508/7509
//     GetButton1Param @0x824EBA78 -> :7528
//     GetButton2Param @0x824EBB38 -> :7547
//   (Hex line numbers are the X360-baked assert lines; the strings below match the
//   X360 assert message text. Reconstructed from BURNOUT_X360_ARTIST.XEX.)
//
// The request carries a small array of "message" parameters plus two fixed "button"
// parameters. Each parameter is an id + a printf-formatted text string. The three
// accessors copy a parameter into a caller-supplied output record: they SPrintf the
// parameter's text (via "%s") into the output's text buffer and copy the parameter's
// id into the output's leading dword.
//
// X360 layout (stores/loads authoritative on width / offset). Each parameter is a
// 0x44-byte ParamInfo record { ...head 8B...; u32 muId @+0x08; char macText[56] @+0x0C }:
//   maMessages[2]   @0x00..0x87  (2 message params; GetMessageParam indexes 0x44*idx)
//   mButton1        @0x88        (id @0x90, text @0x94 == record+0x08 / record+0x0C)
//   mButton2        @0xCC        (id @0xD4, text @0xD8 == record+0x08 / record+0x0C)
//   ...reserved...  @0x110..0x117
//   miNumMessages   @0x118       (GetMessageParam bound: assert idx < miNumMessages)
//   mbButton1Used   @0x11C       (GetButton1Param guard byte: assert non-zero)
//   mbButton2Used   @0x11D       (GetButton2Param guard byte: assert non-zero)
//
// LAYOUT NOTE: only the offsets touched by the three accessors are X360-pinned (the
// message stride 0x44 + the +0x08/+0x0C sub-fields, the two button records at
// +0x88/+0xCC, the count at +0x118 and the two guard bytes at +0x11C/+0x11D). The two
// message slots are sized to exactly reach the first button record (+0x88 == 2*0x44);
// the 8 bytes between the last button record and the count are an explicitly-reserved
// span, not a fabricated member.
// ===================================================================================
class GuiOverlayRequest
{
public:
    // One overlay parameter: a 0x44-byte record holding an id and a text string. The
    // X360 stride between message params is 0x44 (GetMessageParam: r31 = 0x44*idx+this),
    // with the id read at record+0x08 and the text formatted from record+0x0C.
    struct ParamInfo
    {
        u8   maHead[0x08];   // +0x00..+0x07 (head; not touched by the accessors)
        u32  muId;           // +0x08  (copied into the output record's leading dword)
        char macText[0x38];  // +0x0C..+0x43  (SPrintf "%s" source; 56 bytes to the 0x44 stride)
    };

    // The output record the accessors fill: a leading id dword then a 64-byte text
    // buffer (SPrintf writes into output+0x04 with length 64; the id is stored at
    // output+0x00).
    struct ParamOut
    {
        u32  muId;            // +0x00  (*a2 = param.muId)
        char macText[0x40];   // +0x04  (SPrintf(out+4, 64, "%s", param.macText))
    };

    // Message param slots. GetMessageParam asserts the index is in [0, miNumMessages);
    // the first button record sits immediately after these two slots (+0x88).
    static const s32 KI_MAX_MESSAGES = 2;

    // ADDITIVE GROW (CgsGui::GuiModule::AddGuiEvent<GuiOverlayRequest> @0x823D5180): the
    // producer queues this request with the compile-time id 184 (inlined GetEventType()).
    // The type is not GuiEvent<N>-derived, so the id is carried by this method. X360-attested
    // by the AddEvent(&request, 184, 288) literal.
    s32 GetEventType() const { return 184; }

    // @0x823B1CC8 -- initialise the request from an overlay-id string. Asserts the string
    // is non-NULL ("Invalid Overlay Id", BrnGuiEventTypeDefs.h:7404), compresses it to the
    // leading CgsID (stored over the first param record's 8-byte head at +0x00), and clears
    // the message count + the two button-present guards. Returns the compressed id (the X360
    // returns r3 == the CgsIDCompress result). Called by ~45 overlay-request sites.
    CgsID Construct(const char* lpcOverlayId);

    // ADDITIVE GROW (GuiOverlaysDirector::HandleOverlayRequest @0x825162C8, which loads
    // the request's leading qword): the compressed overlay id Construct stored over the
    // first param record's head.
    CgsID GetOverlayId() const
    {
        return *reinterpret_cast<const CgsID*>(maMessages[0].maHead);
    }

    // @0x82472A18 -- append one message param: SPrintf ("%s", cap 63 + forced NUL @+0x4B)
    // lpcParam into the next free record's text, store luId at its +0x08 id slot, bump
    // miNumMessages. Asserts miNumMessages < 2 ("Not enough free params in the Overlay
    // (<n>/2).", streamed) and lpcParam != NULL. Returns the SPrintf result (r3). ADDITIVE
    // GROW (BrnCarSelectMain wave G: HandleLaunchedEvent @0x824C8FBC / the sibling
    // HandleLaunchingEvent @0x824C915C call it). Declaration-only: the body's ledger TU is
    // elsewhere (identity primary_file mis-attributes it to CgsStrStream.h) and no
    // definition exists in the tree yet -- see the wave-G spec.
    s32 AddMessageParam(u32 luId, const char* lpcParam);

    // @0x824EB948 -- copy message param liIndex into lOut. Asserts liIndex < miNumMessages
    // ("Index isn't used in Overlay.") and liIndex >= 0 ("Index isn't valid."). Returns the
    // SPrintf result (the X360 returns r3 from CgsCore::SPrintf).
    s32 GetMessageParam(ParamOut* lpOut, s32 liIndex) const;

    // @0x824EBA78 -- copy button-1 param into lOut. Asserts mbButton1Used != 0
    // ("button 1 param isn't used in Overlay."). Returns the SPrintf result.
    s32 GetButton1Param(ParamOut* lpOut) const;

    // @0x824EBB38 -- copy button-2 param into lOut. Asserts mbButton2Used != 0
    // ("button 2 param isn't used in Overlay."). Returns the SPrintf result.
    s32 GetButton2Param(ParamOut* lpOut) const;

private:
    ParamInfo maMessages[KI_MAX_MESSAGES];  // @0x00..0x87 (message params 0,1)
    ParamInfo mButton1;                     // @0x88 (id @0x90, text @0x94)
    ParamInfo mButton2;                     // @0xCC (id @0xD4, text @0xD8)
    u8        maReserved[0x118 - 0x110];    // @0x110..0x117 (unrecovered span before the count)
    s32       miNumMessages;                // @0x118 (message-param count)
    u8        mbButton1Used;                // @0x11C (button-1 present guard)
    u8        mbButton2Used;                // @0x11D (button-2 present guard)

    // X360-pinned sub-field guards on the parameter records (the loads/stores that index
    // them: id @+0x08, text @+0x0C, 0x44 stride; the output's text buffer at +0x04).
    static_assert(sizeof(ParamInfo) == 0x44, "ParamInfo stride 0x44 (GetMessageParam: 0x44*idx)");
    static_assert(__builtin_offsetof(ParamInfo, muId) == 0x08, "param id @+0x08");
    static_assert(__builtin_offsetof(ParamInfo, macText) == 0x0C, "param text @+0x0C");
    static_assert(__builtin_offsetof(ParamOut, macText) == 0x04, "out text @+0x04 (SPrintf out+4)");
};

// X360-pinned member offsets (the two button records and the count / guard bytes). At
// namespace scope after the complete type; the members are non-public so these are
// validated indirectly via the reserved-span construction documented above. (The button
// records land at +0x88 / +0xCC == 2*0x44 / 3*0x44, the count at +0x118, the guards at
// +0x11C / +0x11D, matching GetButton1Param/GetButton2Param/GetMessageParam.)

// ===================================================================================
// BrnGui::GuiNewBurnoutSkillzEvent -- the per-frame "new burnout skillz scores" event
// the scoring system publishes to the GUI. The burnout-skills HUD manager
// (BrnGui::BurnoutSkillsManager::SetSkillsData, X360 @0x825118F0) consumes it: it walks
// the carried per-skill score array, updates each skill's record-holder, fires the
// "you beat" HUD flash and, for newly-beaten records above a per-skill threshold, emits a
// GuiNewBurnoutHudMessageEvent.
//
// DWARF home BrnGuiEventTypeDefs.h:6330 (GuiEvent<526>); member order/types verbatim from
// the DecFIGS DWARF and corroborated by the X360 SetSkillsData field offsets (member
// access by NAME -- per project policy the GuiEvent<N> base contributes its own header to
// the layout; the asm offsets only fix member ORDER, not byte placement):
//   mNetworkPlayerID      (origin player id)             -- read by the HUD-message branch
//   meActiveRaceCarIndex  (which active race-car scored)
//   mSkillzData           (the 14-skill score array; SetSkillsData reads mafBurnoutSkilz[i])
//   mbUpdateHUDMessage    (gate: only emit the HUD message when set; X360 lbz @event+0x40)
// X360 AddGuiEvent<GuiNewBurnoutSkillzEvent> @0x823D8260 bakes id 541 (was PS3-DWARF 526).
struct GuiNewBurnoutSkillzEvent : public CgsGui::GuiEvent<541>
{
    // DWARF type is RoadRulesRecvData::NetworkPlayerID, a typedef of s32 (see
    // BrnNetwork::RoadRulesRecvData). Modelled as s32 here to avoid dragging the network
    // flyby header into this GUI payload home; value semantics are identical (the manager
    // stores it / compares it to -1 only).
    s32                                mNetworkPlayerID;    // BrnGuiEventTypeDefs.h:6333
    EActiveRaceCarIndex                meActiveRaceCarIndex;// BrnGuiEventTypeDefs.h:6334
    BrnGameState::BurnoutSkillzData    mSkillzData;         // BrnGuiEventTypeDefs.h:6335
    bool                               mbUpdateHUDMessage;  // BrnGuiEventTypeDefs.h:6336
};

// ===================================================================================
// BrnGui::GuiNewBurnoutHudMessageEvent -- the "show a burnout-skill HUD flash" event the
// burnout-skills manager pushes onto the GUI out-event queue when a record changes hands.
// DWARF home BrnGuiEventTypeDefs.h:6358 (GuiEvent<527>). The X360 SetSkillsData builds it
// field by field on the stack and publishes it via OutputBuffer::AddGuiOutEvent<>:
//   mRoadID         -- the road/road-rule id (left zero by SetSkillsData; head dword)
//   meMessageType   -- which flash text (X_BEAT_YS / X_BEAT_YOUR / X_GOT / YOU_GOT)
//   meSkill         -- the skill whose record changed
//   meNewOwner      -- the active race-car that now holds the record
//   mePreviousOwner -- the active race-car that previously held it (-1 if none)
// X360 AddGuiEvent<GuiNewBurnoutHudMessageEvent> @0x823D8318 bakes id 542 (was PS3-DWARF 527).
struct GuiNewBurnoutHudMessageEvent : public CgsGui::GuiEvent<542>
{
    // BrnGuiEventTypeDefs.h:6351 -- which of the four HUD flash strings to show.
    enum EBurnoutSkillzMessageTypes
    {
        E_BURNOUT_SKILLZ_MESSAGE_TYPE_X_BEAT_YS   = 0,
        E_BURNOUT_SKILLZ_MESSAGE_TYPE_X_BEAT_YOUR = 1,
        E_BURNOUT_SKILLZ_MESSAGE_TYPE_X_GOT       = 2,
        E_BURNOUT_SKILLZ_MESSAGE_TYPE_YOU_GOT     = 3,
        E_BURNOUT_SKILLZ_MESSAGE_TYPE_COUNT       = 4,
    };

    CgsID                                          mRoadID;        // BrnGuiEventTypeDefs.h:6361
    EBurnoutSkillzMessageTypes                     meMessageType;  // BrnGuiEventTypeDefs.h:6362
    BrnGameState::BurnoutSkillzData::EBurnoutSkillType meSkill;    // BrnGuiEventTypeDefs.h:6363
    EActiveRaceCarIndex                            meNewOwner;     // BrnGuiEventTypeDefs.h:6364
    EActiveRaceCarIndex                            mePreviousOwner;// BrnGuiEventTypeDefs.h:6365
};

// The colour-calibration SCREEN's show / hide requests: the two events
// BrnGui::CrashNavColourCalibrate posts and BrnGui::ColourCalibrationScreen::RecvEvent
// (@0x824471D0) consumes.
//
// DWARF (rung 2, BrnGuiEventTypeDefs.h:150 / :163) declares them
//     struct BrnGui::GuiEventColourCalibrationScreenShow : public GuiEvent<504> {};
//     struct BrnGui::GuiEventColourCalibrationScreenHide : public GuiEvent<505> {};
// -- both EMPTY, no payload members. The X360 ids are 514 (0x202) / 515 (0x203), not the
// PS3 504/505; read off the binary at BOTH ends, producer and consumer:
//   * BrnGui::ColourCalibrationScreen::RecvEvent @0x82447268 `cmpwi cr6, r27, 0x202`
//     (-> PREPARE_TO_SHOW) and @0x82447270 `cmpwi cr6, r27, 0x203` (-> PREPARE_TO_HIDE);
//   * BrnGui::CrashNavColourCalibrate::ShowCalibrationCard @0x824CE7E8 `li r11, 0x202`
//     and @0x824CE7FC `li r11, 0x203`, baked into the record it queues;
//   * BrnGui::GuiModule::HandleEventsPostBaseModuleUpdate @0x8250788C dispatches
//     `r5 - 0x1ED` cases 21/22 (== 514/515) to ColourCalibrationScreen::RecvEvent, and
//     BrnGame::BrnGameModule::BridgeGuiToDirector @0x823CC6FC/@0x823CC744 turns the same
//     two ids into SetGotColourCalibration{Shown,Hidden}Event.
//
// EMPTY IS ATTESTED, NOT ASSUMED: ShowCalibrationCard queues a channel-40 GuiEventOut
// record { muHeader0 = 1 (payload BYTE COUNT == sizeof(T)), muEventType = 514/515,
// muHeader2 = 12 (payload offset) }, 16 bytes total (0x824CE7D8-F0 / 0x824CE7FC-14:
// `li r11,1 / stw var_30`, `li r11,0xC / stw var_28`, `li r5,0x28 (=channel 40)`,
// `li r6,0x10 (=16)`). A payload size of 1 is the sizeof of an EMPTY struct; a
// CgsGui::GuiEvent<N> base (12 bytes) would have made it 12 and the record 24. So, like
// its GuiOptionsBrightnessContrast siblings below, this is the PAYLOAD, not the record:
// the 12-byte header is supplied by whichever wire posts it and the type id is carried by
// GetEventType() alone.
// PRODUCER NOTE: BrnCrashNavColourCalibrate.cpp posts these as the console's 16-byte channel-40
// record (its TU-local GuiCommandWire16<514>/<515>, header {1, id, 12} + one unwritten payload
// byte); the consumer, BrnGui::ColourCalibrationScreen::RecvEvent, keys on the id alone. These
// two empty types are the DWARF-named payloads for that record.
struct GuiEventColourCalibrationScreenShow : public CgsModule::Event
{
    s32 GetEventType() const { return 514; }
};

struct GuiEventColourCalibrationScreenHide : public CgsModule::Event
{
    s32 GetEventType() const { return 515; }
};

// The options screen's DISPLAY-CALIBRATION hand-off -- the event that carries the
// brightness / contrast slider positions from the GUI to the game module.
//
// DWARF (rung 2, BrnGuiEventTypeDefs.h:6425-6428) declares it
//     struct BrnGui::GuiOptionsBrightnessContrast : public GuiEvent<530>
//     { int32_t mBrightness; int32_t mContrast; };
// The X360 build's id is 545 (0x221), not the PS3 530: the same merge-window +15 that
// moved the PostFxControl sibling 531 -> 546. Both ids are read off the binary, not
// assumed -- BrnGui::CrashNavColourCalibrate::ApplySettings @0x824CEBCC bakes
// `li r11, 0x221` into the record it queues, and BrnGame::BrnGameModule::BridgeGuiToGame
// @0x823CB9D8 dispatches `cmpwi cr6, r3, 0x221`.
//
// THE TWO PRODUCERS both post it as a channel-40 GuiEventOut record
// { muHeader0 = 8 (payload bytes), muEventType = 545, muHeader2 = 12 (payload offset) }
// followed by the two payload words, 20 bytes total:
//   BrnGui::CrashNavColourCalibrate::ApplySettings              @0x824CEB50 (the slider)
//   BrnGui::ScreenLoading::ApplyOptionsDataProfileSettings      @0x824D0DC0 (the save)
// The consumer is BridgeGuiToGame's `case 0x221` @0x823CBA1C, two 32-bit loads at payload
// +0 / +4 into the game module's miBrightness (+10096740) / miContrast (+10096744).
//
// Like its PostFxControl sibling below this is the PAYLOAD, not the record: the queued
// record's 12-byte header is supplied by whichever wire posts it (CgsGui::GuiEvent<545>
// for the channel-40 form, CgsGuiModuleIO::OutputBuffer::AddGuiOutEvent's own keying for
// the type-keyed form), so the type id is carried by GetEventType() alone.
struct GuiOptionsBrightnessContrast : public CgsModule::Event
{
    s32 mBrightness;   // +0x00 (X360 `lwz r11, 0(r29)`)
    s32 mContrast;     // +0x04 (X360 `lwz r11, 4(r29)`)

    s32 GetEventType() const { return 545; }
};

// The "drive the brightness/contrast-calibration post-fx" GUI out-event the colour-
// calibration screen publishes (X360-attested by the AddGuiOutEvent instantiation
// @0x82465D98: AddEvent(&event, /*id*/546, /*X360 record*/12) -- a ResourceHandle qword
// plus the restore flag; no GuiEvent header rides in the X360 image, so the type id is
// carried by GetEventType() alone, matching the CgsGuiModuleIO AddGuiOutEvent generic).
struct GuiOptionsBrightnessContrastPostFxControl : public CgsModule::Event
{
    CgsResource::ResourceHandle mColourCalibrationTextureHandle;  // +0x00 (the X360 qword)
    // DWARF BrnGuiEventTypeDefs.h:6441 names this member mbEnablePostFx; the tree carried the
    // placeholder mbRestoreDefaults until this wave. Polarity is unchanged and was never in
    // doubt: ColourCalibrationScreen::Update stores FALSE while the ramp is on screen (X360
    // case 3, `stb r31 (=0)` @0x8246AD3C) and TRUE when it hides (case 4, `li r11,1; stb`
    // @0x8246AD64-70), and BridgeGuiToGame copies it straight into
    // mbEnableCalibrationUnfriendlyPostFx. Renamed to the DWARF name (rung 2 is the authority
    // for member NAMES) -- see the step-10 calib report section 6.
    // ⚠️ The MEMBER ORDER stays {handle, flag}: DWARF orders it {bool, ResourceHandle} but the
    // X360 image is {ResourceHandle, bool} (`std r11, var_100` at payload +0, `stb` at +8,
    // AddEvent(..., 546, 12) @0x82465D98) and the BINARY wins over the DWARF for layout.
    bool                        mbEnablePostFx;                   // +0x08 (1 = post-fx back on)

    s32 GetEventType() const { return 546; }
};

// The "activate / deactivate the CrashNav (front-end map) flow" GUI event. X360-attested
// by the OutputGuiEvent<BrnGui::GuiEventActivateCrashNav> instantiation @0x82493938: the
// queued record is { muHeader0 = 8 (payload bytes), muEventType = 191, muHeader2 = 12
// (payload offset) } + two payload words, channel 40, 20 bytes. BrnGui::PauseScreen posts
// it as { 1, 0 } when the player picks the pause option that quits to CrashNav. ADDITIVE
// GROW (BrnPauseScreen TU): the second payload word's role is not recovered (the pause
// screen leaves it 0).
struct GuiEventActivateCrashNav : public CgsGui::GuiEvent<191>
{
    u32 muActivate;   // +0x0C payload word 0: nonzero = activate CrashNav
    u32 muParam;      // +0x10 payload word 1 (role not recovered; posted as 0)

    explicit GuiEventActivateCrashNav(bool lbActivate)
        : CgsGui::GuiEvent<191>(8, 12), muActivate(lbActivate ? 1u : 0u), muParam(0) {}
};

// Declaration mirror of the DWARF parent (BrnGuiEventTypeDefs.h:1902). Only the nested
// EIconDisplayType enum is modelled -- it is the type of SatNavRenderer::meIconDisplayType
// (which "event set" the sat-nav renderer draws). ADDITIVE GROW (BrnSatNavRenderer TU):
// adds the enum at its DWARF home; nothing existing is changed.
struct GuiEventEnableSatNavIcons
{
    enum EIconDisplayType
    {
        E_ICON_DISPLAY_TYPE_OFFLINE_EVENTS        = 0,
        E_ICON_DISPLAY_TYPE_ONLINE_EVENT_STARTS   = 1,
        E_ICON_DISPLAY_TYPE_ONLINE_CHECKPOINTS    = 2,
        E_ICON_DISPLAY_TYPE_ONLINE_FINISH_POINTS  = 3,
        E_ICON_DISPLAY_TYPE_ONLINE_EVENT_PRESETS  = 4,
        E_ICON_DISPLAY_TYPE_COUNT                 = 5,
    };
};

// ===================================================================================
// BrnGui::GuiHudMessage -- one HUD message on its way to the message director: the
// hashed message id plus up to 4 formatted parameters per display string (3 strings).
// DWARF home BrnGuiEventTypeDefs.h:5610 (GuiEvent<152>). The Construct/AddParam
// bodies are REAL X360 functions, their own ledger rows (declaration-only here;
// AddParam(type, string, const char*) is the @0x824EB??? family the analyzer drives,
// the s32 overload @0x824EB508). X360 sizeof 840 (the analyzer's delayed-message
// memcpy length @0x8251E3xx).
// ===================================================================================
struct GuiHudMessage : public CgsGui::GuiEvent<152>
{
    static const s32 KI_NUMBER_OF_STRINGS     = 3;   // DWARF h:5613
    static const s32 KI_MAX_PARAMS_PER_STRING = 4;   // DWARF h:5614

    CgsID mMessageIdHash;   // DWARF h:5616 (public)

    // DWARF h:5621/h:5626 -- hash lpcMessageId (or adopt the hash) and clear the
    // parameter counts.
    void Construct(const char* lpcMessageId);
    void Construct(CgsID lMessageIdHash);

    // DWARF h:5633/h:5640/h:5647 -- append one parameter to display string
    // liStringIndex.
    void AddParam(CgsGui::HudMessageParamTypes leType, s32 liStringIndex, const char* lpcValue);
    void AddParam(CgsGui::HudMessageParamTypes leType, s32 liStringIndex, s32 liValue);
    void AddParam(CgsGui::HudMessageParamTypes leType, s32 liStringIndex, f32 lfValue);

    // DWARF h:5654 (GetParam) @0x82674D60 -- copy display-string liStringIndex's parameter
    // liParamIndex into *lpOut. Returns the CgsUnicode::SafelyTerminate result (X360 r3).
    CgsUnicode::CgsUtf8* GetParam(CgsGui::HudMessageParameter* lpOut,
                                  s32 liStringIndex, s32 liParamIndex) const;

    // @0x82674F20 -- parameter count for display string liStringIndex (maiNoOfParams[i]).
    s32 GetParamCount(s32 liStringIndex) const;

private:
    s32                        maiNoOfParams[KI_NUMBER_OF_STRINGS];                          // DWARF h:5663
    CgsGui::HudMessageParameter maaParams[KI_NUMBER_OF_STRINGS][KI_MAX_PARAMS_PER_STRING];   // DWARF h:5664
};

// ===================================================================================
// BrnGui::GuiLiveRevengeUpdateEvent -- an online live-revenge status change. DWARF
// home BrnGuiEventTypeDefs.h:3722 (GuiEvent<364>); consumed by
// HudMessageAnalyzer::HandleLiveRevengeUpdate @0x8251E1F0 (which reads the four
// payload fields in declaration order).
// ===================================================================================
// X360 AddGuiEvent<GuiLiveRevengeUpdateEvent> @0x823CF580 bakes id 369 (was PS3-DWARF 364).
struct GuiLiveRevengeUpdateEvent : public CgsGui::GuiEvent<369>
{
    s32                 miDifference;                   // DWARF h:3725 (the points delta)
    s32                 meNewStatus;                    // DWARF h:3726 (BrnNetwork LiveRevengeStatus; raw s32 -- enum home pending)
    EActiveRaceCarIndex meAggressorActiveRaceCarIndex;  // DWARF h:3727
    EActiveRaceCarIndex meVictimActiveRaceCarIndex;     // DWARF h:3728
};

// ===================================================================================
// Online Stunt Run HUD-event family -- the events BrnOnlineStuntRunMode publishes to the
// GUI that BrnGui::HudMessageAnalyzer::Handle* consume (X360 @0x8251FA90..0x8252006C). No
// DecFIGS DWARF entry exists for these payloads, so the member LAYOUT (offsets/widths) is
// X360-asm-authoritative (the handlers' lwz/lbz displacements) and only `mPlayerId` is named
// verbatim -- it appears in the X360 assert rodata
// ("lp<Event>->mPlayerId == CgsNetwork::K_INVALID_PLAYER_ID").
//
// FLAG: the remaining member NAMES are inferred from the handlers' behaviour and the HUD
// message-ids they select (OnlSr == Online Stunt Run; Pt/Tp == teammate variant, Rt/Ri ==
// rival variant, Lp == "last player standing"); their DWARF names are not recovered. The
// GuiEvent<N> base is EBO-empty (the handlers touch no base bytes), so these payloads are
// modelled as plain structs and their event-id N is not recovered. `mPlayerId` is a
// BrnNetwork::NetworkPlayerID (typedef s32); modelled as s32 to keep this GUI header off the
// network include (K_INVALID_PLAYER_ID == -1).
// ===================================================================================

// The three player-keyed status events (elimination / leading / victory) share the IDENTICAL
// X360 layout but are distinct event types with distinct handlers/assert-strings, so each is
// its own named struct.
//
//   +0x00  mbIsLocalPlayer  (s32; set == the event concerns the LOCAL player, no remote id)
//   +0x04  mPlayerId        (s32; the remote player's network id, -1 when local)   [asm-named]
//   +0x08  mbIsTeammate     (bool; selects the Pt/Tp teammate message vs the Rt/Ri rival one)
//   +0x09  mbIsLastPlayer   (bool; selects the Lp "last player standing" message)

struct GuiOnlineStuntRunEliminationEvent
{
    s32  mbIsLocalPlayer;   // +0x00
    s32  mPlayerId;         // +0x04
    bool mbIsTeammate;      // +0x08
    bool mbIsLastPlayer;    // +0x09
};

struct GuiOnlineStuntRunLeadingEvent
{
    s32  mbIsLocalPlayer;   // +0x00
    s32  mPlayerId;         // +0x04
    bool mbIsTeammate;      // +0x08
    bool mbIsLastPlayer;    // +0x09
};

struct GuiOnlineStuntRunVictoryEvent
{
    s32  mbIsLocalPlayer;   // +0x00
    s32  mPlayerId;         // +0x04
    bool mbIsTeammate;      // +0x08
    bool mbIsLastPlayer;    // +0x09
};

// The stunt-run score/time notification (HandleOnlineStuntRunMessage @0x8251FF38).
//   +0x00  mPlayerId      (s32; the scoring player, resolved to a name for the team branch)
//   +0x04  meMessageType  (s32; 1 == score notification, 3 == time notification)
//   +0x08  miValue        (s32; the score or the time -- appended as an INT HUD param)
struct GuiOnlineStuntRunMessageEvent
{
    // FLAG: only the two values the handler branches on are recovered (1 == score, 3 == time);
    // 0/2 are not observed in the asm.
    enum EMessageType
    {
        E_ONLINE_STUNT_RUN_MESSAGE_SCORE = 1,
        E_ONLINE_STUNT_RUN_MESSAGE_TIME  = 3,
    };

    s32 mPlayerId;      // +0x00
    s32 meMessageType;  // +0x04
    s32 miValue;        // +0x08
};

// The "last run started" notification (HandleOnlineStuntRunLastRun @0x8251FEC8). The handler
// only null-checks the pointer and fires the parameterless "OnlSRLastRun" message; no field is
// read, so the payload layout is not recovered. FLAG: opaque pointer-only payload.
struct GuiOnlineStuntRunLastRunEvent {};

// The developer-challenges-completed notification (X360-only debug flow; id 596, 8 bytes).
// HandleDeveloperChallengeMessageDEBUG @0x824F9D48 reads ONE u64 at +0x00 (ldx @0x824F9D90,
// ld @0x824F9DE0) and OR-accumulates it into the analyzer's FastBitArray<15>; the member name
// is VERBATIM X360 assert rodata (BrnGuiHudMessageAnalyzer.cpp:5974).
// Moved here from BrnGuiDemangledEventTypes.h (where it was an opaque u8[8]) per that
// header's own migration note: recovered analyzer payloads belong in this catalogue.
struct GuiDeveloperChallengesCompleted
{
    CgsContainers::FastBitArray<15> mCompletedDeveloperChallenges;   // +0x00 (one u64)

    s32 GetEventType() const { return 596; }   // id 596 size 8 (raw; size not GuiEvent-shaped)
};

// ===================================================================================
// BrnGui::GuiOverlayFullInfoRequest -- "send me the current overlay's full info"
// (posted by BaseOverlayState::Update's WFINIT phase @0x824B2BD8: a header-only
// record {muHeader0=1, muEventType=186, muHeader2=12} on channel 40, 16 bytes;
// answered by the overlays director's full-info response on 187).
// ===================================================================================
struct GuiOverlayFullInfoRequest : public CgsGui::GuiEvent<186>
{
    // +0x0C -- pads the wire record to the X360's 16 bytes (AddEvent size 0x10
    // @0x824B2BDC). The X360 never writes this word (the queued copy carries stack
    // garbage past the 12-byte header), so it is deliberately left uninitialised.
    u32 muPad0C;

    GuiOverlayFullInfoRequest() : CgsGui::GuiEvent<186>(1, 12) {}
};

// ===================================================================================
// BrnGui::GuiOverlayCompleteEvent -- "this overlay finished" (posted by the running
// overlay state on leave; consumed by the overlays director). DWARF home
// BrnGuiEventTypeDefs.h:5833. EVENT-ID DIVERGENCE: the PS3 DWARF bases it on
// GuiEvent<187>, but on X360 the overlay wire ids are shifted by two -- the full-info
// response rides id 187 and this complete event rides id 189 (BaseOverlayState::OnLeave
// @0x824B2DC8 posts {muHeader0=16, muEventType=189, muHeader2=16} + a 16-byte payload
// on channel 40, 32 bytes total; GuiOverlaysDirector::Update @0x82520668 dispatches the
// hidden notification on 189). X360 wins.
// ===================================================================================
struct GuiOverlayCompleteEvent : public CgsGui::GuiEvent<189>
{
    // DWARF BrnGuiEventTypeDefs.h:5837 -- how the overlay was left.
    enum LeaveMethod
    {
        E_LEAVEMETHOD_NONE   = 0,
        E_LEAVEMETHOD_OK     = 1,
        E_LEAVEMETHOD_CANCEL = 2,
        E_LEAVEMETHOD_COUNT  = 3,
    };

    CgsID       mOverlayId;     // +0x10 (DWARF h:5846; 8-aligned past the 12-byte header,
                                //        matching the X360 record: payload offset 16)
    LeaveMethod meLeaveMethod;  // +0x18 (DWARF h:5847)

    // Headers per the OnLeave record: payload size 16, payload offset 16.
    GuiOverlayCompleteEvent() : CgsGui::GuiEvent<189>(16, 16) {}

    // DWARF h:5859 Construct(CgsID, LeaveMethod) -- fill the payload (the string-keyed
    // h:5853 overload is its own ledger function; not attested in this slice).
    void Construct(CgsID lOverlayId, LeaveMethod leLeaveMethod)
    {
        mOverlayId    = lOverlayId;
        meLeaveMethod = leLeaveMethod;
    }
};

// ===================================================================================
// BrnGui::GuiOverlayFullInfoResponse -- the full overlay description record the
// overlays director keeps (current + buffered) and publishes to the overlay flow on
// event 187 (the PS3 DWARF bases it on GuiEvent<185>; on X360 the id is carried
// out-of-band by AddEvent(., 187, 448) and the queued record starts straight at
// mNameId -- BaseOverlayState::UpdateWFInfo @0x824B25D0 reads the id at payload +0x00).
// DWARF home BrnGuiEventTypeDefs.h:5768; member names/order verbatim from the DWARF.
// X360-pinned offsets: mNameId @+0x00 / macName @+0x08 (GuiOverlaysDirector::
// HandleOverlayRequest @0x825162C8), meStyle @+0x18 (posted as the event-185 payload
// word by the director's Update), meIcon @+0x1C and macTitleId/macMessageId/
// maMessageParams/miMessageParamsUsed @+0x20/+0x40/+0x60/+0xE8 (BaseOverlayState::
// SetupOverlay @0x824B1690); the button tail -- Param @+0xEC/+0x154 BEFORE Id
// @+0x130/+0x198, used-flags @+0x150/+0x1B8 -- by BaseOkOverlayState::SetupOverlay
// @0x824B1BC0 / BaseOkCancelOverlayState::SetupOverlay @0x824B1C78 (the PS3 DWARF
// lists Id before Param inside each button block; the X360 loads win). Natural
// layout; X360 sizeof 448 (0x1C0).
// ===================================================================================
struct GuiOverlayFullInfoResponse
{
    static const s32 MKI_MAX_LENGTH_OF_STRING_ID   = 32;   // DWARF h:5772
    static const s32 MKI_MAX_LENGTH_OF_FLASH_FRAME = 32;   // DWARF h:5773
    static const s32 MKI_MAX_PARAMS_IN_MESSAGE     = 2;    // DWARF h:5774

    CgsID                     mNameId;                                       // +0x00 (0 == empty slot)
    char                      macName[13];                                   // +0x08 (printable overlay name)
    CgsGui::PopupStyle        meStyle;                                       // +0x18
    CgsGui::PopupIcons        meIcon;                                        // +0x1C
    char                      macTitleId[MKI_MAX_LENGTH_OF_STRING_ID];       // +0x20 (title loc-string id)
    char                      macMessageId[MKI_MAX_LENGTH_OF_STRING_ID];     // +0x40 (message loc-string id)
    CgsGui::GuiPopupParameter maMessageParams[MKI_MAX_PARAMS_IN_MESSAGE];    // +0x60 (0x44 stride)
    s32                       miMessageParamsUsed;                           // +0xE8
    CgsGui::GuiPopupParameter mButton1Param;                                 // +0xEC (type @+0xEC, text @+0xF0)
    char                      macButton1Id[MKI_MAX_LENGTH_OF_STRING_ID];     // +0x130 (button-1 loc-string id)
    bool                      mbButon1ParamUsed;                             // +0x150 (DWARF spelling)
    CgsGui::GuiPopupParameter mButton2Param;                                 // +0x154 (type @+0x154, text @+0x158)
    char                      macButton2Id[MKI_MAX_LENGTH_OF_STRING_ID];     // +0x198
    bool                      mbButon2ParamUsed;                             // +0x1B8 (-> pad to 0x1C0)
};

// ===================================================================================
// Road-rules event family (ADDITIVE GROW: BrnRoadRuleComponent.h TU). DWARF homes
// BrnGuiEventTypeDefs.h:113 (RoadRuleLeaderType) / :1071 (GuiEventRoadRuleEnter,
// GuiEvent<329>) / :1215 (GuiEventRoadRuleUpcomingRoads, GuiEvent<337>). Their
// Construct/Setup* bodies are their own ledger functions (declaration-only).
// ===================================================================================

// DWARF BrnGuiEventTypeDefs.h:113 -- who currently holds a road rule.
enum RoadRuleLeaderType
{
    E_ROADRULELEADERTYPE_AI     = 0,
    E_ROADRULELEADERTYPE_PLAYER = 1,
    E_ROADRULELEADERTYPE_FRIEND = 2,
    E_ROADRULELEADERTYPE_COUNT  = 3,
};

// The leader-name slot the road-rules events carry. H2 (2026-08-25): PlayerName is a
// 16-BYTE fixed name string, not an 8-byte hashed id -- pinned by two X360 witnesses:
// GetNameOfRule @0x82414074 (`slwi r11, type, 4; addi r6, r11+event, 0x4C` -- a 16-byte
// stride into the enter event's maFriendLeader block, then SPrintf'd as "%s") and
// HandleRoadRuleTargetUpdate @0x82435668 (a 16-byte memcpy per score type). The old
// {CgsID} stand-in FLAG is retired.
struct PlayerName
{
    char macName[16];   // X360: 16-byte fixed name string (SPrintf'd as "%s")
};

// The world/game actions the road-rules event Constructs consume (pointer-only).
struct RoadRulesEnterRoadAction;
struct RoadRulesUpdateTargetScoreAction;
struct UpcomingRoadChangeAction;

// DWARF :1071 -- "entered a road-ruled road" (the road-rules panel refresh
// payload). H2 (2026-08-25): the FULL X360 interior is now pinned (the old
// "unverified interior" FLAG is retired; the earlier "sizeof 120" note was wrong --
// HandleEnterRoadEvent @0x82413E30 copies exactly 0x70 == 112 bytes). Witnesses,
// all payload-relative:
//   +0x00 mRoadId          EnterRoad @0x82441460 `ld 0x3E8(this)`; HandleLeaveRoadEvent cmpld
//   +0x08 maAILeaderId     FLAG: slot only Construct-zeroed (std @+8/+0x10); no typed reader yet
//   +0x18/+0x20/+0x28      leader trios   Update @0x8243583C / HandleEnterRoadEvent loops
//   +0x30/+0x38/+0x40      best trios     Update / HandleRoadRuleEnd `lwz 0x418` / RefreshBestData
//   +0x48 miRoadIndex      HandleEnterRoadEvent `lwz 0x430(this)` (mTransitionData+0x48)
//   +0x4C maFriendLeader   GetNameOfRule 16-byte stride @+0x4C (PlayerName == char[16])
//   +0x6C mabChallenge     GuiEventRoadRuleEnter::Construct @0x824F60E4 (stbx @+0x6C+i)
// X360 AddGuiEvent<GuiEventRoadRuleEnter> @0x823D69F0 bakes id 333 (was PS3-DWARF 329).
struct GuiEventRoadRuleEnter : public CgsGui::GuiEvent<333>
{
    CgsID                 mRoadId;                                                         // :1083 (X360 +0x00)
    CgsID                 maAILeaderId[BrnStreetData::E_SCORE_TYPE_COUNT];                 // :1075 (X360 +0x08; FLAG slot)
    RoadRuleLeaderType    maeRoadRuleLeaderType[BrnStreetData::E_SCORE_TYPE_COUNT];        // :1076 (X360 +0x18)
    RoadRuleLeaderType    maeOfflineRoadRuleLeaderType[BrnStreetData::E_SCORE_TYPE_COUNT]; // :1077 (X360 +0x20)
    RoadRuleLeaderType    maeOnlineRoadRuleLeaderType[BrnStreetData::E_SCORE_TYPE_COUNT];  // :1078 (X360 +0x28)
    s32                   maiBestValues[BrnStreetData::E_SCORE_TYPE_COUNT];                // :1080 (X360 +0x30)
    s32                   maiBestOfflineValues[BrnStreetData::E_SCORE_TYPE_COUNT];         // :1081 (X360 +0x38)
    s32                   maiBestOnlineValues[BrnStreetData::E_SCORE_TYPE_COUNT];          // :1082 (X360 +0x40)
    BrnStreetData::RoadIndex miRoadIndex;                                                 // :1084 (X360 +0x48)
    PlayerName            maFriendLeader[BrnStreetData::E_SCORE_TYPE_COUNT];               // :1074 (X360 +0x4C)
    bool                  mabChallenge[BrnStreetData::E_SCORE_TYPE_COUNT];                 // :1079 (X360 +0x6C)

    // DWARF :1089 -- its own ledger function (declaration-only; the action-driven form).
    void Construct(const RoadRulesEnterRoadAction* lpAction);

    // @ 0x824F60B8 (DWARF :1093) -- reset: zero the road id, the AI-leader slots, the
    // ACTIVE leader/best pairs (the offline/online mirrors are deliberately left), the
    // road index, the friend-name lead bytes and the challenge flags. Bodied in
    // Events/BrnGuiEventRoadRuleUpcomingRoads.cpp (the road-rule event family TU).
    void Construct();
};

// DWARF :1215 -- the two upcoming junction roads (sides, states, rulers). X360
// sizeof 128 (RoadRuleComponent block-copies it @+0x460 / to a local).
// H2 (2026-08-25): FULL X360 payload order pinned, REORDERED off the PS3 DWARF (same
// mRoadId-first drift as the family siblings). Witnesses, all payload-relative:
//   +0x00 mRoadIds[2]              IsSameAsCurrentRoad ldx @side*8; ShowUpcomingRoads ld 0x460/0x468
//   +0x10 maRoadEntrancePosition   UpdateUpcomingRoadSign @0x8243FE20 lvx event+0x10+16*side ->
//                                  maRoadWorldPosition (the old "+0x60" note was WRONG)
//   +0x30/+0x40/+0x50 leader blocks [side][scoretype] (side stride 8, type stride 4):
//                                  UpdateUpcomingRoadLeaders slots; GetRoadSignColour @+0x490/+0x498
//   +0x60 maiTurningRoadIndices    Construct @0x824F6144 (stores -1); ShowUpcomingRoads lwz 0x4C0/0x4C4
//   +0x68 miCurrentRoadIndex       Construct @0x824F6164 (-1); GetSignOffsetSizeAdjustment lwz 0x4C8
//   +0x6C meRoadStates[2]          Construct zeroes; HandleRoadRuleBegin lwz 0x4CC/0x4D0
//   +0x74 meCurrentSignState       Construct @0x824F615C (:= 3), tail-pad to 0x80
// X360 AddGuiEvent<GuiEventRoadRuleUpcomingRoads> @0x823D6E40 bakes id 341 (was PS3-DWARF 337).
struct GuiEventRoadRuleUpcomingRoads : public CgsGui::GuiEvent<341>
{
    // DWARF :1218 / :1226.
    enum ERoadSide
    {
        E_ROAD_LEFT  = 0,
        E_ROAD_RIGHT = 1,
        E_ROAD_COUNT = 2,
    };
    enum ERoadState
    {
        E_ROADSTATE_NORMAL    = 0,
        E_ROADSTATE_SUGGESTED = 1,
        E_ROADSTATE_WRONG     = 2,
        E_ROADSTATE_COUNT     = 3,
    };

    CgsID              mRoadIds[E_ROAD_COUNT];                                                 // :1238 (X360 +0x00)
    Vector3            maRoadEntrancePosition[E_ROAD_COUNT];                                   // :1243 (X360 +0x10)
    RoadRuleLeaderType maaeLeaderTypes[E_ROAD_COUNT][BrnStreetData::E_SCORE_TYPE_COUNT];        // :1235 (X360 +0x30)
    RoadRuleLeaderType maaeOfflineLeaderTypes[E_ROAD_COUNT][BrnStreetData::E_SCORE_TYPE_COUNT]; // :1236 (X360 +0x40)
    RoadRuleLeaderType maaeOnlineLeaderTypes[E_ROAD_COUNT][BrnStreetData::E_SCORE_TYPE_COUNT];  // :1237 (X360 +0x50)
    BrnStreetData::RoadIndex maiTurningRoadIndices[E_ROAD_COUNT];                              // :1239 (X360 +0x60)
    BrnStreetData::RoadIndex miCurrentRoadIndex;                                               // :1240 (X360 +0x68)
    ERoadState         meRoadStates[E_ROAD_COUNT];                                             // :1241 (X360 +0x6C)
    ERoadState         meCurrentSignState;                                                     // :1242 (X360 +0x74)

    // @ 0x824F6108 (DWARF :1247) -- reset: zero the road ids / entrance positions /
    // ACTIVE leader block (the offline/online mirrors deliberately untouched), turning
    // indices := KI_INVALID_ROAD_INDEX (-1), road states := NORMAL, current index := -1,
    // current sign state := E_ROADSTATE_COUNT. Bodied in
    // Events/BrnGuiEventRoadRuleUpcomingRoads.cpp (the road-rule event family TU).
    void Construct();

    // DWARF :1252 -- its own ledger function (declaration-only; the action-driven form).
    void Construct(const UpcomingRoadChangeAction* lpAction);

    // @ 0x824F6170 (truncated export name "ConvertG") -- map a 3-valued game-state
    // enum (0..2) to this event's road category id (0->0, 1->2, 2->1) with two
    // non-fatal range guards. Static because the X360 body never reads `this`.
    // Re-homed here 2026-08-25 from Events/BrnGuiEventRoadRuleUpcomingRoads.h, whose
    // own local `GuiEventRoadRuleUpcomingRoads : CgsModule::Event` was an ODR fork of
    // this struct (that header is retired).
    static s32 ConvertGameStateToCategory(u32 luGameState);
};

// DWARF :1111 (PS3 GuiEvent<335>; X360 id 339). H2 (2026-08-25): full record, X360
// FIELD ORDER (the PS3 DWARF leads with maFriendLeader; the X360 leads with the road
// id -- the family drift again). Consumer witness HandleRoadRuleTargetUpdate
// @0x824355DC..: `ld 0(event)` road id; the 16-byte-per-type memcpy from event+0x08;
// leader words from event+0x28; best values from event+0x30.
struct GuiEventRoadRuleUpdateTargetScores : public CgsGui::GuiEvent<339>
{
    CgsID              mRoadId;                                                       // :1117 (X360 +0x00)
    PlayerName         maFriendLeader[BrnStreetData::E_SCORE_TYPE_COUNT];             // :1114 (X360 +0x08)
    RoadRuleLeaderType maeRoadRuleLeaderType[BrnStreetData::E_SCORE_TYPE_COUNT];      // :1115 (X360 +0x28)
    s32                maiBestValues[BrnStreetData::E_SCORE_TYPE_COUNT];              // :1116 (X360 +0x30)

    // DWARF :1122 -- its own ledger function (declaration-only).
    void Construct(const RoadRulesUpdateTargetScoreAction* lpAction);
};

// DWARF :1168 (PS3 GuiEvent<332>; X360 id 336). H2 (2026-08-25): full record; here
// the DWARF order HOLDS on X360 -- consumer witness HandleRoadRuleEnd @0x8243F27C:
// `ld 0(event)` road id, `lwz 8` rule type, `lfs 0xC` score, `lbz 0x10` attempt flag.
struct GuiEventRoadRuleEnd : public CgsGui::GuiEvent<336>
{
    CgsID                    mRoadId;         // :1170 (X360 +0x00)
    BrnStreetData::ScoreType meRuleType;      // :1171 (X360 +0x08)
    f32                      mfScore;         // :1172 (X360 +0x0C; seconds for the time rule)
    bool                     mbScoreAttempt;  // :1173 (X360 +0x10)
};

// DWARF home BrnGuiEventTypeDefs.h:4727 -- the pending junction/event-start info pushed to
// the JunctionInfo HUD panel (BrnGui::JunctionInfoComponent). GuiEvent<309> is EBO-empty (a
// type-tag base with only a non-virtual GetEventType()), so the first data member sits at
// struct offset 0 and the whole record is a flat 0x20-byte POD -- X360-attested by
// JunctionInfoComponent::HandleJunctionChange (a 4-qword *lpEvent copy into this+0x100 with
// stb 0 @ this+0x10C == mi8Difficulty at struct+0x0C, requiring CgsID==8B) and
// SetupAptVariables (ld @+0x100 == mSpecialEventCarId, lwz @+0x108 == meGameModeType).
// X360 AddGuiEvent<GuiEventJunctionInfo> @0x823D1AE0 bakes id 311 (was PS3-DWARF 309);
// corrected to the X360 event id. (The record SIZE the producer copies is 32 bytes -- see
// GuiModule::AddGuiEvent; the base is layout-neutral so only GetEventType() changes.)
// id 169 size 12 -- the change-district record (HUD H1 wave, 2026-08-25; UPGRADED here from
// BrnGuiDemangledEventTypes.h's opaque `GuiEvent<169> {}` placeholder, which did not match
// the wire). The record is FLAT: three words at offset +0, no GuiEvent header. Pinned on the
// consumer side by GuiCache::RecEvent @0x8250DDF0 case 169 (three word copies from +0/+4/+8
// into cache+20384/20388/20392), on the producer side by TranslateGameActionsToGuiEvents
// @0x823E9CE0 case 112 (the game action's 8-byte {county,district} pair + a zeroed third
// word), and re-posted by FBurnMainHudState::UpdateRunning's marker refresh with the
// consumed byte set. The flag word's tested byte is its FIRST byte (BE `lbz` @+8 in the
// state's post-loop); modelled as a leading u8 so the LE host tests the authored byte.
// Raw-record form (no GuiEvent base), the committed HudMessageAnalyzer-family recipe.
struct GuiEventChangeDistrict
{
    s32 meCounty;               // +0x00 BrnWorld::ECounty
    s32 meDistrict;             // +0x04 BrnWorld::EDistrict
    u8  mu8Consumed;            // +0x08 0 == fresh, 1 == consumed by the HUD marker
    u8  maPad[3];               // +0x09..+0x0B
    s32 GetEventType() const { return 169; }
};

struct GuiEventJunctionInfo : public CgsGui::GuiEvent<311>
{
    CgsID  mSpecialEventCarId;                                        // +0x00
    BrnGameState::GameStateModuleIO::EGameModeType meGameModeType;    // +0x08
    s8     mi8Difficulty;                                             // +0x0C
    s8     mi8MedalAchieved;                                          // +0x0D
    s32    miEventID;                                                 // +0x10
    bool   mbCanEnterEvent;                                           // +0x14
    bool   mbEventUnlocked;                                           // +0x15
    bool   mbOnEntry;                                                 // +0x16
    bool   mbSpecificCarEventValid;                                   // +0x17
    bool   mbIsNewlyDiscovered;                                       // +0x18
    bool   mbIsAutoUnlockedChallenge;                                 // +0x19
    // pad to 0x20 (u64 alignment of the leading CgsID; total record size 0x20)
};

// ===================================================================================
// HudMessageAnalyzer event-payload family (ADDITIVE GROW: BrnGuiHudMessageAnalyzer.cpp
// keystone). The BrnGui GUI-event payloads the HUD-message analyzer's Handle* methods
// consume, upgraded from their BrnGuiDemangledEventTypes.h opaque placeholders to their
// real DWARF shapes (the placeholder entries are REMOVED from that header -- exactly one
// definition per type; CgsGuiModule_AddGuiEvent_Inst.cpp / the OutputGuiEvent Inst TU
// include both headers and keep compiling).
//
// MODELLING (X360-authoritative): the X360 queue records for this family carry their
// FIELDS AT RECORD OFFSET +0 -- pinned on the consumer side by the analyzer handlers'
// field reads (e.g. HandleTakedown reads meAggressorIndex at record+0x10) and on the
// producer side by the game-state bridge stores (GameBridgeGameStateToX.h documents the
// takedown stores at +0x00..+0x25). A 12-byte CgsGui::GuiEvent<N> base would displace
// every field, so these payloads are FLAT structs carrying their X360 wire id via
// GetEventType() (the raw-record form of the BrnGuiDemangledEventTypes recipe; same
// treatment as the committed GuiOnlineStuntRun* family above). Each struct's sizeof is
// pinned to the X360 AddGuiEvent/AddEvent size literal where one is attested.
//
// PS3-DWARF -> X360 id drift: the DWARF GuiEvent<N> template ids are the PS3 wire ids;
// the X360 ids (used here) come from the X360 AddGuiEvent instantiation literals (the
// former demangled-header entries) or, where no producer instantiation exists, from the
// HudMessageAnalyzer::Update dispatch case that routes the payload to its handler
// (jumptable bases: 82526234 = id 64+case, 82526838 = id 313+case, 82526BF0 = id
// 419+case -- X360 @0x82525FC0).
// ===================================================================================

// DWARF BrnGuiEventTypeDefs.h (before :113) -- which stunt-collectible family an
// area/all-complete event talks about.
enum StuntType
{
    E_STUNTTYPE_JUMP  = 0,
    E_STUNTTYPE_SMASH = 1,
    E_STUNTTYPE_STUNT = 2,
    E_STUNTTYPE_COUNT = 3,
};

// DWARF BrnGuiEventTypeDefs.h:562 area -- the crash-bar state-change payload
// (GuiEvent<373> on PS3; X360 wire id 377, record = the single state word).
// The analyzer stores meCurrentState in its meCrashEntryState member and keys the
// delayed-message flow off it (HandleCrashedEvent switch).
struct GuiPlayerCrashingStateChangeEvent
{
    // DWARF :567.
    enum CrashBarState
    {
        E_CRASHBARSTATE_INVALID        = -1,
        E_CRASHBARSTATE_START_CRASHED  = 0,
        E_CRASHBARSTATE_LEAVE_CRASHED  = 1,
        E_CRASHBARSTATE_START_TAKEDOWN = 2,
        E_CRASHBARSTATE_LEAVE_TAKEDOWN = 3,
        E_CRASHBARSTATE_COUNT          = 4,
    };

    CrashBarState meCurrentState;   // DWARF :1292; +0x00 (the whole 4-byte record)

    s32 GetEventType() const { return 377; }
};
static_assert(sizeof(GuiPlayerCrashingStateChangeEvent) == 4, "X360 record size 4 (id 377)");

// [hud reveal gate 2026-08-25] X360 id 379, record 4 bytes -- THE IGNITION LATCH, and the
// free-burn HUD's reveal gate. Moved here from BrnGuiDemangledEventTypes.h (where it was an
// opaque `u8 maData[4]`) now that its enum is recovered. It is GuiPlayerCrashingStateChangeEvent's
// literal neighbour: BridgeWorldVehicleDataToGui @0x823E5768 posts the crashing 377 first, then
// GuiPlayerDrivableFromCrash 378, then this, all before the IsPlayerCarActive gate.
//
// The enumerator NAMES are the console's own, lifted verbatim from the assert string baked at
// BrnFBurnMainHudState.cpp:1536 --
//   "( GuiPlayerEngineEvent::E_ENGINE_OFF == mpCache->GetPlayerEngineState( )) ||
//    ( GuiPlayerEngineEvent::E_ENGINE_ON  == mpCache->GetPlayerEngineState( ))"
// -- and their VALUES are pinned by that same assert's companion code: UpdateWFInit
// @0x8247C710 range-asserts the word `< 2` and then branches on `== 1`, so OFF is 0, ON is 1.
//
// ⚠️ These are NOT the world-side EActiveRaceCarEngineState values (OFF/STARTING/RUNNING/
// STOPPING/COUNT == 0..4). The producer narrows RUNNING(2) to this ON(1) bool, which is
// exactly why a raw STARTING->STOPPING move changes the world word but posts nothing here.
//
// The record is kept as the attested opaque word rather than a typed member: the console's
// AddGuiEvent<T> takes sizeof(T) and the payload's field breakdown beyond "one word" is not
// recovered, so naming it would be inventing structure.
struct GuiPlayerEngineEvent
{
    enum EEngineState : s32
    {
        E_ENGINE_OFF = 0,
        E_ENGINE_ON  = 1
    };

    u8 maData[4];

    s32 GetEventType() const { return 379; }
};
static_assert(sizeof(GuiPlayerEngineEvent) == 4, "X360 record size 4 (id 379)");

// DWARF BrnGuiEventTypeDefs.h:3565 area (GuiEvent<358>; X360 id 363, record 40 bytes).
// X360 FIELD ORDER (binary authoritative, differs from the PS3-DWARF member listing):
// the car-id qwords lead and the race-car indices follow -- pinned on BOTH sides:
//   producer: BrnGame::TranslateTakedownsToGuiEvents @0x823E1C38 (see the store map in
//             GameBridgeGameStateToX.h: ids @+0x00/+0x08, indices @+0x10/+0x14,
//             type @+0x18, chain @+0x1C, multi @+0x20, status bytes @+0x24/+0x25)
//   consumer: HudMessageAnalyzer::HandleTakedown @0x8251C3C0 (reads +0x10/+0x14/+0x18/
//             +0x24/+0x25) and ConstructTakedownMessage @0x824F9C28 (reads type @+0x18,
//             chain @+0x1C with the <2 / -2<9 ladder, multi @+0x20 with the <=1 gate).
// Member NAMES from the DWARF.
// [gateui r3] STALE NOTE CORRECTED: this used to say "GameBridgeGameStateToX.h keeps a
// file-local FLAGGED placeholder of this record for its own TU; the include graphs do not
// meet." Both halves are now false -- the graphs DO meet (that header reaches this one), the
// placeholder was a straight C2011 redefinition, and it has been deleted. That header now
// includes this one and uses THIS definition; see its own banner for the fork's history.
struct GuiTakedownEvent
{
    CgsID                       mAggressorCarID;          // +0x00
    CgsID                       mVictimCarID;             // +0x08
    EActiveRaceCarIndex         meAggressorIndex;         // +0x10
    EActiveRaceCarIndex         meVictimIndex;            // +0x14
    BrnGameState::ETakedownType meTakedownType;           // +0x18 (KAPC_TAKEDOWN_TYPES index; 3 == revenge/settled special case)
    s32                         miTakedownChainCount;     // +0x1C
    s32                         miMultipleTakedownCount;  // +0x20
    bool                        mbMarkedManTakeDown;      // +0x24
    bool                        mbSettledScore;           // +0x25
    // 2 tail-pad bytes -> the X360 40-byte record

    s32 GetEventType() const { return 363; }
};
static_assert(sizeof(GuiTakedownEvent) == 40, "X360 AddGuiEvent size 40 (id 363)");

// GuiSoftTakedownEvent (id 364, 32 bytes) -- [boost-msg wave 2026-08-26] RECOVERED, retiring
// BOTH the opaque BrnGuiDemangledEventTypes.h shell AND the soft arm this tree had parked:
// GameBridgeGameStateToX.cpp's TranslateTakedownsToGuiEvents decoded the producer stores
// (@0x823E1CDC..0x823E1D1C: two CgsIDs at +0x00/+0x08, the two indices at +0x10/+0x14, the
// takedown type at +0x18 and the two status bytes pulled forward to +0x1C/+0x1D) and asked
// for exactly this grow. The consumer half corroborates: AddGuiEvent<GuiSoftTakedownEvent>
// @0x823D9AD0 -> AddEvent(q, ev, 364, 32), and BoostMessageManager::RecvEvent case 364
// compares +0x10 (the aggressor index) against GuiCache::mePlayerActiveRaceCarIndex
// (@0x82420B18). Same family layout as GuiTakedownEvent above, minus the two chain counts.
struct GuiSoftTakedownEvent
{
    CgsID                       mAggressorCarID;     // +0x00 <- producer src +0x08
    CgsID                       mVictimCarID;        // +0x08 <- producer src +0x10
    EActiveRaceCarIndex         meAggressorIndex;    // +0x10 <- producer src +0x00
    EActiveRaceCarIndex         meVictimIndex;       // +0x14 <- producer src +0x04
    BrnGameState::ETakedownType meTakedownType;      // +0x18 <- producer src +0x18
    bool                        mbMarkedManTakeDown; // +0x1C <- producer src +0x24
    bool                        mbSettledScore;      // +0x1D <- producer src +0x26
    u8                          maPad1E[2];          // +0x1E tail pad to the 32-byte record

    s32 GetEventType() const { return 364; }
};
static_assert(sizeof(GuiSoftTakedownEvent) == 32, "X360 AddGuiEvent size 32 (id 364)");
static_assert(__builtin_offsetof(GuiSoftTakedownEvent, meAggressorIndex) == 0x10,
              "X360 RecvEvent case 364 compares +0x10 to the active race-car index");

// (GuiShutdownEvent -- DWARF :3618 {CgsID mVictimCarID} -- KEEPS its
// BrnGuiDemangledEventTypes.h placeholder, id 373 size 8: HandleShutdown is not part of
// this keystone's fan-out and the analyzer header only forward-declares the type.)

// DWARF :3645 (GuiEvent<360>; X360 id 365, record 12 bytes). HandleImpact @0x824F2E48
// guards meImpactType in (0, 9).
struct GuiImpactEvent
{
    BrnPhysics::Vehicle::EImpactType meImpactType;                 // +0x00
    EActiveRaceCarIndex              meAggressorActiveRaceCarIndex; // +0x04
    EActiveRaceCarIndex              meVictimActiveRaceCarIndex;    // +0x08

    s32 GetEventType() const { return 365; }
};
static_assert(sizeof(GuiImpactEvent) == 12, "X360 AddGuiEvent size 12 (id 365)");

// DWARF :3708 (GuiEvent<363>; X360 id 368 = Update dispatch 313+55). HandleSignatureStunt
// @0x8251D7E8 reads the id qword @+0x00.
struct GuiSignatureStuntEvent
{
    CgsID mId;   // +0x00

    s32 GetEventType() const { return 368; }
};

// DWARF :3741/:3779/:3846 -- the online dirty-trick (payback) trio (PS3 GuiEvent<175/177/179>;
// X360 ids 177/179/181, records 12/12/16 bytes). The analyzer reads aggressor @+0x00,
// victim @+0x04, trick type @+0x08 (guarded < 3 -- the 3-entry X360 Pbk tables) and, on
// the ended event only, the survived byte @+0x0C. NOTE: the X360 'no trick pending'
// sentinel the analyzer stores is 3 (the PS3 six-axis slot of BrnNetwork::EPaybackType).
struct GuiDirtyTrickNewEvent
{
    EActiveRaceCarIndex      meAggressorActiveRaceCarIndex;  // +0x00
    EActiveRaceCarIndex      meVictimActiveRaceCarIndex;     // +0x04
    BrnNetwork::EPaybackType meTrickType;                    // +0x08

    s32 GetEventType() const { return 177; }
};
static_assert(sizeof(GuiDirtyTrickNewEvent) == 12, "X360 AddGuiEvent size 12 (id 177)");

struct GuiDirtyTrickTriggerEvent
{
    EActiveRaceCarIndex      meAggressorActiveRaceCarIndex;  // +0x00
    EActiveRaceCarIndex      meVictimActiveRaceCarIndex;     // +0x04
    BrnNetwork::EPaybackType meTrickType;                    // +0x08

    s32 GetEventType() const { return 179; }
};
static_assert(sizeof(GuiDirtyTrickTriggerEvent) == 12, "X360 AddGuiEvent size 12 (id 179)");

struct GuiDirtyTrickEndedEvent
{
    EActiveRaceCarIndex      meAggressorActiveRaceCarIndex;  // +0x00
    EActiveRaceCarIndex      meVictimActiveRaceCarIndex;     // +0x04
    BrnNetwork::EPaybackType meTrickType;                    // +0x08
    bool                     mbSurvived;                     // +0x0C

    s32 GetEventType() const { return 181; }
};
static_assert(sizeof(GuiDirtyTrickEndedEvent) == 16, "X360 AddGuiEvent size 16 (id 181)");

// DWARF :3812/:3828 (PS3 GuiEvent<479/480>; X360 ids 484/485, records 16 bytes).
// HandleTookLead/HandleTookLast read the index @+0x08 (bounds < 8).
struct GuiTookLeadEvent
{
    CgsID               mOfflineRivalCarID;        // +0x00
    EActiveRaceCarIndex meLeadActiveRaceCarIndex;  // +0x08

    s32 GetEventType() const { return 484; }
};
static_assert(sizeof(GuiTookLeadEvent) == 16, "X360 AddGuiEvent size 16 (id 484)");

struct GuiTookLastEvent
{
    CgsID               mOfflineRivalCarID;        // +0x00
    EActiveRaceCarIndex meLastActiveRaceCarIndex;  // +0x08

    s32 GetEventType() const { return 485; }
};
static_assert(sizeof(GuiTookLastEvent) == 16, "X360 AddGuiEvent size 16 (id 485)");

// DWARF :3908 (PS3 GuiEvent<370>; X360 id 375, record 16 bytes). The analyzer parks a
// copy in mTrophyCarUnlockedEvent (Update id-375 case copies the 2 qwords) and
// HandleTrophyCarUnlocked prints mTrophyCarID.
struct GuiEventTrophyCarUnlock
{
    // DWARF type BrnProgression::TrophyUnlockData::UnlockType -- raw s32 here (the
    // trophy-progression home is not yet committed; the analyzer only forwards it).
    s32   meUnlockType;    // +0x00
    CgsID mTrophyCarID;    // +0x08

    s32 GetEventType() const { return 375; }
};
static_assert(sizeof(GuiEventTrophyCarUnlock) == 16, "X360 AddGuiEvent size 16 (id 375)");

// DWARF :4323/:4353 (PS3 GuiEvent<477/481>; X360 ids 482/486, records 16 bytes). Both
// carry a rival car id qword and the race-car index @+0x08 the analyzer gamer-tags.
struct GuiNetworkPlayerCrashingEvent
{
    CgsID               mRivalCarID;                         // +0x00
    EActiveRaceCarIndex meNetworkPlayerActiveRaceCarIndex;   // +0x08

    s32 GetEventType() const { return 482; }
};
static_assert(sizeof(GuiNetworkPlayerCrashingEvent) == 16, "X360 AddGuiEvent size 16 (id 482)");

struct GuiNetworkPlayerOnTailEvent
{
    CgsID               mOfflineRivalCarID;          // +0x00
    EActiveRaceCarIndex meOnTailActiveRaceCarIndex;  // +0x08

    s32 GetEventType() const { return 486; }
};
static_assert(sizeof(GuiNetworkPlayerOnTailEvent) == 16, "X360 AddGuiEvent size 16 (id 486)");

// DWARF :4461 (PS3 GuiEvent<148>; X360 id 150, record 16 bytes). HandleJoinedEvent
// prints the joining rival's id.
struct GuiEventCarJoinedEvent
{
    CgsID mJoiningRivalID;   // +0x00
    bool  mbDoCamera;        // +0x08

    s32 GetEventType() const { return 150; }
};
static_assert(sizeof(GuiEventCarJoinedEvent) == 16, "X360 AddGuiEvent size 16 (id 150)");

// DWARF :1542 typedef GuiEventCarEliminatedFromEvent = GuiEvent<149> (payload-less on
// PS3). X360 (id 151 = Update dispatch 64+87): HandleEliminatedEvent @0x8251D0E8 reads a
// SIGNED BYTE at record+0x00 (lbz;extsb) and indexes KAPC_FINISH_POSITION_MESSAGES[1 + it].
// FLAG: the field name is consumer-inferred (no DWARF member exists for the X360 payload).
struct GuiEventCarEliminatedFromEvent
{
    s8 mi8FinishPosition;   // +0x00 (0-based position; message id = POSITION_* [pos+1])

    s32 GetEventType() const { return 151; }
};

// DWARF :4528 (PS3 GuiEvent<217>; X360 id 219, record 8 bytes). "All collectibles of
// this stunt family in this county are done" -- HandleStuntsComplete @0x8251F6E8 fires
// "StntAllDone" with KAPC_COLLECTABLE_COMPLETION_STRINGID[meStuntElementType] (< 3) and
// KAPC_COUNTY_STRINGID[meCounty] (< 5).
struct GuiEventStuntAreaComplete
{
    StuntType         meStuntElementType;   // +0x00 (DWARF :2090)
    BrnWorld::ECounty meCounty;             // +0x04 (DWARF :2091)

    s32 GetEventType() const { return 219; }
};
static_assert(sizeof(GuiEventStuntAreaComplete) == 8, "X360 AddGuiEvent size 8 (id 219)");

// ===================================================================================
// [gateui] The rest of the stunt-collectible HUD family. These four payloads used to be
// opaque shells in BrnGuiDemangledEventTypes.h (the auto-derived table read a 12-byte
// record as "GuiEvent header only, no payload", and a 4-byte one as an anonymous byte
// blob). They are NOT header-only: their consumers read named words straight off the
// queued record, so the record IS the payload -- the same correction the committed
// GuiEventStuntAreaComplete / GuiEventProgressionProfileData entries carry.
//
// PS3-DWARF -> X360 id drift in this family is +2 (StuntInfo 215->217, BoostBar 216->218,
// AreaComplete 217->219, AllComplete 218->220); the sign-smash event drifts +5
// (395->400). Every (id,size) pair below is the X360 AddGuiEvent instantiation's two
// literals, read from the asm:
//   AddGuiEvent<GuiEventStuntInfo>          @0x823D3B38 -> AddEvent(q, ev, 217, 12)
//   AddGuiEvent<GuiEventBoostBarStuntInfo>  @0x823D3A80 -> AddEvent(q, ev, 218, 12)
//   AddGuiEvent<GuiEventStuntAllComplete>   @0x823D3CA8 -> AddEvent(q, ev, 220,  4)
//   AddGuiEvent<GuiHUDMessageSignSmashed>   @0x823D9960 -> AddEvent(q, ev, 400,  4)
// ===================================================================================

// DWARF :4515 (PS3 GuiEvent<215>; X360 id 217, record 12 bytes). "Billboards Smashed
// 12/45" -- the running collectible tally for ONE stunt family. HandleStuntInfo
// @0x8251F650 reads the three words at +0x00/+0x04/+0x08 in this order (`lwz r6,0(r31)`
// current, `lwz r6,4(r31)` total, `lwz r8,8(r31)` the type it scales by 8 to index the
// 3-entry CgsID message-name table KA_STUNT_INFO_MESSAGES).
struct GuiEventStuntInfo
{
    s32       miCurrentCount;   // +0x00 (DWARF :2081)
    s32       miTotalCount;     // +0x04 (DWARF :2082)
    StuntType meStuntType;      // +0x08 (DWARF :2083)

    s32 GetEventType() const { return 217; }
};
static_assert(sizeof(GuiEventStuntInfo) == 12, "X360 AddGuiEvent size 12 (id 217)");

// DWARF :4502 (PS3 GuiEvent<216>; X360 id 218, record 12 bytes). The IDENTICAL payload,
// posted instead of 217 by the game-action-58 translation when the current game mode
// wants the boost-bar-anchored presentation. It has no HudMessageAnalyzer::Update arm on
// the X360 (only 217 dispatches to HandleStuntInfo) -- its consumer is the HUD boost-bar
// component -- so it is a distinct type with its own id, not an alias.
struct GuiEventBoostBarStuntInfo
{
    s32       miCurrentCount;   // +0x00 (DWARF :2071)
    s32       miTotalCount;     // +0x04 (DWARF :2072)
    StuntType meStuntType;      // +0x08 (DWARF :2073)

    s32 GetEventType() const { return 218; }
};
static_assert(sizeof(GuiEventBoostBarStuntInfo) == 12, "X360 AddGuiEvent size 12 (id 218)");

// DWARF :4538 (PS3 GuiEvent<218>; X360 id 220, record 4 bytes). "Every collectible of
// this family is done, city-wide" -- HandleStuntsComplete @0x8251F7E8 bound-asserts the
// single word (`lwz r11,0(r31)`; `cmpwi r11,3`) and fires "StntAllDone" with the family
// completion string id and the literal "PARADISE_CITY".
struct GuiEventStuntAllComplete
{
    StuntType meStuntElementType;   // +0x00 (DWARF :2097)

    s32 GetEventType() const { return 220; }
};
static_assert(sizeof(GuiEventStuntAllComplete) == 4, "X360 AddGuiEvent size 4 (id 220)");

// DWARF :6194 (PS3 GuiEvent<395>; X360 id 400, record 4 bytes). The CRASH-MODE OVERHEAD
// SIGN score popup -- NOT a billboard/smash-gate event. Its only producer is
// BrnGame::BrnGameModule::TranslateShowtimeActionToGuiEvent @0x823E1988 case 128
// (E_ACTION_OVERHEAD_SIGN_HIT), whose whole translate call is gated on game mode 2/16;
// the feeder is PropEntityIO's mHitOverheadSignQueue -> CrashModeScoring::
// DealWithHitOverheadSign @0x82312928. HandleSignSmashMessage @0x8251BF40 fires
// "ShowSignHit" with the single word as an INT param on display string 2.
struct GuiHUDMessageSignSmashed
{
    s32 miPointsAwarded;   // +0x00 (DWARF :5358)

    s32 GetEventType() const { return 400; }
};
static_assert(sizeof(GuiHUDMessageSignSmashed) == 4, "X360 AddGuiEvent size 4 (id 400)");

// [gateui] The three crash-mode HUD-popup payloads that sat next to GuiHUDMessageSignSmashed
// as opaque byte blobs. Same recovery, same evidence shape (DWARF fields + the X360
// AddGuiEvent (id,size) literals + the handler's own loads).

// DWARF :6170 (PS3 GuiEvent<425>; X360 id 430, record 8 bytes -- AddGuiEvent @0x823D5B90).
// HandleComboPerformed @0x8251BE40 branches on the BYTE at +0x04 to pick "StuntCmbBst" vs
// "StuntCmb" and appends the word at +0x00 as an INT param of display string 1.
struct GuiHUDMessageComboPerformed
{
    s32  miScore;       // +0x00 (DWARF :5315)
    bool mbBestScore;   // +0x04 (DWARF :5316)

    s32 GetEventType() const { return 430; }
};
static_assert(sizeof(GuiHUDMessageComboPerformed) == 8, "X360 AddGuiEvent size 8 (id 430)");

// DWARF :6199 (PS3 GuiEvent<396>; X360 id 401, record 4 bytes -- AddGuiEvent @0x823D9738).
// HandleCrushComboMessage @0x8251BFA8 fires "ShowCombo" with the count as an INT param.
struct GuiHUDMessageCrushCombo
{
    s32 miCrushComboCount;   // +0x00 (DWARF :5372)

    s32 GetEventType() const { return 401; }
};
static_assert(sizeof(GuiHUDMessageCrushCombo) == 4, "X360 AddGuiEvent size 4 (id 401)");

// DWARF :5343/:5344 (PS3 GuiEvent<394>; X360 id 399, record 8 bytes -- AddGuiEvent
// @0x823D97F0). HandleShowtimeMultiplierMessage @0x8251BEC0 compares the NEW multiplier
// (+0x00) against the analyzer's miLastSignMultiplier latch and, when it changed, fires
// "ShowSmash" with the EARNED value (+0x04) as an INT param before re-latching.
struct GuiHUDMessageShowtimeMultiplier
{
    s32 miNewMultiplier;      // +0x00 (DWARF :5343)
    s32 miMultiplierEarned;   // +0x04 (DWARF :5344)

    s32 GetEventType() const { return 399; }
};
static_assert(sizeof(GuiHUDMessageShowtimeMultiplier) == 8, "X360 AddGuiEvent size 8 (id 399)");

// [gateui] DWARF :6126/:6136 (PS3 GuiEvent<443/444>; X360 ids 448/449, records 8 bytes --
// AddGuiEvent @0x823D4D30 / @0x823D4DE8). The online road-rage "leader is N miles/km from
// the finish" chatter. Both handlers (@0x8251C270 / @0x8251C318) resolve the car index to
// an online name, then append the metre distance converted in the handler itself
// (x0.0006213712 for miles, x0.001 for km) -- the record always carries METRES.
struct GuiLeaderPassedMileBoundaryEvent
{
    EActiveRaceCarIndex meActiveRaceCarIndex;        // +0x00 (DWARF :5251)
    f32                 mfDistanceToFinishInMetres;  // +0x04 (DWARF :5252)

    s32 GetEventType() const { return 448; }
};
static_assert(sizeof(GuiLeaderPassedMileBoundaryEvent) == 8, "X360 AddGuiEvent size 8 (id 448)");

struct GuiLeaderPassedKMBoundaryEvent
{
    EActiveRaceCarIndex meActiveRaceCarIndex;        // +0x00 (DWARF :5263)
    f32                 mfDistanceToFinishInMetres;  // +0x04 (DWARF :5264)

    s32 GetEventType() const { return 449; }
};
static_assert(sizeof(GuiLeaderPassedKMBoundaryEvent) == 8, "X360 AddGuiEvent size 8 (id 449)");

// DWARF :4795 (PS3 GuiEvent<314>; X360 id 316, record 8 bytes). The special-event car
// the burning route could not start with.
struct GuiEventFailedToStartEvent
{
    CgsID mSpecialCarID;   // +0x00 (DWARF :2539)

    s32 GetEventType() const { return 316; }
};
static_assert(sizeof(GuiEventFailedToStartEvent) == 8, "X360 AddGuiEvent size 8 (id 316)");

// DWARF :4846/:4853 (PS3 GuiEvent<274/275>; X360 ids 276/277, records 4/24 bytes).
// RoadRulesRecvData::NetworkPlayerID is a typedef of s32 (-1 == no player).
struct GuiEventNetworkPlayerJoinedLobby
{
    s32 mNetworkPlayerID;   // +0x00 (DWARF :2959)

    s32 GetEventType() const { return 276; }
};
static_assert(sizeof(GuiEventNetworkPlayerJoinedLobby) == 4, "X360 AddGuiEvent size 4 (id 276)");

struct GuiEventNetworkPlayerLeftLobby
{
    s32                    mNetworkPlayerID;        // +0x00 (DWARF :2973)
    CgsNetwork::PlayerName mPlayerName;             // +0x04 (DWARF :2974; 16-byte name)
    bool                   mbIsLocalPlayerInGame;   // +0x14 (DWARF :2975)

    s32 GetEventType() const { return 277; }
};
static_assert(sizeof(GuiEventNetworkPlayerLeftLobby) == 24, "X360 AddGuiEvent size 24 (id 277)");

// DWARF :5422 (PS3 GuiEvent<265>; X360 id 267, record 8 bytes). DWARF declares the two
// fields private behind get/setters; the accessor bodies are their own ledger rows
// (declaration-only) and the analyzer reads via GetNetworkPlayerID.
struct GuiNetworkRemotePlayerDisconnectEvent
{
public:
    void                SetActiveRaceCarIndex(EActiveRaceCarIndex leIndex);   // DWARF :5426
    EActiveRaceCarIndex GetActiveRaceCarIndex() const                          // DWARF :5429 (X360 inlines the read)
    { return meActiveRaceCarIndex; }
    void SetNetworkPlayerID(s32 liPlayerID);                                   // DWARF :5433
    s32  GetNetworkPlayerID() const                                            // DWARF :5436 (X360 inlines the read @+0x04)
    { return mPlayerID; }

    s32 GetEventType() const { return 267; }

private:
    EActiveRaceCarIndex meActiveRaceCarIndex;   // +0x00 (DWARF :5439)
    s32                 mPlayerID;              // +0x04 (DWARF :5440)
};
static_assert(sizeof(GuiNetworkRemotePlayerDisconnectEvent) == 8, "X360 AddGuiEvent size 8 (id 267)");

// DWARF :6066/:6079 (PS3 GuiEvent<434/435>; X360 ids 439/440, records 24/16 bytes).
struct GuiRivalryStatusChange
{
    // DWARF :3620.
    enum ERivalryLevels
    {
        E_RIVAL_LEVEL_NEW     = 0,
        E_RIVAL_LEVEL_RIVAL   = 1,
        E_RIVAL_LEVEL_TARGET  = 2,
        E_RIVAL_LEVEL_WRECKED = 3,
        E_RIVAL_LEVEL_COUNT   = 4,
    };

    CgsID          mRivalID;      // +0x00 (DWARF :5125)
    CgsID          mCarID;        // +0x08 (DWARF :5126; HandleRivalryChangeEvent prints it)
    ERivalryLevels meNewStatus;   // +0x10 (DWARF :5127; the handler's switch operand)

    s32 GetEventType() const { return 439; }
};
static_assert(sizeof(GuiRivalryStatusChange) == 24, "X360 AddGuiEvent size 24 (id 439)");

struct GuiRivalIsFleeing
{
    CgsID mRivalID;   // +0x00 (DWARF :5138)
    CgsID mCarID;     // +0x08 (DWARF :5139)

    s32 GetEventType() const { return 440; }
};
static_assert(sizeof(GuiRivalIsFleeing) == 16, "X360 AddGuiEvent size 16 (id 440)");

// DWARF :6146 (PS3 GuiEvent<448>; X360 id 453, record 4 bytes).
struct GuiTraitorousTakedownEvent
{
    EActiveRaceCarIndex meAggActiveRaceCarIndex;   // +0x00 (DWARF :5275)

    s32 GetEventType() const { return 453; }
};
static_assert(sizeof(GuiTraitorousTakedownEvent) == 4, "X360 AddGuiEvent size 4 (id 453)");

// DWARF :6163 (PS3 GuiEvent<424>; X360 id 429, record 24 bytes). The stunt snapshot the
// scoring system publishes; the analyzer keys skill messages off the flag masks and the
// spin/roll counters. StuntInfo is the committed 20-byte scoring record
// (BrnStuntModeScoring.h); the X360 record carries TWO MORE half-words after it.
// FLAG: the two tail half-words are X360-only (no DWARF names) -- consumer-inferred:
// HandleStuntPerformed @0x8251B608 bound-checks them <= 1 / sums +0x14 with the
// multiplier exactly like the flat-spin/barrel-roll counters, so they are modelled as
// the "super" counter pair pending a better-attested name.
struct GuiHUDMessageStuntPerformed
{
    BrnGameState::StuntInfo mStuntInfo;        // +0x00..+0x13 (DWARF :5301)
    u16                     mu16TailCount14;   // +0x14 (FLAG: X360-only, name unrecovered)
    u16                     mu16TailCount16;   // +0x16 (FLAG: X360-only, name unrecovered)

    s32 GetEventType() const { return 429; }
};
static_assert(sizeof(GuiHUDMessageStuntPerformed) == 24, "X360 AddGuiEvent size 24 (id 429)");

// DWARF :6208 (PS3 GuiEvent<147>; X360 id 149 = Update dispatch 64+85; no producer
// AddGuiEvent literal). Field names verbatim from the X360 assert strings
// ("strlen(lpEvent->mTitle) < sizeof(lpEvent->mTitle)", cpp:2252/2253).
struct GuiGenericHUDMessage
{
    char mTitle[64];      // +0x00 (DWARF :5386)
    char mSubtitle[64];   // +0x40 (DWARF :5387)

    s32 GetEventType() const { return 149; }
};

// DWARF :6228 (PS3 GuiEvent<440>; X360 id 445, record 4 bytes).
struct GuiOnlineTeamChangeEvent
{
    EActiveRaceCarIndex meActiveRaceCarIndex;   // +0x00 (DWARF :5413)

    s32 GetEventType() const { return 445; }
};
static_assert(sizeof(GuiOnlineTeamChangeEvent) == 4, "X360 AddGuiEvent size 4 (id 445)");

// (GuiEventPlayerWrecked -- DWARF :3856, payload-less -- KEEPS its
// BrnGuiDemangledEventTypes.h placeholder, id 548 size 1: HandleWreckedEvent reads no
// payload field, so the opaque 1-byte record is already the faithful shape.)

// DWARF :2017 (PS3 GuiEvent<392>; X360 id 397, record 16 bytes). HandleShowtimeModeSwitch
// @0x8251F240 resolves mNetworkPlayerID to a name, keys off meActiveRaceCarIndex vs the
// local player and fires only when mbEnteringShowtime is set.
struct GuiShowtimeModeSwitch
{
    s32                 mNetworkPlayerID;       // +0x00 (DWARF :6393)
    EActiveRaceCarIndex meActiveRaceCarIndex;   // +0x04 (DWARF :6394)
    s32                 miFinalShowtimeScore;   // +0x08 (DWARF :6395)
    bool                mbEnteringShowtime;     // +0x0C (DWARF :6396)

    s32 GetEventType() const { return 397; }
};
static_assert(sizeof(GuiShowtimeModeSwitch) == 16, "X360 AddGuiEvent size 16 (id 397)");

// DWARF :6659 (PS3 GuiEvent<563>; X360 id 578, record 24 bytes). HandleChallengeEnded
// @0x824F33E0 switches on meChallengeStatus (1/2/3/6 park a copy in mChallengedEndedData;
// 4 is ignored; anything else asserts "Unknown Challenge Status") and copies the whole
// 24-byte record.
struct GuiChallengeEndEvent
{
    CgsID                         mChallengeID;                  // +0x00 (DWARF :6609)
    BrnGameState::EChallengeStatus meChallengeStatus;            // +0x08 (DWARF :6610)
    s32                           miNumChallengesComplete;       // +0x0C (DWARF :6611)
    s32                           miTotalNumChallenges;          // +0x10 (DWARF :6612)
    bool                          mbAbortingToStartNewChallenge; // +0x14 (DWARF :6613)

    s32 GetEventType() const { return 578; }
};
static_assert(sizeof(GuiChallengeEndEvent) == 24, "X360 AddGuiEvent size 24 (id 578)");

// DWARF :4198 (PS3 GuiEvent<333>; X360 id 337, record 48 bytes). X360 FIELD ORDER pinned
// by HandleRoadRuleFailed @0x8251F020: the road id qword LEADS the record (SPrintf'd
// "%llu" from +0x00 -- same X360 mRoadId-first drift as the committed GuiEventRoadRuleEnter),
// the failed rule type is the word @+0x08 (0 == time rule, 1 == showtime/crash rule,
// anything else asserts), the current ruler's name string sits @+0x0C and the two
// ruled-by flags @+0x2C/+0x2D. Member names verbatim from the DWARF.
struct GuiEventRoadRuleFail
{
    CgsID                    mRoadID;                    // +0x00 (X360-pinned head)
    BrnStreetData::ScoreType meRuleType;                 // +0x08
    char                     macCurrentRulerName[32];    // +0x0C
    bool                     mbRoadRuledByLocalPlayer;   // +0x2C
    bool                     mbRoadRuledByAI;            // +0x2D

    s32 GetEventType() const { return 337; }
};
static_assert(sizeof(GuiEventRoadRuleFail) == 48, "X360 OutputGuiEvent size 48 (id 337)");

// DWARF :4252 (PS3 GuiEvent<338>; X360 id 342, record 48 bytes).
//
// [gateui r3] X360 FIELD ORDER, REORDERED 2026-08-20 -- the DWARF member order (16-byte
// PlayerName FIRST) is PS3-only drift; the X360 record leads with the road id. Two
// independent store-for-store witnesses, both re-derived from the asm:
//   READER   HudMessageAnalyzer::TriggerNewRoadRulesHighScoreMessage @0x8251ED08, with
//            mRoadRuleHighScoreData at analyzer+0x480: `ld r6, 0x480` feeds
//            SPrintf("%llu") from record+0x00; `lwz 0x488` meScoreType; `lwz 0x48C`
//            miNumScoresLost; `lwz 0x490` miNumRoadsNowRuled; `addi r30, r31, 0x494`
//            passes record+0x14 as the STRING (char*) param; `lbz 0x4A4/5/6/7/8` the
//            five bools.
//   PRODUCER TranslateGameActionsToGuiEvents case 281 @0x823EC7C4: `ld r11, 0(r31)` +
//            `std` for the leading qword id, `memcpy(dst+0x14, src+0x14, 0x10)` for the
//            name, the three words at +0x08/+0x0C/+0x10, then `lbz 0x24(r31)` onward.
// sizeof is 48 in BOTH orders, which is exactly why the static_assert below could never
// have caught it. Same mRoadId-first drift the siblings GuiEventRoadRuleFail (:337) and
// GuiEventRoadRuleEnter already carry. This closes this struct's old "the interior X360
// offsets are unverified" FLAG.
// NOTE (not a live corruption before the fix): the only producer/consumer pair in the tree
// is BrnGuiHudMessageAnalyzer_wB_07.cpp's typed struct copy `mRoadRuleHighScoreData =
// *lpEvent;`, so host code was self-consistent whatever the order. Re-verified this round:
// nothing reads this record at raw X360 offsets.
struct GuiEventRoadRuleNewHighScore
{
    CgsID                    mRoadId;                    // +0x00  DWARF :1276
    BrnStreetData::ScoreType meScoreType;                // +0x08  DWARF :1277
    s32                      miNumScoresLost;            // +0x0C  DWARF :1278
    s32                      miNumRoadsNowRuled;         // +0x10  DWARF :1279
    CgsNetwork::PlayerName   mPlayerName;                // +0x14  DWARF :1275 (16-byte name)
    bool                     mbIsLocalPlayer;            // +0x24  DWARF :1280
    bool                     mbIsWholeRoadOwned;         // +0x25  DWARF :1281
    bool                     mbWasRulePlayersBefore;     // +0x26  DWARF :1282
    bool                     mbMultipleScores;           // +0x27  DWARF :1283
    bool                     mbOnlineLossButOfflineWin;  // +0x28  DWARF :1284

    s32 GetEventType() const { return 342; }
};
static_assert(sizeof(GuiEventRoadRuleNewHighScore) == 48, "X360 AddGuiEvent size 48 (id 342)");
static_assert(__builtin_offsetof(GuiEventRoadRuleNewHighScore, mRoadId)            == 0x00, "X360 mRoadId @0x00");
static_assert(__builtin_offsetof(GuiEventRoadRuleNewHighScore, meScoreType)        == 0x08, "X360 meScoreType @0x08");
static_assert(__builtin_offsetof(GuiEventRoadRuleNewHighScore, miNumScoresLost)    == 0x0C, "X360 miNumScoresLost @0x0C");
static_assert(__builtin_offsetof(GuiEventRoadRuleNewHighScore, miNumRoadsNowRuled) == 0x10, "X360 miNumRoadsNowRuled @0x10");
static_assert(__builtin_offsetof(GuiEventRoadRuleNewHighScore, mPlayerName)        == 0x14, "X360 mPlayerName @0x14");
static_assert(__builtin_offsetof(GuiEventRoadRuleNewHighScore, mbIsLocalPlayer)    == 0x24, "X360 mbIsLocalPlayer @0x24");
static_assert(__builtin_offsetof(GuiEventRoadRuleNewHighScore, mbOnlineLossButOfflineWin) == 0x28,
              "X360 mbOnlineLossButOfflineWin @0x28");

// ---------------------------------------------------------------------------------------
// GuiEventProgressionProfileData (id 350, X360 record 12 bytes)
//
// The live-progression handoff from the GAME STATE module to the GUI. Recovered end to
// end from BURNOUT_X360_ARTIST.XEX this wave:
//
//   producer   BrnGameState::GameStateModule::PreWorldUpdate @0x823A5EA4 --
//              AddEvent(&{ &this->mProfile (this+48288),
//                          ResourcePtr<ProgressionData> ? its resource : NULL,
//                          byte }, /*game action*/ 193, /*size*/ 12)
//              on the module output buffer's GameActionQueue (VariableEventQueue<13312,16>).
//   transport  BrnGame::BrnGameModule::BridgeGameStateToGui @0x823EE880 ->
//              TranslateGameActionsToGuiEvents @0x823E9CE0, jump-table case 193
//              @0x823EBBA4: `lwz 0(payload)` / `lwz 4(payload)` / `lbz 8(payload)` into a
//              12-byte stack record, then AddGuiEvent<GuiEventProgressionProfileData>
//              @0x823EBBC8 (the ONLY producer of event 350 in the image).
//   consumers  CgsGui::GuiModule::Update case 209 (== id 350; the switch is rebased by
//              141) @0x82528AB4 -> ProfileManager::SetProgressionProfile(payload[0],
//              payload[1]); plus the registered flow states (BrnGui::InGame,
//              BrnGui::Intro, BrnGui::FBurnMainHudState, BrnGui::HudMessageAnalyzer),
//              all of which read the Profile pointer at offset 0.
//
// The X360 record is {u32,u32,u8} padded to 12; the pointers widen to 8 bytes here
// (semantic parity by named members). The trailing flag is the OR the producer computes
// from Profile::muMedalCountFromTheStart >= 4 and two game-state-module words -- the same
// medal-count test ProgressionManager::AreRoadRulesAvailable uses -- so it is named for
// that meaning; no reconstructed consumer reads it yet.
//
// (This type used to sit in BrnGuiDemangledEventTypes.h as an opaque
// `GuiEvent<350>`-derived shell, i.e. the auto-derived table read the attested record
// size 12 as "header only, no payload". That was wrong: the 12 bytes ARE the payload.)
struct GuiEventProgressionProfileData
{
    BrnProgression::Profile*               mpProfile;              // +0x00
    const BrnProgression::ProgressionData* mpProgressionData;      // +0x04 (X360) / +0x08 (x64)
    bool                                   mbRoadRulesAvailable;   // +0x08 (X360)

    GuiEventProgressionProfileData()
        : mpProfile(0)
        , mpProgressionData(0)
        , mbRoadRulesAvailable(false)
    {
    }

    s32 GetEventType() const { return 350; }
};

// =========================================================================================
// [gateui r3] TEN MORE HudMessageAnalyzer-consumed payloads, upgraded here from their
// opaque BrnGuiDemangledEventTypes.h placeholders (`u8 maData[8]` / `u8 maPayload[N]` /
// an empty GuiEvent<N> shell). Those placeholders are DELETED in the same change -- there
// is exactly one definition of each name in the program.
//
// MODELLING: plain payload structs, NO CgsGui::GuiEvent<N> base -- the same treatment the
// round-1 upgrades got, and for the same reason: the X360 GuiEvent<N> base is provably
// EMPTY while the host's is 12 bytes, so only a plain struct makes sizeof(T) equal the
// console's baked AddEvent literal. Every consumer reads its fields at record+0, which a
// 12-byte base would displace. Each therefore carries its own
// `s32 GetEventType() const { return <X360 id>; }` -- the member CgsGui::GuiModule::
// AddGuiEvent<T> bakes (CgsGuiModule.h:69) -- plus the house sizeof pin.
//
// LAYOUTS: DecFIGS DWARF (references/DecFIGS/dwarfdump/GameSource/Gui/BrnGuiEventTypeDefs.h
// at the cited lines), each confirmed member-for-member against the X360 consumer's loads.
// SIZES: the `AddEvent(queue, payload, <id>, <size>)` literal pair inside that type's
// AddGuiEvent<T> instantiation. Where NO instantiation exists in the image the size is not
// pinned by a literal and the record is pinned by offsetof instead, never by a guess.
// IDS: X360 wire ids. In this range the X360 id is the PS3-DWARF GuiEvent<N> + 5.
// =========================================================================================

// DWARF :5467 (GuiEvent<399>; X360 id 404). sizeof 8 == the AddEvent literal @0x823D8488.
// HandlePowerParkingResult @0x8251F330 tests meOutcome == E_PPO_SUCCESS(1) / != E_PPO_FAILURE(2)
// and bands miOverallRating by a literal 20.
struct GuiPowerParkResult
{
    BrnWorld::EPowerParkOutcome meOutcome;        // +0x00  (lwz 0(a2))
    s32                         miOverallRating;  // +0x04  (lwz 4(a2))

    s32 GetEventType() const { return 404; }
};
static_assert(sizeof(GuiPowerParkResult) == 8, "X360 AddGuiEvent size 8 (id 404)");

// DWARF :5608 (GuiEvent<414>; X360 id 419). NO AddGuiEvent<T> instantiation exists anywhere
// in the image, so the record SIZE is not pinned by a console literal -- the two consumer
// loads in HandleEventDistanceToFinish @0x8251E550 (`lfs 0(a2)` / `lwz 4(a2)`) are the whole
// attestation and the record is pinned by offsetof below.
struct GuiInEventDistanceToFinish
{
    f32 mfDistanceToFinish;   // +0x00  metres
    s32 miPlayerPosition;     // +0x04  1-based

    s32 GetEventType() const { return 419; }
};
static_assert(__builtin_offsetof(GuiInEventDistanceToFinish, mfDistanceToFinish) == 0x00,
              "X360 mfDistanceToFinish @0x00 (lfs 0(a2) @0x8251E5B0)");
static_assert(__builtin_offsetof(GuiInEventDistanceToFinish, miPlayerPosition) == 0x04,
              "X360 miPlayerPosition @0x04 (lwz 4(a2) @0x8251E5C8)");

// DWARF :5618 (GuiEvent<415>; X360 id 420). sizeof 24 == the AddEvent literal @0x823D55D0
// (8 + 4 + 4 + 1 rounded to CgsID's 8-byte alignment).
struct GuiInEventLeaderSplit
{
    CgsID               mLeadersCarID;               // +0x00
    f32                 mfLeadTime;                  // +0x08  (lfs 8(a2) @0x8251E6E0)
    EActiveRaceCarIndex meLeaderActiveRaceCarIndex;  // +0x0C  (lwz 0xC(a2))
    bool                mbLocalPlayerIsLeading;      // +0x10  (lbz 0x10(a2))

    s32 GetEventType() const { return 420; }
};
static_assert(sizeof(GuiInEventLeaderSplit) == 24, "X360 AddGuiEvent size 24 (id 420)");
static_assert(__builtin_offsetof(GuiInEventLeaderSplit, mfLeadTime) == 0x08, "X360 mfLeadTime @0x08");
static_assert(__builtin_offsetof(GuiInEventLeaderSplit, meLeaderActiveRaceCarIndex) == 0x0C,
              "X360 meLeaderActiveRaceCarIndex @0x0C");
static_assert(__builtin_offsetof(GuiInEventLeaderSplit, mbLocalPlayerIsLeading) == 0x10,
              "X360 mbLocalPlayerIsLeading @0x10");

// DWARF :5690 (GuiEvent<420>; X360 id 425). sizeof 12 == the AddEvent literal @0x823D4B08.
// HandleRaceCheckpointReached @0x8251B350 compares the GLOBAL index (not the active one).
struct GuiRaceCheckpointReached
{
    EActiveRaceCarIndex meActiveRaceCarIndex;   // +0x00
    EGlobalRaceCarIndex meGlobalRaceCarIndex;   // +0x04
    s32                 miCheckpointIndex;      // +0x08  0-based

    s32 GetEventType() const { return 425; }
};
static_assert(sizeof(GuiRaceCheckpointReached) == 12, "X360 AddGuiEvent size 12 (id 425)");

// DWARF :6153 (GuiEvent<449>; X360 id 454). sizeof 8 == the AddEvent literal @0x823D5968.
struct GuiBHRCheckpointReachedEvent
{
    EActiveRaceCarIndex meActiveRaceCarIndex;   // +0x00
    s32                 miNumCheckpointsToGo;   // +0x04

    s32 GetEventType() const { return 454; }
};
static_assert(sizeof(GuiBHRCheckpointReachedEvent) == 8, "X360 AddGuiEvent size 8 (id 454)");

// DWARF :6218 (GuiEvent<450>; X360 id 455). sizeof 8 == the AddEvent literal @0x823D5A20.
struct GuiHUDMessageBHRRunnerCrashed
{
    EActiveRaceCarIndex meActiveRaceCarIndex;   // +0x00
    s32                 miNumCrashesToGo;       // +0x04

    s32 GetEventType() const { return 455; }
};
static_assert(sizeof(GuiHUDMessageBHRRunnerCrashed) == 8, "X360 AddGuiEvent size 8 (id 455)");

// DWARF :5097 (GuiEvent<361>; X360 id 366). sizeof 8 == the AddEvent literal @0x823D3070.
// HandleDriveThrough @0x8251D570 asserts meDriveThroughType < E_DRIVE_THROUGH_TYPE_COUNT and
// selects one of three message tables on mbEffective (lbz 4(a2)).
struct GuiDriveThroughEvent
{
    // DWARF :3665 -- the nested enumerator set, verbatim.
    enum DriveThroughType
    {
        E_DRIVE_THROUGH_TYPE_CAR_WASH    = 0,
        E_DRIVE_THROUGH_TYPE_BODY_SHOP   = 1,
        E_DRIVE_THROUGH_TYPE_PAINT_SHOP  = 2,
        E_DRIVE_THROUGH_TYPE_GAS_STATION = 3,
        E_DRIVE_THROUGH_TYPE_AUTO_PARTS  = 4,
        E_DRIVE_THROUGH_TYPE_FAILED      = 5,
        E_DRIVE_THROUGH_TYPE_COUNT       = 6,
    };

    DriveThroughType meDriveThroughType;   // +0x00  DWARF :3680
    bool             mbEffective;          // +0x04  DWARF :3681

    s32 GetEventType() const { return 366; }
};
static_assert(sizeof(GuiDriveThroughEvent) == 8, "X360 AddGuiEvent size 8 (id 366)");

// DWARF :5651 (GuiEvent<418>; X360 id 423). sizeof 8 == the AddEvent literal @0x823D5740.
// HandleEventFinisher @0x824F2FB0 fires only for miFinishPosition == 1.
struct GuiInEventFinisher
{
    EActiveRaceCarIndex meActiveRaceCarIndex;   // +0x00  DWARF :4803
    s32                 miFinishPosition;       // +0x04  DWARF :4804  (1-based)

    s32 GetEventType() const { return 423; }
};
static_assert(sizeof(GuiInEventFinisher) == 8, "X360 AddGuiEvent size 8 (id 423)");

// DWARF :5315 (GuiEvent<478>; X360 id 483 -- the case the committed fan-out dispatches,
// BrnGuiHudMessageAnalyzer_wB_12.cpp :: Update). This type had NO definition anywhere in the
// tree, only the forward declaration at BrnGuiHudMessageAnalyzer.h:74. Like
// GuiInEventDistanceToFinish it has NO AddGuiEvent<T> instantiation in the image, so the
// size is not pinned by a literal; the two consumer loads in HandleNetworkBattling
// @0x8251CDC0 (`lwz 0(a2)` / `lwz 4(a2)`, each range-asserted against [0, 8)) are the
// attestation and the record is pinned by offsetof.
struct GuiNetworkPlayerBattlingEvent
{
    EActiveRaceCarIndex meAggressorActiveRaceCarIndex;   // +0x00  DWARF :4340
    EActiveRaceCarIndex meVictimActiveRaceCarIndex;      // +0x04  DWARF :4341

    s32 GetEventType() const { return 483; }
};
static_assert(__builtin_offsetof(GuiNetworkPlayerBattlingEvent, meAggressorActiveRaceCarIndex) == 0x00,
              "X360 aggressor index @0x00 (lwz 0(a2) @0x8251D004)");
static_assert(__builtin_offsetof(GuiNetworkPlayerBattlingEvent, meVictimActiveRaceCarIndex) == 0x04,
              "X360 victim index @0x04 (lwz 4(a2) @0x8251D034)");

// -----------------------------------------------------------------------------------------
// DWARF :4566 (GuiEvent<204>; X360 id 206). sizeof 28 == the AddEvent literal @0x823DA510.
//
// X360 FIELD ORDER (binary authoritative; the PS3 DWARF lists the nine leading bools first,
// the X360 record leads with the scalars). Pinned STORE FOR STORE by the sole producer,
// BrnGame::BrnGameModule::BridgeWorldVehicleDataToGui @0x823E5768 (@0x823E5AFC..0x823E5B70),
// which copies the world's 36-byte BrnWorld::RaceCarEntityModuleIO::BoostOutputInfo
// (BrnRaceCarEntityModuleOutputInterface.h:72, itself X360-attested) field by field:
//
//   dst+0x00 <- src+0x0C muNumChained        dst+0x10 <- src+0x1C mbBoostIsFull
//   dst+0x04 <- src+0x10 mfBoostAmount       dst+0x11 <- src+0x00 mbIsBoosting
//   dst+0x08 <- src+0x14 mfMaxBoost          dst+0x12 <- src+0x01 mbIsInAir
//   dst+0x0C <- src+0x20 meBoostType         dst+0x13 <- src+0x02 mbIsOncoming
//                                            dst+0x14 <- src+0x03 mbIsDrifting
//                                            dst+0x15 <- src+0x04 mbNearMiss
//                                            dst+0x16 <- src+0x05 mbIsBlueMode
//                                            dst+0x17 <- src+0x06 mbWasChainJustCompleted
//                                            dst+0x18 <- src+0x0A mbAllowedToBoost
//                                            dst+0x19 <- src+0x08 mbIsTailgating
//
// Fourteen stores, fourteen DWARF members -- the mapping is exact, not inferred, and it is
// the SECOND witness the round-2 park asked for (the first being HandleChainedBoost's own
// reads of +0x00 / +0x0C / +0x10 / +0x11 / +0x17). `mbIsBlueMode` is the world-side name for
// the slot the GUI DWARF calls `mbIsChainedMode`; the GUI name is used here.
// The producer post-processes two slots in Showtime/Crash mode (game mode 2 or 16,
// @0x823E5B74): mbBoostIsFull is FORCED true and mbAllowedToBoost becomes
// `!CrashModeScoring::HasCrashModeEnded() && mbAllowedToBoost`.
//
// The old model (`GuiEvent<206>` + `u8 maPayload[16]`) was provably wrong: HandleChainedBoost
// reads a live field at record+0x00, which a 12-byte host header would displace.
// -----------------------------------------------------------------------------------------
struct GuiEventBoostInfo
{
    u32                  muNumChained;             // +0x00  DWARF :2152
    f32                  mfBoostAmount;            // +0x04  DWARF :2153
    f32                  mfMaxBoost;               // +0x08  DWARF :2154
    BrnWorld::EBoostType meBoostType;              // +0x0C  DWARF :2155
    bool                 mbBoostIsFull;            // +0x10  DWARF :2156
    bool                 mbIsBoosting;             // +0x11  DWARF :2143
    bool                 mbIsInAir;                // +0x12  DWARF :2144
    bool                 mbIsOncoming;             // +0x13  DWARF :2145
    bool                 mbIsDrifting;             // +0x14  DWARF :2146
    bool                 mbNearMiss;               // +0x15  DWARF :2147
    bool                 mbIsChainedMode;          // +0x16  DWARF :2148 (world-side mbIsBlueMode)
    bool                 mbWasChainJustCompleted;  // +0x17  DWARF :2149
    bool                 mbAllowedToBoost;         // +0x18  DWARF :2150
    bool                 mbIsTailgating;           // +0x19  DWARF :2151
    // 2 tail-pad bytes -> the X360 28-byte record

    s32 GetEventType() const { return 206; }
};
static_assert(sizeof(GuiEventBoostInfo) == 28, "X360 AddGuiEvent size 28 (id 206)");
static_assert(__builtin_offsetof(GuiEventBoostInfo, muNumChained)            == 0x00, "X360 muNumChained @0x00");
static_assert(__builtin_offsetof(GuiEventBoostInfo, meBoostType)             == 0x0C, "X360 meBoostType @0x0C");
static_assert(__builtin_offsetof(GuiEventBoostInfo, mbBoostIsFull)           == 0x10, "X360 mbBoostIsFull @0x10");
static_assert(__builtin_offsetof(GuiEventBoostInfo, mbIsBoosting)            == 0x11, "X360 mbIsBoosting @0x11");
static_assert(__builtin_offsetof(GuiEventBoostInfo, mbWasChainJustCompleted) == 0x17, "X360 mbWasChainJustCompleted @0x17");
static_assert(__builtin_offsetof(GuiEventBoostInfo, mbIsTailgating)          == 0x19, "X360 mbIsTailgating @0x19");


// =========================================================================================
// [E1 event-status wave 2026-08-26] THE EVENT SCORE / TIMER FEED PAYLOADS.
//
// The three records BrnGameModule::BridgeGameStateToGui @0x823EE880 posts to carry an
// event's clock and score. All three used to be opaque `GuiEvent<N> + u8[]` shells in
// BrnGuiDemangledEventTypes.h, i.e. modelled as "12-byte GuiEvent header + payload"; the
// asm proves every one of them is RAW -- each AddGuiEvent<T> instantiation hands AddEvent
// the object AS PASSED, so the queued bytes open at field +0x00. Their (id,size) pairs are
// unchanged, so the explicit instantiations in CgsGuiModule_AddGuiEvent_Inst.cpp still
// match the console constants.
//
// They live HERE rather than in the demangled catalogue because their consumer,
// GuiCache::RecEvent, cannot include that header: BrnGuiOptionsDataProfile.h (which
// BrnGuiCache.cpp needs for the +0xB878 options block) defines its own compile-only
// `BrnGui::GuiEventAudioTraxUpdate` slice, and the two definitions are a hard C2011. This
// is the same migration the header's own top-of-file rule prescribes and that
// GuiEventBoostInfo / GuiEventChangeDistrict / GuiEventBoostBarStuntInfo already took.
// =========================================================================================
// [E1 event-status wave 2026-08-26] RECOVERED (was the opaque `GuiEvent<492> + u8[108]`
// shell). THE RECORD IS RAW: AddGuiEvent<GuiEventCurrentStatus> @0x823D0DF0 ends
//     li r6, 0x78 ; li r5, 0x1EC ; mr r4, r27 ; bl ...AddEvent
// with r27 == the object as passed, so the 120 queued bytes open at +0x00. Size 120 and
// id 492 are unchanged, so the AddGuiEvent<T> instantiation is unaffected.
//
// Producer BrnGameModule::BridgeGameStateToGui @0x823EEB48..0x823EEC20 builds it at
// sp+0x1B0 (var_2450) -- the SAME stack slot the id-239 GuiEventRaceDistanceRemaining
// record was posted from four instructions earlier, which is why only +0x00..+0x37 are
// rewritten here; +0x38.. is the checkpoint-index OUT buffer
// (`addi r4, r1, var_2418` == payload+0x38) that CarCheckpointData::
// GetAllRemainingCheckpointIndexes @0x823C4D50 fills, and its 16-entry bound is that
// function's own `cmpwi r10, 0x10` guard -- 16*4 == 64 == 0x78-0x38 exactly.
//   +0x00/+0x04  <- mTimerStatusInterface.GetGameTimerStatus()->GetTime()
//                   (lwz/lfs 0(r22)/4(r22), r22 == gm+10095388 == that status +0x10)
//   +0x08        <- GetGameTimerStatus()->GetCurrentTimeStep()  (lfs 8(r21) * lfs 4(r21))
//   +0x0C        <- ScoringOutputInterface::maCarScoreData[player].mfDistanceToFinishLive
//                   (`mulli 0x128` + `lfs 0x18`)
//   +0x10        <- ScoringOutputInterface::mfDistanceDrivenInCurrentCar (lfs 0xAA4)
//   +0x14..+0x33 <- OnlineScoringOutputInterface::maePlayerTeam[8] (the 8-word ctr loop
//                   from r7 == onlineScoring+0x60)
//   +0x34        <- the remaining-checkpoint COUNT (0 on every path but the
//                   E_MODE_ONLINE_BURNING_HOME_RUN runner search)
// Consumer GuiCache::RecEvent @0x82510540..0x82510600 (jpt_825101AC case 112 == id 492)
// reads +0x34 -> cache 0x4F9C, +0x38.. as the landmark-tracker source, +0x10 (lfs) ->
// cache 0x13B94 (mfDistanceDriven) and +0x14..+0x33 -> cache 0xB808 (maCurrentPlayerTeam).
// FLAG: the four leading time/distance words have no recovered consumer yet (RecEvent
// does not read them); they are producer-named.
struct GuiEventCurrentStatus
{
    s32 miGameTimeSeconds;                   // +0x00  <- TimerStatus::GetTime().GetSeconds()
    f32 mfGameTimeFraction;                  // +0x04  <- TimerStatus::GetTime().GetFraction()
    f32 mfGameTimeStep;                      // +0x08  <- TimerStatus::GetCurrentTimeStep()
    f32 mfDistanceToFinishLive;              // +0x0C  <- maCarScoreData[player].mfDistanceToFinishLive
    f32 mfDistanceDrivenInCurrentCar;        // +0x10  -> GuiCache::mfDistanceDriven (+0x13B94)
    s32 maePlayerTeam[8];                    // +0x14  -> GuiCache::maCurrentPlayerTeam (+0xB808)
    s32 miNumRemainingCheckpoints;           // +0x34  -> GuiCache +0x4F9C
    s32 maiRemainingCheckpointIndexes[16];   // +0x38  (GetAllRemainingCheckpointIndexes OUT)

    s32 GetEventType() const { return 492; }
};
static_assert(sizeof(GuiEventCurrentStatus) == 120, "X360 AddGuiEvent size 120 (id 492) @0x823D0DF0");
static_assert(__builtin_offsetof(GuiEventCurrentStatus, mfDistanceDrivenInCurrentCar) == 0x10 &&
              __builtin_offsetof(GuiEventCurrentStatus, maePlayerTeam)                == 0x14 &&
              __builtin_offsetof(GuiEventCurrentStatus, miNumRemainingCheckpoints)    == 0x34 &&
              __builtin_offsetof(GuiEventCurrentStatus, maiRemainingCheckpointIndexes) == 0x38,
              "GuiEventCurrentStatus wire drift (RecEvent case 112 @0x82510540/0x825105A0/0x825105B0)");

// =========================================================================================
// ⭐⭐ [event-starts producer wave 2026-08-27] GuiEventUpdateEventStarts (id 203, size 8416)
// -- RECOVERED, and MOVED HERE out of BrnGuiDemangledEventTypes.h for the usual two reasons:
// its consumer is GuiCache::RecEvent (which cannot include that header -- see the note above
// GuiEventCurrentStatus), and the placeholder that stood there was WRONG, not merely opaque.
//
// ⛔ WHAT THE PLACEHOLDER GOT WRONG. It read `GuiEvent<203> + u8 maPayload[8404]`, i.e. "a
// 12-byte GuiEvent header then 8404 bytes of payload" -- the honest default the demangled
// header applies when nothing is known. The asm says otherwise: THERE IS NO HEADER. The one
// producer, BridgeGameStateToGui @0x823EF1B8..0x823EF1DC, does
//     li r5, 0x20E0 ; addi r4, r15, 0x2B0F0 ; addi r3, r1, var_2180 ; bl memcpy
//     addi r4, r1, var_2180 ; bl AddGuiEvent<GuiEventUpdateEventStarts>
// -- it memcpys the OutputBuffer's SetUpAllEventStartsInterface onto a bare stack local and
// hands that local straight to AddGuiEvent, which queues `sizeof(T)` bytes from offset 0
// (@0x823D1394 `li r6, 0x20E0 ; li r5, 0xCB`). So the queued 8416 bytes ARE the interface,
// starting at its first EventStart's position lane. Under the old shape the first 12 of them
// would have been read as a GuiEvent header and the array would have been 12 bytes out --
// silently, since the size literal matched either way.
// The consumer confirms it from the other end: RecEvent's case-203 arm is
// `memcpy(cache + 22160, payload, 8416)` and cache+0x5690 is the FIRST EventStart of the
// cache's own embedded copy (GuiCache.h's maEventStarts[175] + miEventStartsCount @+0x7760).
//
// So the record IS the interface, spelled by name rather than as an opaque span. That also
// makes the 8416 self-proving: SetUpAllEventStartsInterface::_AssertLayout already pins its
// own sizeof at 0x20E0 against the SAME two console literals.
struct GuiEventUpdateEventStarts
{
    BrnGameState::GameStateModuleIO::SetUpAllEventStartsInterface mEventStarts;

    s32 GetEventType() const { return 203; }
};
static_assert(sizeof(GuiEventUpdateEventStarts) == 0x20E0,
              "X360 AddGuiEvent size 8416 (id 203) @0x823D1394 == the bridge memcpy 0x20E0 @0x823EF1C0");

// [E1 event-status wave 2026-08-26] RECOVERED (was the opaque `GuiEvent<424> + u8[8]`
// shell). THE RECORD IS RAW: AddGuiEvent<GuiEventScoreUpdate> @0x823D0EA8 ends
//     li r6, 0x14 ; li r5, 0x1A8 ; mr r4, r27 ; bl ...AddEvent
// with r27 == the object as passed. Size 20 / id 424 unchanged.
//
// Producer BrnGameModule::BridgeGameStateToGui @0x823EEC54..0x823EED6C, built at
// sp+0x50 (var_25B0):
//   +0x00 <- ScoringOutputInterface::meCurrentMedalTarget      (lwz 0xA9C -> stw +0x00)
//   +0x04 <- ::mfModeTimeElapsed                               (lfs 0xA90 -> stfs +0x04)
//            ...OVERWRITTEN in the mode-{3,7,12,14,17} arm (jpt_823EEC98 cases
//            0,4,9,11,14 of `mode - 3`) by ::mfModeTimeRemaining CLAMPED AT ZERO --
//            `lfs f0, 0xA94 ; fsel f0, f0, f0, f31` with f31 == flt_82001CC0, and
//            image.bin @0x82001CC0 == 00 00 00 00 == 0.0f, i.e. (x >= 0) ? x : 0.
//   +0x08 <- ::mfCurrentTargetModeTime                         (lfs 0xA98 -> stfs +0x08)
//   +0x0C <- maCarScoreData[player].mfDistanceToNextCheckpointLive (`mulli 0x128`+`lfs 0x20`)
//   +0x10 <- ::mbTimerActive                                   (lbz 0xAA8 -> stb +0x10)
// Consumer GuiCache::RecEvent @0x825107F4..0x82510878 (jpt_825101AC case 44 == id 424):
//   cache 0x9F28 <- +0x00; and ONLY IF +0x10 != 0, cache 0x9F2C (mfEventTime) <- +0x04
//   and cache 0x9F30 (mfTargetTime) <- +0x08; then +0x0C drives mfDistanceInEvent
//   (0x9F48) unless it equals the sentinel flt_82F27EFC (image.bin: 7F 7F FF FF == FLT_MAX),
//   in which case the cached distance is only raised to 0.0f when it had gone negative.
struct GuiEventScoreUpdate
{
    s32  meCurrentMedalTarget;            // +0x00 <- ScoringOutputInterface::meCurrentMedalTarget
    f32  mfModeTime;                      // +0x04 <- ::mfModeTimeElapsed, or max(::mfModeTimeRemaining, 0)
    f32  mfCurrentTargetModeTime;         // +0x08 <- ::mfCurrentTargetModeTime
    f32  mfDistanceToNextCheckpoint;      // +0x0C <- maCarScoreData[player].mfDistanceToNextCheckpointLive
    bool mbTimerActive;                   // +0x10 <- ::mbTimerActive (gates the two time words)
    u8   maPad11[3];                      // +0x11..+0x13 (the console posts 20 bytes)

    s32 GetEventType() const { return 424; }
};
static_assert(sizeof(GuiEventScoreUpdate) == 20, "X360 AddGuiEvent size 20 (id 424) @0x823D0EA8");
static_assert(__builtin_offsetof(GuiEventScoreUpdate, mfModeTime)               == 0x04 &&
              __builtin_offsetof(GuiEventScoreUpdate, mfDistanceToNextCheckpoint) == 0x0C &&
              __builtin_offsetof(GuiEventScoreUpdate, mbTimerActive)            == 0x10,
              "GuiEventScoreUpdate wire drift (RecEvent case 44 @0x82510814/0x82510834/0x82510804)");

// [E1 event-status wave 2026-08-26] RECOVERED (was the opaque `GuiEvent<428> + u8[28]`
// shell, which mis-modelled the wire as a 12-byte GuiEvent header plus 28 payload bytes).
// THE RECORD IS RAW: AddGuiEvent<GuiAttackScoreUpdate> @0x823D1188 ends
//     li r6, 0x28 ; li r5, 0x1AC ; mr r4, r27 ; bl VariableEventQueue<32768,16>::AddEvent
// with r27 == the object AS PASSED (no +12), so the 40 queued bytes OPEN with the first
// score word. Size 40 and id 428 are unchanged, so the instantiation is unaffected.
//
// Pinned at BOTH ends, field for field:
//   producer  BrnGameModule::BridgeGameStateToGui @0x823EEE4C..0x823EEEA8 (the
//             mode-{7,9,12,14,17} arm of jpt_823EED94) copies ScoringOutputInterface
//             +0xA64/+0xA68/+0xA6C/+0xA70/+0xA74/+0xA78 -> payload +0x00..+0x14,
//             +0xA84 (lfs) -> +0x18, +0xA7C (ld, the merged maStunts[0] pair) -> +0x1C/+0x20,
//             +0xA88/+0xA89 (lbz) -> +0x24/+0x25.
//   consumer  GuiCache::RecEvent @0x82510960..0x82510A08 (jpt_825101AC case 48 == id 428)
//             reads +0x00/+0x04/+0x08/+0x0C -> cache 0x9FC4/0x9FC8/0x9FCC/0x9FD0,
//             +0x10/+0x14 -> 0xAC64/0xAC68, +0x18 (lfs) -> 0xAC6C,
//             +0x1C/+0x20 -> 0xAC5C/0xAC60, +0x24/+0x25 (lbz) -> 0xAC70/0xAC71.
// The NAMES are the producer's own member names (BrnGameStateSharedIO.h
// ScoringOutputInterface :559-:568) -- every payload word is a straight copy of the
// like-named scoring-output member, and the consumer stores each into the cache slot
// BrnGuiCache.h already carries under that name.
struct GuiAttackScoreUpdate
{
    s32  miCurrentScore;             // +0x00  <- ScoringOutputInterface::miCurrentScore
    s32  miTargetScore;              // +0x04  <- ::miTargetScore
    s32  miComboScore;               // +0x08  <- ::miComboScore
    s32  miComboMultiplier;          // +0x0C  <- ::miComboMultiplier
    u32  muCurrentStunts;            // +0x10  <- ::muCurrentStunts
    u32  muAllStunts;                // +0x14  <- ::muAllStunts
    f32  mfComboWarningTimeActive;   // +0x18  <- ::mfComboWarningTimeActive
    s32  meStuntToDisplayType;       // +0x1C  <- ::maStunts[0].meStuntType
    s32  miStuntToDisplayScore;      // +0x20  <- ::maStunts[0].miStuntScore
    bool mbComboWarningActive;       // +0x24  <- ::mbComboWarningActive
    bool mbComboInProgress;          // +0x25  <- ::mbComboInProgress
    u8   maPad26[2];                 // +0x26..+0x27 (the console posts 40 bytes)

    s32 GetEventType() const { return 428; }
};
static_assert(sizeof(GuiAttackScoreUpdate) == 40, "X360 AddGuiEvent size 40 (id 428) @0x823D1188");
static_assert(__builtin_offsetof(GuiAttackScoreUpdate, mfComboWarningTimeActive) == 0x18 &&
              __builtin_offsetof(GuiAttackScoreUpdate, meStuntToDisplayType)     == 0x1C &&
              __builtin_offsetof(GuiAttackScoreUpdate, mbComboWarningActive)     == 0x24,
              "GuiAttackScoreUpdate wire drift (RecEvent case 48 @0x825109E4/0x825109FC/0x825109EC)");

// =========================================================================================
// ⭐⭐⭐ [showtime score wave 2026-08-29] RECOVERED (was the opaque
// `GuiEvent<434> { u8 maPayload[4]; }` shell in BrnGuiDemangledEventTypes.h, which
// mis-modelled the wire as a 12-byte GuiEvent header plus 4 payload bytes).
// THE RECORD IS RAW: AddGuiEvent<GuiCrashScoreUpdate> @0x823D10D0 ends
//     li r6, 0x10 ; li r5, 0x1B2 ; mr r4, r27 ; bl VariableEventQueue<32768,16>::AddEvent
// with r27 == the object AS PASSED (no +12), so the 16 queued bytes OPEN with miCarsCrashed.
// Size 16 and id 434 are unchanged, so the AddGuiEvent<T> instantiation
// (CgsGuiModule_AddGuiEvent_Inst.cpp:77) is unaffected.
//
// ⛔⛔ THE ID IS 434, NOT THE DWARF'S 429 -- AND THAT MATTERS.
// The DecFIGS DWARF spells this `struct GuiCrashScoreUpdate : public GuiEvent<429>`
// (BrnGuiEventTypeDefs.h:4883). It is a PS3-build id. This whole family is shifted by +5 in
// ARTIST, every one of them read off its own instantiation's `li r5`:
//     GuiEventScoreUpdate      DWARF 419 -> X360 424 (0x1A8) @0x823D0EA8
//     GuiRoadRageScoreUpdate   DWARF 421 -> X360 426 (0x1AA) @0x823D0F60
//     GuiAttackScoreUpdate     DWARF 423 -> X360 428 (0x1AC) @0x823D1188
//     GuiPursuitScoreUpdate    DWARF 427 -> X360 432 (0x1B0) @0x823D1018
//     GuiCrashScoreUpdate      DWARF 429 -> X360 434 (0x1B2) @0x823D10D0
// Posting the DWARF id would be a producer no consumer can hear: RecEvent's third
// sub-switch rebases by 0x17C, and 429-380 == 49 sits inside jpt_825101AC's DEFAULT case
// list (cases 41-53), so that arm discards the record without a diagnostic.
//
// Pinned at BOTH ends, field for field:
//   producer  BrnGameModule::BridgeGameStateToGui @0x823EEE18..0x823EEE44 (the mode-{2,16}
//             arm of jpt_823EED94 -- jump-table cases 0 and 14, the table being indexed by
//             mode-2) copies ScoringOutputInterface +0xA54 -> payload +0x00,
//             +0xA5C -> +0x04, +0xA58 -> +0x08 and +0xA60 (lfs) -> +0x0C.
//   consumer  GuiCache::RecEvent @0x82510AA0..0x82510B28 (jpt_825101AC case 54 == id 434)
//             reads +0x00 -> cache 0xA004, +0x04 -> 0xA008, +0x0C (lfs) -> 0xA00C.
// ⚠️ +0x08 (miScoreMultiplier) IS POSTED AND NEVER CONSUMED. The console writes it into the
// record and RecEvent's arm has no store for it -- GuiCache carries no member for it either.
// That is the binary's behaviour; it is transcribed, not "cleaned up".
//
// The NAMES are the DWARF's own (BrnGuiEventTypeDefs.h:4885-4888) and their ORDER matches the
// producer's offsets exactly, which is the independent corroboration that the +5 shift is an
// ID shift and not a different record.
struct GuiCrashScoreUpdate
{
    s32 miCarsCrashed;         // +0x00  <- ScoringOutputInterface::miShowtimeCarsCrashed
    s32 miComboMultiplier;     // +0x04  <- ::miShowtimeComboMultiplier
    s32 miScoreMultiplier;     // +0x08  <- ::miShowtimeScoreMultiplier   (posted, unconsumed)
    f32 mfDistanceTravelled;   // +0x0C  <- ::mfShowtimeDistanceTravelled

    s32 GetEventType() const { return 434; }
};
static_assert(sizeof(GuiCrashScoreUpdate) == 16, "X360 AddGuiEvent size 16 (id 434) @0x823D10D0");
static_assert(__builtin_offsetof(GuiCrashScoreUpdate, miComboMultiplier)   == 0x04 &&
              __builtin_offsetof(GuiCrashScoreUpdate, miScoreMultiplier)   == 0x08 &&
              __builtin_offsetof(GuiCrashScoreUpdate, mfDistanceTravelled) == 0x0C,
              "GuiCrashScoreUpdate wire drift (RecEvent case 54 @0x82510AF4/0x82510B10/0x82510B18)");

// =========================================================================================
// [A9 mode-type arm 2026-08-27] RECOVERED (was the opaque `GuiEvent<93> + u8[140]` shell in
// BrnGuiDemangledEventTypes.h, which mis-modelled the wire as a 12-byte GuiEvent header plus
// 140 payload bytes). THE RECORD IS RAW: AddGuiEvent<GuiEventPrepareForModeStart> @0x823D27D0
// posts the object AS PASSED, and GuiCache::RecEvent's case-93 arm reads the 8-byte pursued-car
// id with `ld r11, 0(r30)` (@0x8250E968) -- i.e. the 152 queued bytes OPEN with that id.
// Size 152 and id 93 are unchanged, so the AddGuiEvent<T> instantiation
// (CgsGuiModule_AddGuiEvent_Inst.cpp:140) is unaffected.
//
// Pinned at BOTH ends, field for field:
//   producer  BrnGame::TranslateEventFlowGameActionToGuiEvent case 23 -- the console's
//             GameBridgeGameStateToX @0x823EADCC..0x823EAFD0 store map, transcribed in
//             GameSource/Game/GameBridgeGameStateToX_EventFlowGuiEvents.cpp (the TU-local
//             `PrepareForModeStartWire93` this type supersedes -- see the conductor request).
//   consumer  GuiCache::RecEvent @0x8250E7E0..0x8250EAA0 (jumptable 8250DE3C case 89 ==
//             GUI id 93; the console's jump-table index is `id - 4`).
//             +0x0C -> cache 0x9E58 meGameModeType   *** THE EventInfoComponent::Update GATE ***
//             +0x10 -> 0x9E5C muEventID       +0x14 -> 0x9E60 muJunctionID
//             +0x84 -> 0x9F2C mfEventTime AND 0x9F30 mfTargetTime (the SAME word, twice)
//             +0x80/+0x7C/+0x78 -> 0x9F34/0x9F38/0x9F3C mafTargetScores[0..2]
//             +0x88 -> 0x9FEC   +0x8C -> 0x9FB8 (lbz/stb)   +0x8E -> 0x9F44 (lbz/stb)
//             +0x8F -> 0x9FC0 (lbz + EXTSB + stw -- a SIGNED byte widened to a word)
//             +0x90 -> 0x4B4C (lbz/stb)   +0x00 -> 0x9FE0 (ld/std, the 8-byte CgsID)
//             +0x18[i] -> 0x9F54 + 2i (sth)   +0x38[i] -> 0x9F74 + 4i (stw)
// The trailing 6 bytes are the record's tail padding to the attested 152 (the console posts a
// stack frame that width; nothing reads +0x92..+0x97).
struct GuiEventPrepareForModeStart
{
    CgsID mPursuedCarId;                   // +0x00  GameModeParams::mPursuedCarID
    s32   miCurrentRound;                  // +0x08  action->GetCurrentRound()
    s32   meGameModeType;                  // +0x0C  GameModeParams::GetGameModeType()
    u32   muEventJunctionID;               // +0x10  GameModeParams::muEventJunctionID
    u32   muJunctionID;                    // +0x14  GameModeParams::muJunctionID
    u16   mau16CheckpointLandmark[16];     // +0x18  CheckpointData[i]+0x00
    s32   maiCheckpointDistrict[16];       // +0x38  CheckpointData[i]+0x04
    f32   mfNeedForBronze;                 // +0x78  GameModeParams+0x60
    f32   mfNeedForSilver;                 // +0x7C  GameModeParams+0x64
    f32   mfNeedForGold;                   // +0x80  GameModeParams+0x68
    f32   mfModeTimeLimit;                 // +0x84  GameModeParams+0x6C
    s32   miPursuitRivalTotalDamage;       // +0x88  GameModeParams+0x50
    u8    mu8CheckpointCount;              // +0x8C  (u8)GameModeParams::GetCheckpointCount()
    u8    mu8DifficultyLevel;              // +0x8D  GameModeParams::muDifficultyLevel
    u8    mu8CarCount;                     // +0x8E  mbIsOnline ? miNumNetworkPlayers : miNumRivals
    s8    mi8RoadRageThreshold;            // +0x8F  (s8)GameModeParams::miRoadRageThreshold
                                           //        ⚠️ SIGNED: RecEvent widens it with `extsb`.
    u8    mbIsOnline;                      // +0x90  GameModeParams::mbIsOnline
    u8    mbOnlineLobbyTransition;         // +0x91  (name FLAGGED at the producer)
    u8    maPad92[6];                      // +0x92..+0x97 tail padding to the attested 152

    s32 GetEventType() const { return 93; }
};
static_assert(sizeof(GuiEventPrepareForModeStart) == 152,
              "X360 AddGuiEvent size 152 (id 93) @0x823D27D0");
static_assert(__builtin_offsetof(GuiEventPrepareForModeStart, meGameModeType)          == 0x0C &&
              __builtin_offsetof(GuiEventPrepareForModeStart, mau16CheckpointLandmark) == 0x18 &&
              __builtin_offsetof(GuiEventPrepareForModeStart, maiCheckpointDistrict)   == 0x38 &&
              __builtin_offsetof(GuiEventPrepareForModeStart, mfModeTimeLimit)         == 0x84 &&
              __builtin_offsetof(GuiEventPrepareForModeStart, mu8CheckpointCount)      == 0x8C &&
              __builtin_offsetof(GuiEventPrepareForModeStart, mbIsOnline)              == 0x90,
              "GuiEventPrepareForModeStart wire drift (RecEvent case 89 @0x8250E7E4/0x8250E90C/"
              "0x8250E9CC/0x8250E8FC)");

// ==========================================================================================
// GuiEventOfflinePostEvent::OfflinePostEventData -- the 192-byte offline event-result record.
//
// WHERE THE SIZE AND BASE COME FROM (both proven, neither guessed):
//   InstantResultsState::HandleIncomingEvents @0x824DBAD8 case 64 does, verbatim,
//       memcpy(this + 0x2278, *lpEvent + 40552, 192)
//   -- so the record is EXACTLY 192 bytes, and the state's copy sits at +0x2278. The far end
//   is independently confirmed: the very next member the class touches is
//   mpcAnimatingComponentName at +0x2338, and 0x2278 + 192 == 0x2338 exactly. Two independent
//   derivations of the same boundary.
//
// ⚠️⚠️ THE PS3 DWARF'S DECLARATION ORDER IS **NOT** THE X360 ORDER, SO IT IS NOT USED HERE
// AS A LAYOUT SOURCE. The DWARF (BrnGuiEventTypeDefs.h:2382-2412) lists
// meFinishedGameModeType first and mbHasRankedUp fourth-from-last; the X360 asm puts
// meFinishedGameModeType at +24 and mbHasRankedUp LAST at +184, and gathers every small
// member into one run at the tail. C++ does not reorder members, so ARTIST's header genuinely
// declares them in a different order (the FIGS->main merge-window delta, AGENTS.md BUILD
// LINEAGE). Every offset below therefore comes from the ARTIST **assembly**, and the DWARF is
// used only to supply the NAME once the asm has pinned the slot.
//
// EVERY NAMED MEMBER IS ATTESTED BY THE BINARY, one at a time:
//   +0x00  mNewlyUnlockedRivalID   `ld r11, 0x2278(r31)` in SelectSubstates @0x824D5A4C --
//          an EIGHT-byte load whose truth gates mabSubStateFlags[RANK_UP_SHOWING_RIVALS].
//          ⚠️ Hex-Rays renders this load as `*(a1 + 8828)` (i.e. +0x227C); the ASM says
//          0x2278 and the asm is rung 1. Do not "correct" this back to +4.
//   +0x10  mBeatenRival            `ld`, streamed by the debug print "Results.Beaten Rival = "
//                                  through the CgsID formatter (sub_82203EE8), not the int one.
//   +0x18  meFinishedGameModeType  `lwz r11, 0x2290(r31)` (SelectSubstates @0x824D59DC);
//          streamed as "OFFLINE INSTANT RESULTS RECEIVED FOR GAME MODE = " and switched on by
//          SetupComponents @0x824B3FF0.
//   +0x20  mfTime                  SPrintf'd into "POSTRACE_FINISH_YOUR_TIME" (SetupComponents
//                                  case 5) as a float.
//   +0x30  maCarsToUnlockFromSpecialEvent  `Array<__int64,8>::GetLength(this + 8872)` in the
//          HandleIncomingEvents debug print "Results.Cars To Unlock Count = ". The DWARF types
//          it DerivedCarArray, and BrnDerivedCars.h:2 declares
//          `DerivedCarArray : public Array<CgsID,8u>` -- i.e. Array<u64,8>, whose committed
//          layout is {T maElements[N]; s32 miCount;}. TRIPLE CHECK: that puts miCount at
//          0x2278+0x30+64 == 0x22E8, which is EXACTLY where the class constructor
//          @0x825007F8 emits `stw r11(-1)` -- and CgsArray.h's own comment already records
//          that the -1 KI_UNCONSTRUCTED sentinel "the X360 emits as a single `stw -1`".
//   +0xA0..+0xAC  miPlayerOldRank / miPlayerNewRank / miCarsRevealed / miEventsUnlocked --
//          streamed by name: "Results.Rank Up (Old -> New) = ... ( old = ", ", new = ",
//          "Results.Cars Revealed = ", "Results.Events Unlocked = ".
//   +0xB0  miPlayerFinishPosition  streamed "Results.Player Finish Position = "; SetupComponents
//          uses (it - 1) as the index into KAC_FINISH_POS_STRINGIDS[8] and asserts
//          "liFinishPositionIndex < KI_NUM_FINISH_POS_STRINGS".
//   +0xB6  mbCompletedLastRank     streamed "Results.Completed Last Rank = " (bool formatter).
//   +0xB7  mbHasUnlockedFreeCar    streamed "Results.Unlocked Free Car = "; `lbz 0x232F`.
//   +0xB8  mbHasRankedUp           `lbz r11, 0x2330(r31)` gating mabSubStateFlags[RANK_UP_LICENSE].
//
// ⛔ WHAT IS DELIBERATELY LEFT UNNAMED (six spans). The DWARF supplies four more flag names
// (mbCrashedOut / mbTimedOut / mbEliminated / mbCountsTowardsProgression) plus miModeScore,
// miBaseScore, miScoreMultiplier, mfDistanceTravelled and mNewlyUnlockedFreeCarID, but the
// X360 asm does not pin ANY of them to a slot from this class's code, and the DWARF order is
// already proven wrong for this struct. SetupComponents reads +0xB3 where the string is
// "POSTRACE_OUTOFTIME" (so +0xB3 is a timed-out flag) but the DWARF run would put mbEliminated
// there -- i.e. naming that run from the DWARF would demonstrably mis-name at least one field.
// They are therefore reserved spans, not guesses. Name them when a producer-side write pins
// them (the writer is GuiCache +40552; that is the place to look, not this consumer).
struct GuiEventOfflinePostEvent
{
    struct OfflinePostEventData
    {
        CgsID mNewlyUnlockedRivalID;             // +0x00  ld @0x824D5A4C (gates substate 7)
        u8    maReserved08[8];                   // +0x08  UNNAMED -- no X360 access from this class
        CgsID mBeatenRival;                      // +0x10  ld; "Results.Beaten Rival = "
        s32   meFinishedGameModeType;            // +0x18  lwz @0x824D59DC; switched on
        s32   miModeScore;                       // +0x1C  SetupComponents case 7/9 SPrintf's it
                                                 //   "%d" into "POSTRACE_FINISH_YOUR_SCORE_POINTS"
                                                 //   (== "your score"), beside GetTargetScoreInEvent
                                                 //   for the target -- so this word is the PLAYER's
                                                 //   mode score. String-id attested, not inferred.
        f32   mfTime;                            // +0x20  "POSTRACE_FINISH_YOUR_TIME"
        // ---- the SHOWTIME trio (named 2026-08-29, was `u8 maReserved24[12]`) -------------
        // These are the three slots BrnGui::ShowtimeInstantResultsState reads, and they are
        // named from TWO independent sources that agree:
        //   (a) the DecFIGS DWARF declares exactly `int32_t miBaseScore; int32_t
        //       miScoreMultiplier; float32_t mfDistanceTravelled;` as a consecutive run
        //       (BrnGuiEventTypeDefs.h:2393/:2394/:2395), and this 12-byte hole is the only
        //       3-slot run left unaccounted for in the X360 record;
        //   (b) the X360 pins the MIDDLE one by name in its own stringized assert --
        //       UpdateScoreTotalling @0x824C62C4 fires `"mResults.miScoreMultiplier > 0"`
        //       on the word it loads from +0x28. With the middle slot nailed, the DWARF's
        //       declaration order fixes the two either side.
        // Corroborating uses: SetupTotalling @0x824BB548 multiplies mfDistanceTravelled by
        // 1.0936133 (the metres->yards constant flt_820DB5A8) and UpdateScoreTotalling hands
        // it to SetLocalisedText under "KAC_SHOWTIME_DISTANCE_IN_METRES" -- i.e. it really is
        // a distance in metres; and miScoreMultiplier - 1 is the multiplier SetupTotalling
        // scales the score by. Layout-neutral: three 4-byte slots in the same 12 bytes.
        s32   miBaseScore;                       // +0x24  DWARF :2393
        s32   miScoreMultiplier;                 // +0x28  DWARF :2394, ASSERT-ATTESTED
        f32   mfDistanceTravelled;               // +0x2C  DWARF :2395 (metres)
        Array<CgsID, 8> maCarsToUnlockFromSpecialEvent;  // +0x30  GetLength; miCount @0x22E8
        u8    maReserved78[32];                  // +0x78  UNNAMED
        s32   miCtorSentinel98;                  // +0x98  a SECOND -1 CgsArray sentinel. ITS
                                                 //   OWNING ARRAY IS DELIBERATELY NOT NAMED:
                                                 //   the element type and capacity are not
                                                 //   attested anywhere, so declaring an
                                                 //   Array<T,N> here would be a guess. What IS
                                                 //   attested is the store -- and twice over,
                                                 //   from both sides of the copy: this class's
                                                 //   ctor @0x825007FC emits `stw -1, 0x2310`
                                                 //   (== record + 0x98), and the PRODUCER's own
                                                 //   reconstruction independently recorded the
                                                 //   twin at GuiCache +0x9F00 (== record + 0x98
                                                 //   again, with the record based at +0x9E68) as
                                                 //   "ctor writes -1 (CgsArray sentinel;
                                                 //   sub-array un-homed)". Two independent waves,
                                                 //   two independent addresses, same slot.
        u8    maReserved9C[4];                   // +0x9C  UNNAMED
        s32   miPlayerOldRank;                   // +0xA0  "( old = "
        s32   miPlayerNewRank;                   // +0xA4  ", new = "
        s32   miCarsRevealed;                    // +0xA8  "Results.Cars Revealed = "
        s32   miEventsUnlocked;                  // +0xAC  "Results.Events Unlocked = "
        s8    miPlayerFinishPosition;            // +0xB0  "Results.Player Finish Position = "
        u8    maReservedB1[5];                   // +0xB1  UNNAMED flag run (see the ⛔ above)
        bool  mbCompletedLastRank;               // +0xB6  "Results.Completed Last Rank = "
        bool  mbHasUnlockedFreeCar;              // +0xB7  lbz @0x824D5A3C
        bool  mbHasRankedUp;                     // +0xB8  lbz @0x824D59F0
        u8    maReservedB9[7];                   // +0xB9  tail padding to the attested 192
    };

    // The wire record for GUI event id 289 IS the data (see the tombstone left in
    // BrnGuiDemangledEventTypes.h): the attested record size is 192 and the record carries no
    // CgsGui::GuiEvent<289> header, exactly like GuiEventPrepareForModeStart (id 93, 152).
    OfflinePostEventData mPostEventData;

    s32 GetEventType() const { return 289; }
};
static_assert(sizeof(GuiEventOfflinePostEvent) == 192,
              "GuiEventOfflinePostEvent id 289 record is 192 bytes of payload, no GuiEvent<289> header");

// The memcpy length AND the distance to the next member both say 192; this asserts we
// reproduced it. It holds on x64 too because the record contains no pointers -- CgsID is
// u64 on both sides, so nothing widens.
static_assert(sizeof(GuiEventOfflinePostEvent::OfflinePostEventData) == 192,
              "OfflinePostEventData is 192 bytes (memcpy @0x824DBAD8 case 64; and "
              "0x2338 - 0x2278 == 192)");
static_assert(__builtin_offsetof(GuiEventOfflinePostEvent::OfflinePostEventData, mBeatenRival)           == 0x10 &&
              __builtin_offsetof(GuiEventOfflinePostEvent::OfflinePostEventData, meFinishedGameModeType) == 0x18 &&
              __builtin_offsetof(GuiEventOfflinePostEvent::OfflinePostEventData, mfTime)                 == 0x20 &&
              __builtin_offsetof(GuiEventOfflinePostEvent::OfflinePostEventData, maCarsToUnlockFromSpecialEvent) == 0x30 &&
              __builtin_offsetof(GuiEventOfflinePostEvent::OfflinePostEventData, miPlayerOldRank)        == 0xA0 &&
              __builtin_offsetof(GuiEventOfflinePostEvent::OfflinePostEventData, miPlayerFinishPosition) == 0xB0 &&
              __builtin_offsetof(GuiEventOfflinePostEvent::OfflinePostEventData, mbHasRankedUp)          == 0xB8,
              "OfflinePostEventData slot drift vs the X360 loads (0x2278/0x2288/0x2290/0x2298/"
              "0x22A8/0x2318/0x2328/0x2330 with the record based at 0x2278)");

} // namespace BrnGui
