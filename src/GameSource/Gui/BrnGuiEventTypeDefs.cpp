// BrnGuiEventTypeDefs.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The four range-checked accessors on
// BrnGui::GuiEventUpdateSatNav::SatNavIconInfo. Each reads a single byte off `this`,
// runs two non-fatal CGS_ASSERT range guards (the X360 binary returns the raw byte
// even when a guard fails), and returns the value cast to its enum type.
//
//   GetCounty             @ 0x823A6A20  byte @0x24 (zero-extended)  guards: >=0, < E_COUNTY_COUNT(6)
//   GetDistrict           @ 0x823A6AA8  byte @0x25 (zero-extended)  guards: >=0, < E_DISTRICT_COUNT(0x13)
//   GetActiveRaceCarIndex @ 0x824B2EF8  byte @0x26 (sign-extended)  guards: >= INVALID(-1), < COUNT(8)
//   GetPlayerTeam         @ 0x824EB190  byte @0x27 (sign-extended)  guards: >= START(0), < COUNT(9)
//
// The X360-baked assert file/line are discarded per project convention; the stringized
// condition matches the X360 assert message text.

#include "GameSource/Gui/BrnGuiEventTypeDefs.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"  // CgsCore::SPrintf / SnPrintf
#include "GameShared/GameClasses/Core/CgsID.h"           // CgsID / CgsIDCompress
#include "GameShared/GameClasses/Fonts/CgsUnicode.h"     // CgsUnicode::SafelyTerminate (GuiHudMessage::GetParam)

#include <cstring>  // std::memcpy -- the DoWorstCase compaction is a 0x30-byte block move.

