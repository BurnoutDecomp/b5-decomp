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
            return *reinterpret_cast<const s16*>(&maHeadReserved[0x20 - 0x10]);
        }

    private:
        // ---- leading payload (PS3 DWARF :1699-1707) ----
        // The DWARF-documented head order is, at byte offsets: Vector3 mv3Position
        // @0x00; CgsID mCgsId @0x10; f32 mfRotation @0x18; f32 mfSpeedMph @0x1C;
        // LandmarkIndex @0x20; u8 mu8DesignIndex; bool mbIsHiddenDriveThru. Only the
        // 16-byte position lane (read/written by DoWorstCase via lvx128/stvx128) is
        // pinned by the X360 binary; the remaining head bytes' exact inter-member
        // padding is NOT independently verified, so they stay an explicitly-reserved
        // mid block. This keeps the four verified trailing bytes at their proven
        // offsets. See LAYOUT NOTE at top of file.
        Vector4 mv4Position;          // @0x00 .. 0x0F  (X360-pinned; DoWorstCase lvx128)
        u8      maHeadReserved[0x13]; // @0x10 .. 0x22  (named head tail, see comment above)
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

    // ---- reconstructed out-of-line bodies (X360 ARTIST) ----
    // @0x823B1980 — recompute the "worst case" sat-nav icon set: validate the active
    // icons, compact the located player-position icon to the front, then synthesise a
    // ring of placeholder icons. Returns `this`.
    GuiEventUpdateSatNav* DoWorstCase();

    // @0x823A6B30 — range-check + return the leading icon's icon-type byte (a static
    // helper the CrashNavMap callers use). Reads maIconInfo[0].mi8IconType.
    static s32 SatNavI(const GuiEventUpdateSatNav* lpThis);
};

// ===================================================================================
// BrnGui::GuiEventDrawEventIcons -- the "draw event icons" GUI event payload.
//   Home: this header (Gui/BrnGuiEventTypeDefs.h; the X360 asserts reference it:
//   Construct @0x824EB218 -> BrnGuiEventTypeDefs.h:2785; GetIgnoreIcons @0x82443518 ->
//   :2815/:2816). Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Construct populates the event from an icon-set descriptor: a per-event float, two
// ids, a flag byte, and an "icons to ignore" list (up to KI_MAX_ICONS_TO_IGNORE = 10
// entries) copied in from a caller-supplied source array. GetIgnoreIcons reads the
// list back out (count + the entries).
//
// X360 layout (stores authoritative on width / offset):
//   maIgnoreIcons[10]    @0x00..0x27  (the copied list; Construct loop / GetIgnoreIcons)
//   mfDisplayTime        @0x28        (stfs f1)
//   muIconSetId          @0x2C        (stw a3)
//   miNumIconsToIgnore   @0x30        (stw count -- read by both methods)
//   mbFlag               @0x34        (stb a2)
// ===================================================================================
class GuiEventDrawEventIcons
{
public:
    // BrnGuiEventTypeDefs.h:2785 guard bound -- max entries in the ignore list.
    static const s32 KI_MAX_ICONS_TO_IGNORE = 10;

    // @0x824EB218 -- populate the event. lbFlag/luIconSetId/lfDisplayTime are stored
    // verbatim; the ignore list (lpuIconsToIgnore[0..liNumIconsToIgnore-1]) is copied
    // into maIgnoreIcons. Asserts the list is empty OR (non-NULL and within bound).
    // Returns `this`.
    GuiEventDrawEventIcons* Construct(u8 lbFlag,
                                      u32 luIconSetId,
                                      f32 lfDisplayTime,
                                      s32 liUnused,
                                      const u32* lpuIconsToIgnore,
                                      s32 liNumIconsToIgnore);

    // @0x82443518 -- copy the ignore list back out. Asserts both pointers non-NULL,
    // writes the count to *lpiNumIconsToIgnore, copies the entries to lpuIconsToIgnore.
    // Returns `this`.
    GuiEventDrawEventIcons* GetIgnoreIcons(u32* lpuIconsToIgnore, s32* lpiNumIconsToIgnore) const;

private:
    u32 maIgnoreIcons[KI_MAX_ICONS_TO_IGNORE]; // @0x00 -- the copied "icons to ignore" list
    f32 mfDisplayTime;                         // @0x28
    u32 muIconSetId;                           // @0x2C
    s32 miNumIconsToIgnore;                    // @0x30 -- entry count
    u8  mbFlag;                                // @0x34
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
struct GuiNewBurnoutSkillzEvent : public CgsGui::GuiEvent<526>
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
struct GuiNewBurnoutHudMessageEvent : public CgsGui::GuiEvent<527>
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
struct GuiLiveRevengeUpdateEvent : public CgsGui::GuiEvent<364>
{
    s32                 miDifference;                   // DWARF h:3725 (the points delta)
    s32                 meNewStatus;                    // DWARF h:3726 (BrnNetwork LiveRevengeStatus; raw s32 -- enum home pending)
    EActiveRaceCarIndex meAggressorActiveRaceCarIndex;  // DWARF h:3727
    EActiveRaceCarIndex meVictimActiveRaceCarIndex;     // DWARF h:3728
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
struct GuiEventRoadRuleEnter : public CgsGui::GuiEvent<329>
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
struct GuiEventRoadRuleUpcomingRoads : public CgsGui::GuiEvent<337>
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

} // namespace BrnGui
