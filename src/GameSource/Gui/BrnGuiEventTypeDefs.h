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

    s32 GetEventType() const { return 142; }
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

        // ---- declared-only setters/getters (DWARF :1710-1731), out of scope ----
        void          SetCounty(BrnWorld::ECounty leCounty);
        void          SetDistrict(BrnWorld::EDistrict leDistrict);
        void          SetActiveRaceCarIndex(EActiveRaceCarIndex leIndex);
        void          SetIconType(SatNavIconType leType);
        SatNavIconType GetIconType() const;

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
            return *reinterpret_cast<const s16*>(&maHeadReserved[0x20 - 0x18]);
        }

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
        u8      maHeadReserved[0x0B]; // @0x18 .. 0x22  (mfRotation / mfSpeedMph / the
                                      //   landmark-index half remain reserved)
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
    bool                        mbRestoreDefaults;                // +0x08 (1 = tear the post-fx down)

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

// The leader-name slot the road-rules events carry. FLAG: the X360 interior pins
// (GuiEventRoadRuleEnter's maeRoadRuleLeaderType at event+0x18 with mRoadId at +0
// and maFriendLeader between) size this at 8 bytes -- most likely a hashed name
// (CgsID); the PlayerName type's own home lands with the events TU.
struct PlayerName
{
    CgsID mNameId;   // FLAG: 8-byte slot per the X360 offset arithmetic
};

// The world/game actions the road-rules event Constructs consume (pointer-only).
struct RoadRulesEnterRoadAction;
struct RoadRulesUpdateTargetScoreAction;
struct UpcomingRoadChangeAction;

// DWARF :1071 -- "entered a road-ruled road" (the road-rules panel refresh
// payload). X360 sizeof 120 (RoadRuleComponent copies it whole @+0x3E8).
// X360-DIVERGENCE NOTE: the PS3 DWARF lists mRoadId near the tail (:1083), but the
// X360 RoadRuleComponent reads the event's LEADING u64 as the road id
// (EnterRoad @0x82441460 `ld r11,0x3E8(this)`; HandleLeaveRoadEvent @0x82413D98
// `cmpld` against it) -- the X360 layout pins mRoadId FIRST; the remaining member
// order is kept from the DWARF (unverified interior).
// X360 AddGuiEvent<GuiEventRoadRuleEnter> @0x823D69F0 bakes id 333 (was PS3-DWARF 329).
struct GuiEventRoadRuleEnter : public CgsGui::GuiEvent<333>
{
    // X360-PINNED head: mRoadId @+0x00 (EnterRoad/HandleLeaveRoadEvent read it as
    // the event's leading u64) and maeRoadRuleLeaderType @+0x18 (EnterRoad passes
    // event+0x18 as the const RoadRuleLeaderType* to GetRoadSignColour). FLAG: the
    // members past +0x20 keep the PS3 DWARF order but their X360 offsets are
    // unverified interior (nothing in-tree reads them yet).
    CgsID                 mRoadId;                                                         // :1083 (X360 +0x00)
    PlayerName            maFriendLeader[BrnStreetData::E_SCORE_TYPE_COUNT];               // :1074 (X360 +0x08)
    RoadRuleLeaderType    maeRoadRuleLeaderType[BrnStreetData::E_SCORE_TYPE_COUNT];        // :1076 (X360 +0x18)
    RoadRuleLeaderType    maeOfflineRoadRuleLeaderType[BrnStreetData::E_SCORE_TYPE_COUNT]; // :1077
    RoadRuleLeaderType    maeOnlineRoadRuleLeaderType[BrnStreetData::E_SCORE_TYPE_COUNT];  // :1078
    CgsID                 maAILeaderId[BrnStreetData::E_SCORE_TYPE_COUNT];                 // :1075 (FLAG: interior)
    bool                  mabChallenge[BrnStreetData::E_SCORE_TYPE_COUNT];                 // :1079
    s32                   maiBestValues[BrnStreetData::E_SCORE_TYPE_COUNT];                // :1080
    s32                   maiBestOfflineValues[BrnStreetData::E_SCORE_TYPE_COUNT];         // :1081
    s32                   maiBestOnlineValues[BrnStreetData::E_SCORE_TYPE_COUNT];          // :1082
    BrnStreetData::RoadIndex miRoadIndex;                                                 // :1084

    // DWARF :1089/:1093 -- their own ledger functions (declaration-only).
    void Construct(const RoadRulesEnterRoadAction* lpAction);
    void Construct();
};