namespace BrnGui
{

// File-scope sat-nav "worst case" animation phase (X360 flt_82FB508C). A persistent
// scalar advanced each DoWorstCase call: clamped-ramp up by 1.5 while <= 100, else reset
// to 0. Used to wobble the synthesised icon-ring X coordinate. The X360 stored this in a
// module global; modelled here as the equivalent translation-unit-local scalar.
static f32 sfWorstCasePhase = 0.0f;

// X360 immediates baked into DoWorstCase (named for clarity; values from the asm float
// pool): the phase ramp limit, the per-call phase step, and the icon-ring spacing/offset.
static const f32 KF_WORST_CASE_PHASE_LIMIT = 100.0f; // flt_820049E0
static const f32 KF_WORST_CASE_PHASE_STEP  = 1.5f;   // flt_82004D04
static const f32 KF_WORST_CASE_PHASE_RESET = 0.0f;   // flt_82001CC0
static const f32 KF_WORST_CASE_RING_STEP   = 50.0f;  // flt_820138DC (ring Z spacing & X offset)

// @0x824F6350 -- copy the three names into their fixed 32-byte fields, force
// termination at byte 31, then store the action enum between component and label.
void GuiAudioTriggerEvent::Construct(s32 leAction, const char* lpComponentName,
                                     const char* lpLabel, const char* lpMovieName)
{
    CGS_ASSERT(lpComponentName && lpLabel && lpMovieName,
               "lpComponentName && lpLabel && lpMovieName");

    CgsCore::SPrintf(macComponent, sizeof(macComponent), lpComponentName);
    macComponent[31] = 0;
    CgsCore::SPrintf(macLabel, sizeof(macLabel), lpLabel);
    macLabel[31] = 0;
    CgsCore::SPrintf(macMovie, sizeof(macMovie), lpMovieName);
    macMovie[31] = 0;
    meAction = leAction;
}

// @ 0x823A6A20 — zero-extended byte; guards leCounty >= 0 (vacuous for an unsigned
// byte but the X360 still emits it) and leCounty < BrnWorld::E_COUNTY_COUNT.
BrnWorld::ECounty GuiEventUpdateSatNav::SatNavIconInfo::GetCounty() const
{
    const u8 luCounty = mu8County;
    CGS_ASSERT( luCounty >= 0, "leCounty >= 0" );
    CGS_ASSERT( luCounty < BrnWorld::E_COUNTY_COUNT, "leCounty < BrnWorld::E_COUNTY_COUNT" );
    return static_cast<BrnWorld::ECounty>( luCounty );
}

// @ 0x823A6AA8 — zero-extended byte; guards leDistrict >= 0 and
// leDistrict < BrnWorld::E_DISTRICT_COUNT (0x13).
BrnWorld::EDistrict GuiEventUpdateSatNav::SatNavIconInfo::GetDistrict() const
{
    const u8 luDistrict = mu8District;
    CGS_ASSERT( luDistrict >= 0, "leDistrict >= 0" );
    CGS_ASSERT( luDistrict < BrnWorld::E_DISTRICT_COUNT, "leDistrict < BrnWorld::E_DISTRICT_COUNT" );
    return static_cast<BrnWorld::EDistrict>( luDistrict );
}

// @ 0x824B2EF8 — sign-extended byte; guards leActiveRaceCarIndex >= INVALID(-1) and
// < E_ACTIVE_RACE_CAR_INDEX_COUNT(8).
EActiveRaceCarIndex GuiEventUpdateSatNav::SatNavIconInfo::GetActiveRaceCarIndex() const
{
    const s8 liActiveRaceCarIndex = mi8ActiveRaceCarIndex;
    CGS_ASSERT( liActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_INVALID,
                "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_INVALID" );
    CGS_ASSERT( liActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT" );
    return static_cast<EActiveRaceCarIndex>( liActiveRaceCarIndex );
}

// @ 0x824EB190 (IDA "GetPlay") — sign-extended byte; guards lePlayerTeam >= START(0)
// and < GsmIO::E_PLAYER_TEAM_COUNT(9).
EPlayerTeam GuiEventUpdateSatNav::SatNavIconInfo::GetPlayerTeam() const
{
    const s8 liPlayerTeam = mi8PlayerTeam;
    CGS_ASSERT( liPlayerTeam >= E_PLAYER_TEAM_START, "lePlayerTeam >= GsmIO::E_PLAYER_TEAM_START" );
    CGS_ASSERT( liPlayerTeam < E_PLAYER_TEAM_COUNT, "lePlayerTeam < GsmIO::E_PLAYER_TEAM_COUNT" );
    return static_cast<EPlayerTeam>( liPlayerTeam );
}

// ---------------------------------------------------------------------------
// GuiEventUpdateSatNav::SatNavI @ 0x823A6B30
//
//   lbz r11,0x28(r3) ; extsb r31,r11           -> v = (s8)maIconInfo[0].mi8IconType
//   assert v >= 0      ("leIconType >= 0",      BrnGuiEventTypeDefs.h:1911)
//   assert v < 14      ("leIconType < E_SATNAVICON_MAX", :1912)
//   return v
//
// Sign-extends and range-checks the leading icon's icon-type byte (offset +0x28),
// returning it. The X360 guards are non-fatal (the raw byte is returned regardless).
// ---------------------------------------------------------------------------
s32 GuiEventUpdateSatNav::SatNavI(const GuiEventUpdateSatNav* lpThis)
{
    const s32 liIconType = lpThis->maIconInfo[0].mi8IconType; // lbz;extsb -> sign-extended
    CGS_ASSERT( liIconType >= 0, "leIconType >= 0" );
    CGS_ASSERT( liIconType < SatNavIconInfo::E_SATNAVICON_MAX, "leIconType < E_SATNAVICON_MAX" );
    return liIconType;
}

// ---------------------------------------------------------------------------
// GuiEventUpdateSatNav::DoWorstCase @ 0x823B1980
//
// Three phases (store order / offsets per the X360 asm; see BrnGuiEventTypeDefs.h
// outer-struct grow banner):
//
//   1. Scan the active icons for the player-car entry (icon-type 0). For each scanned
//      icon assert leIconType in [0, E_SATNAVICON_MAX); stop at the first type-0 icon;
//      if the scan reaches the last active icon without finding it, fire the X360
//      "FAILED to find player pos to do worst case!" assert. If the player icon was
//      found at index liPlayerIdx > 0, compact it to the front (a 0x30-byte block move).
//
//   2. Advance the module animation phase (sfWorstCasePhase): ramp +1.5 while <= 100,
//      else reset to 0.
//
//   3. Synthesise a 7-icon "worst case" ring (icons[1..7]) around the player icon's
//      position, all tagged E_SATNAVICON_NETWORKRIVAL (== 2) with the player icon's
//      county/district, an incrementing per-icon index, and a ring position offset by
//      the phase/step. Finally set the icon count to 8.
//
// The X360 used VMX (lvx128/stvx128) to move the 16-byte position lanes; reproduced
// here as scalar Vector4 lane assignments (semantic parity, endian-independent).
// ---------------------------------------------------------------------------
GuiEventUpdateSatNav* GuiEventUpdateSatNav::DoWorstCase()
{
    // ---- phase 1: locate the player-car icon (type 0) and compact it to the front ----
    s32 liPlayerIdx = 0;
    if ( miNumIcons > 0 )
    {
        do
        {
            const s32 liIconType = maIconInfo[liPlayerIdx].mi8IconType; // lbz;extsb
            CGS_ASSERT( liIconType >= 0, "leIconType >= 0" );
            CGS_ASSERT( liIconType < SatNavIconInfo::E_SATNAVICON_MAX, "leIconType < E_SATNAVICON_MAX" );
            if ( liIconType == 0 )
                break;
            CGS_ASSERT( liPlayerIdx != miNumIcons - 1,
                        "FAILED to find player pos to do worst case!" );
            ++liPlayerIdx;
        }
        while ( liPlayerIdx < miNumIcons );

        if ( liPlayerIdx != 0 )
        {
            std::memcpy( &maIconInfo[0], &maIconInfo[liPlayerIdx], sizeof(SatNavIconInfo) );
        }
    }

    // ---- phase 2: advance the animation phase ----
    if ( sfWorstCasePhase <= KF_WORST_CASE_PHASE_LIMIT )
        sfWorstCasePhase = sfWorstCasePhase + KF_WORST_CASE_PHASE_STEP;
    else
        sfWorstCasePhase = KF_WORST_CASE_PHASE_RESET;

    // ---- phase 3: synthesise the 7-icon ring (icons[1..7]) ----
    const SatNavIconInfo& lPlayer = maIconInfo[0];
    // FLAG (ARTIST 0x823B1980): the ring-Z accumulator (f31) is seeded to
    // player.z - 100.0 (lvx128 this; vspltw lane 2 -> player.z; vsubfp 100.0;
    // lfs f31, var_A0), NOT the bare constant 100.0. Each entry then does f31 += 50
    // before the store, giving icon[i].z = player.z - 100 + 50*i. The earlier
    // "starts at 100.0" reading was a misread of the vsubfp pre-step.
    f32 lfRingZ = lPlayer.mv4Position.z - KF_WORST_CASE_PHASE_LIMIT;
    for ( s32 liIndex = 1; liIndex < 8; ++liIndex )
    {
        SatNavIconInfo& lIcon = maIconInfo[liIndex];

        const u8 lu8County = lPlayer.mu8County;
        CGS_ASSERT( lu8County < BrnWorld::E_COUNTY_COUNT, "leCounty < BrnWorld::E_COUNTY_COUNT" );
        lIcon.mu8County = lu8County;

        const u8 lu8District = lPlayer.mu8District;
        CGS_ASSERT( lu8District < BrnWorld::E_DISTRICT_COUNT, "leDistrict < BrnWorld::E_DISTRICT_COUNT" );
        lIcon.mu8District = lu8District;

        lIcon.mi8IconType = SatNavIconInfo::E_SATNAVICON_NETWORKRIVAL; // _R31[3] = 2
        lfRingZ = lfRingZ + KF_WORST_CASE_RING_STEP;                   // v17 += 50
        lIcon.mi8ActiveRaceCarIndex = static_cast<s8>( liIndex );      // _R31[1] = v12++

        // position = { player.x + 50 - phase, player.y, ringZ, 0 }
        lIcon.mv4Position.x = ( lPlayer.mv4Position.x + KF_WORST_CASE_RING_STEP ) - sfWorstCasePhase;
        lIcon.mv4Position.y = lPlayer.mv4Position.y;
        lIcon.mv4Position.z = lfRingZ;
        lIcon.mv4Position.w = 0.0f;
    }

    miNumIcons = 8;
    return this;
}

// ===================================================================================
// BrnGui::GuiOverlayRequest -- the three out-of-line parameter accessors.
//
// Each copies one overlay parameter (id + text) into a caller-supplied output record:
// it SPrintf's the parameter text through "%s" into the output's 64-byte text buffer
// (output+0x04), then copies the parameter id into the output's leading dword
// (output+0x00, in that order, matching the X360 store sequence).
//
// RETURN-VALUE NOTE: the X360 functions return r3 == the char-count result of
// CgsCore::SPrintf. The committed CgsCore::SPrintf is declared `void` (it does not
// surface the printf char-count), so the count is not propagated here; the accessors
// return 0. The load-bearing behaviour is the copy side-effect, which is reproduced
// exactly. (See flagged_type_changes re: CgsCore::SPrintf's return type.)
// ===================================================================================

// @0x823B1CC8 -- initialise the request from an overlay-id string.
//   guard: a2 == 0 -> "Invalid Overlay Id" (BrnGuiEventTypeDefs.h:7404)
//   result = CgsIDCompress(a2)
//   std r3, 0(this)        -> the 8-byte CgsID over the leading param record head (+0x00)
//   stw 0,   0x118(this)   -> miNumMessages = 0
//   stb 0,   0x11C(this)   -> mbButton1Used = 0
//   stb 0,   0x11D(this)   -> mbButton2Used = 0
//   return result
//
// The X360 stores the compressed id at this+0x00 (`std r3,0(r27)`), i.e. over the first
// 8 bytes of maMessages[0] (its 8-byte maHead). Reproduced as an 8-byte block write to the
// head so the on-object id sits at the proven offset; sizeof(CgsID) == 8 matches the std.
CgsID GuiOverlayRequest::Construct(const char* lpcOverlayId)
{
    CGS_ASSERT(lpcOverlayId != 0, "Invalid Overlay Id");

    const CgsID lId = CgsIDCompress(lpcOverlayId);

    static_assert(sizeof(CgsID) == sizeof(maMessages[0].maHead),
                  "overlay id (std r3,0) overlays the first param record's 8-byte head");
    std::memcpy(&maMessages[0].maHead[0], &lId, sizeof(CgsID)); // std r3, 0(this)

    miNumMessages = 0;   // stw 0, 0x118(this)
    mbButton1Used = 0;   // stb 0, 0x11C(this)
    mbButton2Used = 0;   // stb 0, 0x11D(this)
    return lId;
}

// @0x82472A18 -- append one message param at the next free slot.
//   guard: *(this+0x118) >= 2 -> streamed "Not enough free params in the Overlay (<n>/2)."
//                                (BrnGuiEventTypeDefs.h:7438)
//   guard: lpcParam == NULL   -> "lpcParam != NULL"  (h:7440)
//   entry = this + 0x44*count
//   SPrintf(entry+0x0C, 63, "%s", lpcParam);  stb 0, 0x4B(entry);  stw luId, 8(entry);  ++count
//   returns r3 == the SPrintf result (untouched between the call and the epilogue).
//
// ⚠️ CONSOLE-ATTESTED OVERRUN, reproduced deliberately: the text write spans entry+0x0C
// .. entry+0x4B inclusive (63 bytes + the forced NUL at +0x4B) == 64 bytes, but the param
// record stride is only 0x44. Each slot therefore spills its last 8 bytes into the FOLLOWING
// record's 8-byte maHead: slot 0 into maMessages[1].maHead, slot 1 into mButton1.maHead.
// Both are inside this object and neither head is read by any accessor (only
// maMessages[0].maHead is used, and Construct writes the overlay id there BEFORE any
// AddMessageParam call), so the spill is observationally inert -- but GetMessageParam reads
// the text back with "%s", so a >55-char param genuinely relies on those 8 bytes and the
// bytes must be written where the console writes them. Addressed through the record base with
// the offsets pinned below rather than through macText[0x38], which would be a declared-bound
// overrun.
s32 GuiOverlayRequest::AddMessageParam(u32 luId, const char* lpcParam)
{
    static_assert(sizeof(ParamInfo) == 0x44, "param record stride 0x44 (GetMessageParam: 0x44*idx)");
    static_assert(__builtin_offsetof(ParamInfo, muId) == 0x08, "param id @ record+0x08");
    static_assert(__builtin_offsetof(ParamInfo, macText) == 0x0C, "param text @ record+0x0C");
    // The console's 64-byte text write from the LAST slot must still land inside the object.
    static_assert(KI_MAX_MESSAGES * 0x44 - 0x44 + 0x0C + 64 <= sizeof(GuiOverlayRequest),
                  "the attested 64-byte text write stays within the request object");

    CGS_ASSERT(miNumMessages < KI_MAX_MESSAGES,
               "Not enough free params in the Overlay.");   // h:7438 (streamed "<n>/2." on the X360)
    CGS_ASSERT(lpcParam != 0, "lpcParam != NULL");          // h:7440

    ParamInfo& lParam = maMessages[miNumMessages];          // this + 0x44*count
    char* const lpacText = reinterpret_cast<char*>(&lParam) + 0x0C;

    CgsCore::SPrintf(lpacText, 0x3F, "%s", lpcParam);       // li r4, 0x3F
    lpacText[0x3F] = 0;                                     // stb 0, 0x4B(entry)  (0x4B-0x0C == 0x3F)
    lParam.muId    = luId;                                  // stw r24, 8(entry)
    ++miNumMessages;                                        // stw r11, 0x118(this)

    // The X360 leaves SPrintf's r3 in place and returns it. The committed CgsCore::SPrintf
    // reconstruction returns void (CgsStringUtils.h:11), so that value is not available here --
    // return 0, exactly as the sibling GetMessageParam/GetButton1Param/GetButton2Param bodies
    // already do for the same reason. No caller in the tree inspects the result.
    return 0;
}

// @0x824EB948 -- copy message param liIndex into *lpOut.
//   guard: liIndex >= *(this+0x118)  -> "Index isn't used in Overlay." (h:7508)
//   guard: liIndex <  0              -> "Index isn't valid."           (h:7509)
//   entry = this + 0x44*liIndex; SPrintf(out+4, 64, "%s", entry+0x0C); *out = *(entry+0x08)
s32 GuiOverlayRequest::GetMessageParam(ParamOut* lpOut, s32 liIndex) const
{
    // X360-pinned member offsets (validated from a member context where the private
    // members are visible to offsetof): the two button records at +0x88/+0xCC, the count
    // at +0x118 and the two guard bytes at +0x11C/+0x11D.
    static_assert(__builtin_offsetof(GuiOverlayRequest, mButton1) == 0x88, "button1 record @+0x88");
    static_assert(__builtin_offsetof(GuiOverlayRequest, mButton2) == 0xCC, "button2 record @+0xCC");
    static_assert(__builtin_offsetof(GuiOverlayRequest, miNumMessages) == 0x118, "count @+0x118");
    static_assert(__builtin_offsetof(GuiOverlayRequest, mbButton1Used) == 0x11C, "button1 guard @+0x11C");
    static_assert(__builtin_offsetof(GuiOverlayRequest, mbButton2Used) == 0x11D, "button2 guard @+0x11D");

    CGS_ASSERT( liIndex < miNumMessages, "Index isn't used in Overlay." );
    CGS_ASSERT( liIndex >= 0,            "Index isn't valid." );

    const ParamInfo& lParam = maMessages[liIndex]; // this + 0x44*liIndex
    CgsCore::SPrintf( lpOut->macText, sizeof(lpOut->macText), "%s", lParam.macText );
    lpOut->muId = lParam.muId;                      // *a2 = *(entry+0x08)
    return 0;
}

// @0x824EBA78 -- copy button-1 param into *lpOut.
//   guard: *(this+0x11C) == 0 -> "button 1 param isn't used in Overlay." (h:7528)
//   SPrintf(out+4, 64, "%s", this+0x94); *out = *(this+0x90)
s32 GuiOverlayRequest::GetButton1Param(ParamOut* lpOut) const
{
    CGS_ASSERT( mbButton1Used != 0, "button 1 param isn't used in Overlay." );

    CgsCore::SPrintf( lpOut->macText, sizeof(lpOut->macText), "%s", mButton1.macText );
    lpOut->muId = mButton1.muId;
    return 0;
}

// @0x824EBB38 -- copy button-2 param into *lpOut.
//   guard: *(this+0x11D) == 0 -> "button 2 param isn't used in Overlay." (h:7547)
//   SPrintf(out+4, 64, "%s", this+0xD8); *out = *(this+0xD4)
s32 GuiOverlayRequest::GetButton2Param(ParamOut* lpOut) const
{
    CGS_ASSERT( mbButton2Used != 0, "button 2 param isn't used in Overlay." );

    CgsCore::SPrintf( lpOut->macText, sizeof(lpOut->macText), "%s", mButton2.macText );
    lpOut->muId = mButton2.muId;
    return 0;
}

// ============================================================================
// [gateui] GuiHudMessage -- the message-BUILDING half. These four bodies were
// declaration-only, which made every HudMessageAnalyzer::Handle* body in the tree
// unlinkable: Construct @0x824F78B0 has 59 X360 callers and AddParam 46 + 17. They are
// the keystone of the whole HUD-message path, so they land here in the type's own TU
// alongside the already-committed reading half (GetParam / GetParamCount).
//
// X360 RECORD LAYOUT (from AddParam's own address arithmetic @0x824EB2C8, which is the
// only place all three members are indexed together):
//     +0x00  CgsID mMessageIdHash                 (Construct stores it here)
//     +0x08  s32   maiNoOfParams[3]               ("4 * (a3 + 2)" == +8 + 4*stringIndex)
//     +0x14  HudMessageParameter maaParams[3][4]  (base a1+20, stride 68, row stride 4;
//                                                  meParamType @param+0, macParameter
//                                                  @param+4 -- "a1 + 24" in the asm)
// The host record widens (the GuiEvent<152> base sits in front of mMessageIdHash), so
// every access below is BY NAME; the offsets above are recorded only to show how the
// member roles were read out of the asm.
// ============================================================================

// @0x824F78B0 -- hash the message id and clear the three parameter counts.
// The X360 body's assert is the streamed form ("Invalid Message Id",
// BrnGuiEventTypeDefs.h:7225) and is NON-GATING: it falls through into CgsIDCompress with
// the null pointer exactly as written.
void GuiHudMessage::Construct(const char* lpcMessageId)
{
    CGS_ASSERT( lpcMessageId != NULL, "Invalid Message Id" );

    mMessageIdHash = CgsIDCompress( lpcMessageId );
    maiNoOfParams[0] = 0;
    maiNoOfParams[1] = 0;
    maiNoOfParams[2] = 0;
}

// DWARF h:5626 -- the pre-hashed sibling. It has no standalone X360 symbol because every
// caller inlines it; the inline is verbatim in HandleStuntInfo @0x8251F650
// (`std r9, var_360` = the id straight into +0x00, then three `stw r10` zeroing
// +0x08/+0x0C/+0x10 @0x8251F69C..0x8251F6A4) -- i.e. the same body as the const char*
// overload minus the compress step and minus the null assert.
void GuiHudMessage::Construct(CgsID lMessageIdHash)
{
    mMessageIdHash = lMessageIdHash;
    maiNoOfParams[0] = 0;
    maiNoOfParams[1] = 0;
    maiNoOfParams[2] = 0;
}

namespace
{
    // The overflow report all three AddParam overloads share (X360 @0x824EB2C8 lines
    // "Invalid num of params ( N of max 4 ) in the Hud Message with ID <id>", gated on
    // CgsDev::Message::gxMessageFilterFlags & 1, followed by an unconditional
    // FireAssert("\n")). De-inlined here because the overloads emit it identically.
    //
    // ⚠ CONSOLE OFF-BY-ONE, REPRODUCED VERBATIM: the guard the three overloads apply is
    // `count > KI_MAX_PARAMS_PER_STRING`, not `>=` (X360 `cmplwi cr6, r11, 4; ble` at
    // 0x824EB2FC / 0x824EB53C / 0x824EB754). A string that already holds 4 parameters
    // therefore passes the guard and writes slot [4] -- one past the row. It is
    // unreachable on the shipped content (no HUD message in the image adds more than two
    // parameters to one display string) and CGS_ASSERT is non-gating in this build, so
    // "fixing" it to `>=` would be a silent behaviour change on a path the console never
    // takes. Left as the binary has it; flagged so nobody re-derives it as a typo.
    void ReportHudMessageParamOverflow(CgsID lMessageIdHash, s32 liCount, s32 liMax)
    {
        char lacMessageId[16];
        CgsIDUnCompress( lMessageIdHash, lacMessageId );

        if ( ( CgsDev::Message::gxMessageFilterFlags & 1 ) != 0 && CgsDev::Log::gpDebugPrint != 0 )
        {
            *CgsDev::Log::gpDebugPrint
                << "Invalid num of params ( " << liCount
                << " of max " << liMax
                << " ) in the Hud Message with ID " << lacMessageId
                << " \n";
        }

        CGS_ASSERT( false, "\n" );
    }
}

// @0x824EB2C8 -- append a STRING-valued parameter to display string liStringIndex.
// Register order (r4 type, r5 string index, r6 value) is the asm's, and it matches the
// DWARF declaration order; the pseudocode's a2/a3/a4 map to leType / liStringIndex /
// lpcValue respectively (a2 is what lands in the slot's meParamType, a3 is what the
// "Invalid string index." guard bounds, a4 is what SnPrintf formats).
void GuiHudMessage::AddParam(CgsGui::HudMessageParamTypes leType, s32 liStringIndex,
                             const char* lpcValue)
{
    CGS_ASSERT( static_cast<u32>(liStringIndex) <= 2u, "Invalid string index." );

    if ( maiNoOfParams[liStringIndex] > KI_MAX_PARAMS_PER_STRING )
        ReportHudMessageParamOverflow( mMessageIdHash, maiNoOfParams[liStringIndex],
                                       KI_MAX_PARAMS_PER_STRING );

    CGS_ASSERT( lpcValue != NULL, "lpcParam != NULL" );

    CgsGui::HudMessageParameter& lrParam = maaParams[liStringIndex][maiNoOfParams[liStringIndex]];
    CgsCore::SnPrintf( lrParam.macParameter,
                       CgsGui::HudMessageParameter::KI_MAX_PARAM_STRING_LENGTH,
                       "%s", lpcValue );
    CgsUnicode::SafelyTerminate( reinterpret_cast<CgsUnicode::CgsUtf8*>( lrParam.macParameter ),
                                 CgsGui::HudMessageParameter::KI_MAX_PARAM_STRING_LENGTH );
    lrParam.meParamType = leType;
    ++maiNoOfParams[liStringIndex];
}

// @0x824EB508 (unnamed in the export set; identified by its "%d" format and by being the
// callee of every `AddParam(msg, 2 /*INT*/, index, value)` site) -- the INT-valued
// overload. Identical to the string one except for the format and the missing null guard.
void GuiHudMessage::AddParam(CgsGui::HudMessageParamTypes leType, s32 liStringIndex,
                             s32 liValue)
{
    CGS_ASSERT( static_cast<u32>(liStringIndex) <= 2u, "Invalid string index." );

    if ( maiNoOfParams[liStringIndex] > KI_MAX_PARAMS_PER_STRING )
        ReportHudMessageParamOverflow( mMessageIdHash, maiNoOfParams[liStringIndex],
                                       KI_MAX_PARAMS_PER_STRING );

    CgsGui::HudMessageParameter& lrParam = maaParams[liStringIndex][maiNoOfParams[liStringIndex]];
    CgsCore::SnPrintf( lrParam.macParameter,
                       CgsGui::HudMessageParameter::KI_MAX_PARAM_STRING_LENGTH,
                       "%d", liValue );
    CgsUnicode::SafelyTerminate( reinterpret_cast<CgsUnicode::CgsUtf8*>( lrParam.macParameter ),
                                 CgsGui::HudMessageParameter::KI_MAX_PARAM_STRING_LENGTH );
    lrParam.meParamType = leType;
    ++maiNoOfParams[liStringIndex];
}

// @ 0x824EB720 (also unnamed in the export set; identified by its "%f" format
// @0x824EB8E4 and by being the callee of HandleLeaderPassedMileBoundary @0x8251C2F0 /
// HandleLeaderPassedKMBoundary @0x8251C398, both of which pass a float distance) -- the
// FLOAT-valued overload. PPC note: the value rides f1 and consumes no GPR slot, which is
// why Hex-Rays renders this one's parameter list a word short of the other two.
void GuiHudMessage::AddParam(CgsGui::HudMessageParamTypes leType, s32 liStringIndex,
                             f32 lfValue)
{
    CGS_ASSERT( static_cast<u32>(liStringIndex) <= 2u, "Invalid string index." );

    if ( maiNoOfParams[liStringIndex] > KI_MAX_PARAMS_PER_STRING )
        ReportHudMessageParamOverflow( mMessageIdHash, maiNoOfParams[liStringIndex],
                                       KI_MAX_PARAMS_PER_STRING );

    CgsGui::HudMessageParameter& lrParam = maaParams[liStringIndex][maiNoOfParams[liStringIndex]];
    CgsCore::SnPrintf( lrParam.macParameter,
                       CgsGui::HudMessageParameter::KI_MAX_PARAM_STRING_LENGTH,
                       "%f", lfValue );
    CgsUnicode::SafelyTerminate( reinterpret_cast<CgsUnicode::CgsUtf8*>( lrParam.macParameter ),
                                 CgsGui::HudMessageParameter::KI_MAX_PARAM_STRING_LENGTH );
    lrParam.meParamType = leType;
    ++maiNoOfParams[liStringIndex];
}

// @0x82674D60 -- copy display-string liStringIndex's parameter liParamIndex into *lpOut:
// format the value string (bounded), UTF-8-safely terminate it, and copy the param type.
// Returns the SafelyTerminate result (X360 r3).
CgsUnicode::CgsUtf8* GuiHudMessage::GetParam(CgsGui::HudMessageParameter* lpOut,
                                             s32 liStringIndex, s32 liParamIndex) const
{
    CGS_ASSERT( liStringIndex >= 0 && liStringIndex <= KI_NUMBER_OF_STRINGS - 1,
                "Invalid string index." );
    CGS_ASSERT( liParamIndex < maiNoOfParams[liStringIndex], "Index isn't used in Message." );
    CGS_ASSERT( liParamIndex >= 0, "Index isn't valid." );

    const CgsGui::HudMessageParameter& lParam = maaParams[liStringIndex][liParamIndex];

    CgsCore::SnPrintf( lpOut->macParameter,
                       CgsGui::HudMessageParameter::KI_MAX_PARAM_STRING_LENGTH,
                       "%s", lParam.macParameter );
    CgsUnicode::CgsUtf8* lpResult =
        CgsUnicode::SafelyTerminate( reinterpret_cast<CgsUnicode::CgsUtf8*>( lpOut->macParameter ),
                                     CgsGui::HudMessageParameter::KI_MAX_PARAM_STRING_LENGTH );
    lpOut->meParamType = lParam.meParamType;
    return lpResult;
}

// @0x82674F20 -- parameter count for display string liStringIndex (maiNoOfParams[i]).
s32 GuiHudMessage::GetParamCount(s32 liStringIndex) const
{
    CGS_ASSERT( liStringIndex >= 0 && liStringIndex <= KI_NUMBER_OF_STRINGS - 1,
                "Invalid string index." );
    return maiNoOfParams[liStringIndex];
}

} // namespace BrnGui