// DWARF :1215 -- the two upcoming junction roads (sides, states, rulers). X360
// sizeof 128 (RoadRuleComponent block-copies it @+0x460 / to a local).
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

    RoadRuleLeaderType maaeLeaderTypes[E_ROAD_COUNT][BrnStreetData::E_SCORE_TYPE_COUNT];        // :1235
    RoadRuleLeaderType maaeOfflineLeaderTypes[E_ROAD_COUNT][BrnStreetData::E_SCORE_TYPE_COUNT]; // :1236
    RoadRuleLeaderType maaeOnlineLeaderTypes[E_ROAD_COUNT][BrnStreetData::E_SCORE_TYPE_COUNT];  // :1237
    CgsID              mRoadIds[E_ROAD_COUNT];                                                 // :1238
    BrnStreetData::RoadIndex maiTurningRoadIndices[E_ROAD_COUNT];                              // :1239
    BrnStreetData::RoadIndex miCurrentRoadIndex;                                               // :1240
    ERoadState         meRoadStates[E_ROAD_COUNT];                                             // :1241
    ERoadState         meCurrentSignState;                                                     // :1242
    Vector3            maRoadEntrancePosition[E_ROAD_COUNT];                                   // :1243 (X360 +0x60, 16-aligned)

    // DWARF :1247/:1252 -- their own ledger functions (declaration-only).
    void Construct();
    void Construct(const UpcomingRoadChangeAction* lpAction);
};

// Pointer-only in the road-rule component surface (their full DWARF records land
// with their consumers): :1111 GuiEvent<335> / :1168 GuiEvent<332>.
struct GuiEventRoadRuleUpdateTargetScores;
struct GuiEventRoadRuleEnd;

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

// DWARF BrnGuiEventTypeDefs.h:3565 area (GuiEvent<358>; X360 id 363, record 40 bytes).
// X360 FIELD ORDER (binary authoritative, differs from the PS3-DWARF member listing):
// the car-id qwords lead and the race-car indices follow -- pinned on BOTH sides:
//   producer: BrnGame::TranslateTakedownsToGuiEvents @0x823E1C38 (see the store map in
//             GameBridgeGameStateToX.h: ids @+0x00/+0x08, indices @+0x10/+0x14,
//             type @+0x18, chain @+0x1C, multi @+0x20, status bytes @+0x24/+0x25)
//   consumer: HudMessageAnalyzer::HandleTakedown @0x8251C3C0 (reads +0x10/+0x14/+0x18/
//             +0x24/+0x25) and ConstructTakedownMessage @0x824F9C28 (reads type @+0x18,
//             chain @+0x1C with the <2 / -2<9 ladder, multi @+0x20 with the <=1 gate).
// Member NAMES from the DWARF. (GameBridgeGameStateToX.h keeps a file-local FLAGGED
// placeholder of this record for its own TU; the include graphs do not meet.)
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

// DWARF :4252 (PS3 GuiEvent<338>; X360 id 342, record 48 bytes). The analyzer's
// HandleNewRoadRulesHighScore @0x824F32A0 block-copies the whole 48-byte record into
// mRoadRuleHighScoreData (no field is read there), so only the DWARF member ORDER is
// carried here. FLAG: the interior X360 offsets are unverified (nothing in this TU reads
// them); the 16-byte PlayerName head + the 48-byte total are the X360-load-bearing facts.
struct GuiEventRoadRuleNewHighScore
{
    CgsNetwork::PlayerName   mPlayerName;                // DWARF :1275 (16-byte name)
    CgsID                    mRoadId;                    // DWARF :1276
    BrnStreetData::ScoreType meScoreType;                // DWARF :1277
    s32                      miNumScoresLost;            // DWARF :1278
    s32                      miNumRoadsNowRuled;         // DWARF :1279
    bool                     mbIsLocalPlayer;            // DWARF :1280
    bool                     mbIsWholeRoadOwned;         // DWARF :1281
    bool                     mbWasRulePlayersBefore;     // DWARF :1282
    bool                     mbMultipleScores;           // DWARF :1283
    bool                     mbOnlineLossButOfflineWin;  // DWARF :1284

    s32 GetEventType() const { return 342; }
};
static_assert(sizeof(GuiEventRoadRuleNewHighScore) == 48, "X360 AddGuiEvent size 48 (id 342)");

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

} // namespace BrnGui
