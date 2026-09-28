#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficLightManager.h"
#include "SharedClasses/Traffic/Junctions/BrnTrafficLightCollection.h"   // TrafficLightCollection::GetInstanceIndexForInstanceID
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // gpDebugPrint (the countdown witness)
#include "SharedClasses/Traffic/BrnTrafficHull.h"                         // Hull::muFirstTrafficLight / muLastTrafficLight
                                                                          // (the corona renderers; one ETrafficLightState
                                                                          // home since the BL-1 merge, L3 2026-09-27)
#include <cstddef>                                                        // offsetof (the countdown member pins)
#include <cstdlib>                                                        // std::getenv (the countdown witness)

// BrnTraffic::TrafficLightManager::GetLightState @ 0x8274F9A0
//
// Returns the address of a traffic light's 8-byte runtime state record, addressed by a flat
// instance index. The X360 build:
//   mr   this -> r30, instance -> r31
//   if (instance >= 0x258)                       cmplwi instance, 0x258 ; blt skip
//       BeginAssert/FireAssert("luInstance < KU_MAX_TRAFFIC_LIGHT_INSTANCES", file, 209)/EndAssert
//   slwi r11, instance, 3                         ; instance * 8
//   add  r3, r11, this                            ; this + 8*instance
//   return r3
//
// Reproduced with CGS_ASSERT (the baked d:\p4 path + line 209 are dropped per project policy).
// The pointer arithmetic is expressed through the named array member so there are no raw
// offset casts: &maLightStates[instance] == (u8*)this + 8*instance (8 == sizeof record).

namespace BrnTraffic
{

namespace
{
    // [DIAG] NOT IN THE X360 BINARY. Backing state for Q7Diag_GetLastResolvedLightIndex /
    // ...InstanceID (declared in BrnTrafficLightManager.h -- see the banner there for why this
    // is a file-static + free function rather than a member). Written only by the two
    // GotSmashed/GotRestored bodies below, read only by the [Q7-tlight] one-shot in
    // BrnTrafficEntityModule_wQ7_01.cpp. No console counterpart.
    s32 gQ7DiagLastResolvedLightIndex      = -1;
    u32 gQ7DiagLastResolvedLightInstanceID = 0;
}

// [DIAG] NOT IN THE X360 BINARY -- see BrnTrafficLightManager.h.
s32 Q7Diag_GetLastResolvedLightIndex()      { return gQ7DiagLastResolvedLightIndex; }
u32 Q7Diag_GetLastResolvedLightInstanceID() { return gQ7DiagLastResolvedLightInstanceID; }

TrafficLightState* TrafficLightManager::GetLightState(u32 luInstance)
{
    CGS_ASSERT(luInstance < KU_MAX_TRAFFIC_LIGHT_INSTANCES,
               "luInstance < KU_MAX_TRAFFIC_LIGHT_INSTANCES");

    return &maLightStates[luInstance];
}

// -- TrafficLightGotSmashed @ 0x827519A0 --------------------------------------
//
// A prop-smash event (from TrafficEntityModule::HandlePropModuleRequests) knocks over a
// traffic light: resolve the persistent instance id to its dense index via the baked
// collection's id hash, then flag the matching runtime state record as smashed by setting
// the top bit (0x80) of its flags byte (record+5 == TrafficLightRuntimeState::muFlags).
// A missing id (index == -1) is silently ignored. Assert strings are the X360 rodata
// literals verbatim (baked d:\p4 path + lines 417/425 dropped per project policy).
void TrafficLightManager::TrafficLightGotSmashed(const TrafficLightCollection* lpTrafficLightData,
                                                 u32 luInstanceID)
{
    CGS_ASSERT(lpTrafficLightData, "lpTrafficLightData");

    const s32 liInstanceIndex = lpTrafficLightData->GetInstanceIndexForInstanceID(luInstanceID);
    if (liInstanceIndex != -1)
    {
        TrafficLightState* lpState = GetLightState(static_cast<u32>(liInstanceIndex));
        CGS_ASSERT(lpState, "lpState");

        // Set the smashed bit (0x80) of the record's flags byte at +5 (the attested
        // TrafficLightRuntimeState::muFlags offset). Byte access keeps this slice additive
        // over the placeholder TrafficLightState element type (see BrnTrafficLightManager.h).
        reinterpret_cast<u8*>(lpState)[5] |= 0x80u;

        // [DIAG] NOT IN THE X360 BINARY -- record the resolution for the [Q7-tlight] one-shot.
        gQ7DiagLastResolvedLightIndex      = liInstanceIndex;
        gQ7DiagLastResolvedLightInstanceID = luInstanceID;
    }
}

// -- TrafficLightGotRestored @ 0x82751A40 (40 insns) --------------------------
//
// The paired "this traffic light is back" event: the same id-hash resolution as
// TrafficLightGotSmashed, then CLEAR the smashed bit instead of setting it. Measured
// instruction for instruction (the address has no .ida-exports JSON -- exporter-run gap,
// gotcha 6 -- so it was dumped with headless idat on a private .i64 copy,
// scratchpad/waveQ7/ida_tl/out.json, fn_0x82751A40):
//
//   cmplwi r31,0 / bne              -> assert("lpTrafficLightData")            :0x1BB (443)
//   bl GetInstanceIndexForInstanceID
//   cmpwi  r4,-1 / beq  loc_82751AD8-> a missing id is silently ignored
//   bl GetLightState
//   cmplwi r31,0 / bne              -> assert("lpState")                        :0x1C3 (451)
//   lbz    r11,5(r31)
//   clrlwi r11,r11,25               -> KEEP bits 25..31 == muFlags &= 0x7F      (the ONE
//                                      instruction that differs from the smashed twin,
//                                      which sets 0x80 instead)
//   stb    r11,5(r31)
//
// The baked d:\p4 path + the two line numbers are dropped per project policy; the assert
// STRINGS are the X360 rodata literals verbatim.
void TrafficLightManager::TrafficLightGotRestored(const TrafficLightCollection* lpTrafficLightData,
                                                  u32 luInstanceID)
{
    CGS_ASSERT(lpTrafficLightData, "lpTrafficLightData");

    const s32 liInstanceIndex = lpTrafficLightData->GetInstanceIndexForInstanceID(luInstanceID);
    if (liInstanceIndex != -1)
    {
        TrafficLightState* lpState = GetLightState(static_cast<u32>(liInstanceIndex));
        CGS_ASSERT(lpState, "lpState");

        // Clear the smashed bit (0x80) of the record's flags byte at +5, leaving the low
        // 7 bits (the active-state mask the corona render legs consume) untouched. Byte
        // access mirrors the smashed twin above and keeps this slice additive over the
        // placeholder TrafficLightState element type.
        reinterpret_cast<u8*>(lpState)[5] &= 0x7Fu;

        // [DIAG] NOT IN THE X360 BINARY -- record the resolution for the [Q7-tlight] one-shot.
        gQ7DiagLastResolvedLightIndex      = liInstanceIndex;
        gQ7DiagLastResolvedLightInstanceID = luInstanceID;
    }
}

// ============================================================================
// THE EVENT-COUNTDOWN TRIO (crash parity FX-NETCRASH, 2026-09-25) -- DWARF BrnTrafficLightManager.h
// :117 / :123 / :128 with the members :178..:180.
//
// An event's countdown (ModeManager::CheckCountdownDisplay posts E_ACTION_SET_COUNTDOWN with the
// display value whenever it changes; TrafficEntityModule::HandleExternalRequests arm 47 @0x8274BD98 hands
// it here on every one, offline and online) puts every traffic light in the countdown's state: RED at 3
// and 2, AMBER at 1, GREEN at 0. The GREEN lasts KF_COUNTDOWN_RED_TIME seconds of sim time, run down
// by Update from PostPhysicsUpdate @0x8274EB04, and then the lights go back to their own phases. The
// reader is RenderLightsForHull @0x8275DBF0 (lbz 0x12C0 / lwz 0x12C4 -> 1 << state as the corona mask
// for every light of the hull); it is not reconstructed on this build yet.
// ============================================================================

namespace
{
    // DWARF BrnTrafficLightManager.cpp:30 -- the countdown's GREEN time: flt_82004270 == 3.0f
    // (0x40400000), loaded by SetCountdownValue @0x82751798. By elimination: the file's other
    // constant, KF_AMBER_TIME (:28), is the 2.0f (0x820C0F6C) ChangeLightState @0x82751958 stores
    // when a light goes amber.
    const f32 KF_COUNTDOWN_RED_TIME = 3.0f;

    // DWARF BrnTrafficLightManager.cpp:28 -- how long a light shows AMBER on its way to RED: flt_820C0F6C
    // == 2.0f (0x40000000, x360rd), stored by ChangeLightState @0x82751958.
    const f32 KF_AMBER_TIME = 2.0f;

    // [FLAG PC witness] (crash parity FX-NETCRASH; NOT console code). BRN_TRAFFIC_LIGHT_DIAG,
    // capped, reads only: every countdown value the manager receives, and the end of the countdown.
    CgsDev::Log::DebugPrint* TrafficLightDiagStream()
    {
        static const bool sbEnabled = std::getenv("BRN_TRAFFIC_LIGHT_DIAG") != nullptr;
        return sbEnabled ? CgsDev::Log::gpDebugPrint : nullptr;
    }
    s32 giTrafficLightDiagLinesLeft = 32;
}

// -- Construct @ 0x82751708 (18 insns) -----------------------------------------
//   r11 = this + 4 ; 600 x { stfs 0.0f (flt_82001CC0), -4(r11) ; stb 0, 1(r11) ; stb 2, 0(r11) }
//        == TrafficLightRuntimeState::Construct (DWARF .cpp:46), inlined: no time left in the
//           state (:93 +0), no state bits (:95 +5), E_STATE_GREEN (:54, value 2) (:94 +4)
//   stfs 0.0f, 0x12C8 ; stb 0, 0x12C0 ; stw 3, 0x12C4   (0x82751740..0x82751748)
// Called by TrafficEntityModule::Reset @0x8272D39C.
void TrafficLightManager::Construct()
{
    static_assert(sizeof(TrafficLightRuntimeState) == sizeof(TrafficLightState),
                  "the attested record and the array's placeholder are the same 8 bytes");
    static_assert(offsetof(TrafficLightManager, mbCountdownLights) == 0x12C0, "mbCountdownLights @0x12C0");
    static_assert(offsetof(TrafficLightManager, meCountdownState) == 0x12C4, "meCountdownState @0x12C4");
    static_assert(offsetof(TrafficLightManager, mfCountdownRemainingTime) == 0x12C8,
                  "mfCountdownRemainingTime @0x12C8");

    for (u32 luTrafficLight = 0; luTrafficLight < KU_MAX_TRAFFIC_LIGHT_INSTANCES; ++luTrafficLight)
    {
        // The array still holds the 8-byte placeholder record (see the header); the stores go
        // through the attested record's names.
        TrafficLightRuntimeState& lrState =
            reinterpret_cast<TrafficLightRuntimeState&>(maLightStates[luTrafficLight]);
        lrState.mfTimer = 0.0f;   // mfTimeLeftInState
        lrState.muFlags = 0;      // mxStateBits
        lrState.muState = 2;      // E_STATE_GREEN
    }

    mfCountdownRemainingTime = 0.0f;
    mbCountdownLights        = false;
    meCountdownState         = E_TRAFFICLIGHTSTATE_COUNT;
}

// -- SetCountdownValue @ 0x82751750 (22 insns) ---------------------------------
//   li r11, 1 ; cmpwi r4, 0 ; blt -> ; stb r11, 0x12C0        display >= 0 -> the lights count down
//   cmpwi r4, 2 ; blt -> ; li r11, 0 ; stw r11, 0x12C4 ; blr   display >= 2 -> RED
//   cmpwi r4, 1 ; beq -> stw r11(== 1), 0x12C4 ; blr           display == 1 -> AMBER
//   cmpwi r4, 0 ; bnelr                                         a negative display stops here
//   lwz 0x12C4 ; cmpwi 2 ; beqlr                                already GREEN: keep its time
//   lfs flt_82004270 (3.0f) ; stfs 0x12C8 ; li 2 ; stw 0x12C4  display == 0 -> GREEN for 3 s
void TrafficLightManager::SetCountdownValue(s32 liCountdownDisplay)
{
    if (liCountdownDisplay >= 0)
    {
        mbCountdownLights = true;
    }

    if (liCountdownDisplay >= 2)
    {
        meCountdownState = E_TRAFFICLIGHTSTATE_RED;
    }
    else if (liCountdownDisplay == 1)
    {
        meCountdownState = E_TRAFFICLIGHTSTATE_AMBER;
    }
    else if (liCountdownDisplay == 0 && meCountdownState != E_TRAFFICLIGHTSTATE_GREEN)
    {
        mfCountdownRemainingTime = KF_COUNTDOWN_RED_TIME;
        meCountdownState         = E_TRAFFICLIGHTSTATE_GREEN;
    }

    CgsDev::Log::DebugPrint* const lpDiag = TrafficLightDiagStream();
    if (lpDiag != nullptr && giTrafficLightDiagLinesLeft > 0)
    {
        --giTrafficLightDiagLinesLeft;
        *lpDiag << "[traffic-lights] SetCountdownValue display=" << liCountdownDisplay
                << " -> countdown=" << (mbCountdownLights ? 1 : 0) << " state=" << meCountdownState
                << " remaining=" << mfCountdownRemainingTime << " [FLAG PC witness]\n";
    }
}

// -- ChangeLightState @ 0x827518E0 (47 insns) -- DWARF .cpp:175 (luInstance, lbChangeToRed) ----------
//   cmplwi luInstance, 0x258 ; blt -> else the :391 (0x187) tripwire "luInstance <
//     KU_MAX_TRAFFIC_LIGHT_INSTANCES" (0x82751900..0x8275191C)
//   lbChangeToRed (clrlwi r29 ; beq 0x82751978):
//     GetLightState ; lbz +4 ; beq -> return                  a light already RED stays red
//     GetLightState ; lfs flt_820C0F6C (2.0f) -> stfs +0 ; lbz +5 ; rlwimi r11, 1, 1, 29, 23 ; stb +5 ;
//       stb 1 -> +4                                           AMBER for KF_AMBER_TIME, state bit 1 << 1
//   else:
//     GetLightState ; lbz +5 ; rlwimi r9, 1, 2, 29, 23 ; stb +5 ; stb 2 -> +4
//                                                             GREEN, state bit 1 << 2, the time untouched
// The rlwimi masks (MB 29 > ME 23, wrapped) keep bits 3..7 of the flags byte and replace bits 0..2 with the
// state's bit: the inlined TrafficLightRuntimeState::SetState (DWARF .cpp:179 / :181). The AMBER then runs
// down to RED in TrafficLightRuntimeState::Update @0x827515D8.
void TrafficLightManager::ChangeLightState(u32 luInstance, bool lbChangeToRed)
{
    CGS_ASSERT(luInstance < KU_MAX_TRAFFIC_LIGHT_INSTANCES, "luInstance < KU_MAX_TRAFFIC_LIGHT_INSTANCES");

    // The array still holds the 8-byte placeholder record (see the header); the stores go through the
    // attested record's names, as Construct's do.
    if (lbChangeToRed)
    {
        if (reinterpret_cast<TrafficLightRuntimeState*>(GetLightState(luInstance))->muState
            == static_cast<u8>(E_TRAFFICLIGHTSTATE_RED))
        {
            return;
        }
        TrafficLightRuntimeState* const lpState =
            reinterpret_cast<TrafficLightRuntimeState*>(GetLightState(luInstance));
        lpState->mfTimer = KF_AMBER_TIME;
        lpState->muFlags = static_cast<u8>((lpState->muFlags & 0xF8u) | (1u << E_TRAFFICLIGHTSTATE_AMBER));
        lpState->muState = static_cast<u8>(E_TRAFFICLIGHTSTATE_AMBER);
        return;
    }

    TrafficLightRuntimeState* const lpState = reinterpret_cast<TrafficLightRuntimeState*>(GetLightState(luInstance));
    lpState->muFlags = static_cast<u8>((lpState->muFlags & 0xF8u) | (1u << E_TRAFFICLIGHTSTATE_GREEN));
    lpState->muState = static_cast<u8>(E_TRAFFICLIGHTSTATE_GREEN);
}

// -- Update @ 0x827517A8 (20 insns) --------------------------------------------
//   lbz 0x12C0 ; beqlr                          not counting down: nothing
//   lwz 0x12C4 ; cmpwi 2 ; bnelr                only the GREEN phase runs down
//   lfs 0x12C8 ; fsubs f0, f0, f1 ; stfs 0x12C8  one rounding
//   fcmpu vs flt_82001CC0 (0.0f) ; bgtlr        time left: done (a NaN is not > 0 and falls through)
//   stfs 0.0f, 0x12C8 ; stb 0, 0x12C0 ; stw 3, 0x12C4   the countdown is over
// Called by TrafficEntityModule::PostPhysicsUpdate @0x8274EB04 with mfSimTimeStep (+0x713FC).
void TrafficLightManager::Update(f32 lfTimeDelta)
{
    if (!mbCountdownLights)
    {
        return;
    }
    if (meCountdownState != E_TRAFFICLIGHTSTATE_GREEN)
    {
        return;
    }

    mfCountdownRemainingTime = mfCountdownRemainingTime - lfTimeDelta;
    if (mfCountdownRemainingTime > 0.0f)
    {
        return;
    }

    mfCountdownRemainingTime = 0.0f;
    mbCountdownLights        = false;
    meCountdownState         = E_TRAFFICLIGHTSTATE_COUNT;

    CgsDev::Log::DebugPrint* const lpDiag = TrafficLightDiagStream();
    if (lpDiag != nullptr && giTrafficLightDiagLinesLeft > 0)
    {
        --giTrafficLightDiagLinesLeft;
        *lpDiag << "[traffic-lights] countdown over -> countdown=0 state=" << meCountdownState
                << " (the lights return to their own phases) [FLAG PC witness]\n";
    }
}

// ============================================================================
// THE CORONA RENDERERS (L3 RACEINTRO, 2026-09-27) -- DWARF BrnTrafficLightManager.h :144 / :154, .cpp:290 / :353.
//
// The owner's report: "The races intro doesn't show the traffic light going green". The lights cycled on the PC
// (0802404c) and followed the event countdown (30d481f4), but nothing drew them: a traffic light's lit lamps are its
// coronas, and these two renderers -- the only callers of TrafficLightCollection::RenderCoronasForInstance
// @0x827571B8 -- had no body. TrafficEntityModule::RenderTrafficLightCoronas @0x8271EC80 calls them from the
// traffic dispatch pass.
// ============================================================================

namespace
{
    // [FLAG PC witness] (L3 RACEINTRO; NOT console code). BRN_TRAFFIC_LIGHT_CORONA_DIAG=<instance>, capped, reads
    // only: the active-state mask a renderer hands RenderCoronasForInstance for that ONE light instance, each time
    // it changes -- e.g. a start light through an event countdown (1 RED, 2 AMBER, 4 GREEN; 0 lights nothing).
    s32 TrafficLightCoronaDiagInstance()
    {
        static const char* const spcValue = std::getenv("BRN_TRAFFIC_LIGHT_CORONA_DIAG");
        static const s32 siInstance = (spcValue != nullptr) ? std::atoi(spcValue) : -1;
        return siInstance;
    }
    u32 guTrafficLightCoronaDiagLastMask  = 0xFFFFFFFFu;
    s32 giTrafficLightCoronaDiagLinesLeft = 48;

    void ReportTrafficLightCorona(u32 luInstance, bool lbAllLightsRenderer, bool lbCountdown, bool lbSmashed,
                                  u32 luActiveStates)
    {
        // The change key: the mask, whether it is the countdown's, and which renderer drew it.
        const u32 luMask = lbSmashed ? 0x80000000u
                                     : (luActiveStates | (lbCountdown ? 0x40000000u : 0u)
                                        | (lbAllLightsRenderer ? 0x20000000u : 0u));
        if (static_cast<s32>(luInstance) != TrafficLightCoronaDiagInstance() || luMask == guTrafficLightCoronaDiagLastMask
            || giTrafficLightCoronaDiagLinesLeft <= 0 || CgsDev::Log::gpDebugPrint == nullptr)
        {
            return;
        }
        guTrafficLightCoronaDiagLastMask = luMask;
        --giTrafficLightCoronaDiagLinesLeft;
        *CgsDev::Log::gpDebugPrint << "[tl-corona] instance " << luInstance << " "
                                   << (lbAllLightsRenderer ? "RenderAllLightsToBeInStateForHull" : "RenderLightsForHull")
                                   << (lbCountdown ? " countdown" : "");
        if (lbSmashed)
        {
            *CgsDev::Log::gpDebugPrint << " smashed: not drawn";
        }
        else
        {
            *CgsDev::Log::gpDebugPrint << " mask " << luActiveStates;
        }
        *CgsDev::Log::gpDebugPrint << " [FLAG PC witness]\n";
    }
}

// -- TrafficLightRuntimeState::IsSmashed / GetLightState -- DWARF .cpp:100 / :118, inlined by both renderers ---------
//   IsSmashed:      lbz r11, 5(state) ; rlwinm r11, r11, 0, 0, 24 (& 0xFFFFFF80) ; cmplwi r11, 0 ; bne -> skip
//   GetLightState:  clrlwi r5, r11, 25 (& 0x7F), r11 the same flags byte (0x8275DDEC / 0x8275DE08)
// The flags byte is TrafficLightRuntimeState::muFlags (+5): SetState (ChangeLightState's rlwimi) keeps its low three
// bits at 1 << state, and TrafficLightGotSmashed / GotRestored set / clear the top bit.
bool TrafficLightRuntimeState::IsSmashed() const
{
    return (muFlags & 0x80u) != 0;
}

u8 TrafficLightRuntimeState::GetLightState() const
{
    return static_cast<u8>(muFlags & 0x7Fu);
}

// -- RenderLightsForHull @ 0x8275DBF0 (a hole in the export set; read with tools/re/ppcdis.py) -- DWARF .cpp:290 ----
//   r4 lpHull (r25) ; r5 lpTrafficLightData (r24) ; r6 lpCoronaSubmissionInterface (r22) ;
//   v1 / v2 / v3 = lCameraPosition / lCameraDirection / lfCullDistSq (held in v127 / v126 / v125)
//   cmplwi lpHull ; bne              else the .cpp:296 (0x128) tripwire "lpHull"             0x8275DC34..0x8275DC58
//   cmplwi lpTrafficLightData ; bne  else the .cpp:297 (0x129) tripwire "lpTrafficLightData" 0x8275DC5C..0x8275DC7C
//   lbz 0x12C0 (mbCountdownLights) ; beq -> the lights' own states                          0x8275DC80..0x8275DC88
//   COUNTDOWN: lwz 0x12C4 ; cmpwi 3 ; bne  else the .cpp:301 (0x12D) tripwire
//              "meCountdownState != E_TRAFFICLIGHTSTATE_COUNT"                               0x8275DC8C..0x8275DCB0
//              luActiveStates = 1 << meCountdownState (lwz 0x12C4 again ; slw)               0x8275DCB4..0x8275DCC8
//              luInstance from lhz +0xA (muFirstTrafficLight) while < lhz +0xC (muLastTrafficLight, re-read):
//                GetLightState(luInstance), the const inline (h:217, the 0xD9 tripwire)     0x8275DCF0..0x8275DD0C
//                cmplwi lpLightState ; bne  else the .cpp:309 (0x135) tripwire "lpLightState"
//                IsSmashed() ; bne -> next light                                           0x8275DD30..0x8275DD3C
//                RenderCoronasForInstance(luInstance, luActiveStates, lpCoronaSubmissionInterface, v1, v2, v3)
//                                                                                            (bl 0x827571B8 at 0x8275DD5C)
//   OWN STATES: the same walk with the .cpp:327 (0x147) "lpLightState" tripwire; the mask is each light's own
//              GetLightState() (clrlwi r5, r11, 25 at 0x8275DE08; bl 0x827571B8 at 0x8275DE18).
// Every countdown light shows the countdown's ONE lamp: RED at 3 and 2, AMBER at 1, GREEN at 0 for
// KF_COUNTDOWN_RED_TIME (SetCountdownValue / Update above).
void TrafficLightManager::RenderLightsForHull(const Hull* lpHull, const TrafficLightCollection* lpTrafficLightData,
                                              BrnCoronaManager::BrnSubmissionInterface* lpCoronaSubmissionInterface,
                                              Vector3 lCameraPosition, Vector3 lCameraDirection,
                                              VecFloat lfCullDistSq) const
{
    CGS_ASSERT(lpHull, "lpHull");                                                        // .cpp:296
    CGS_ASSERT(lpTrafficLightData, "lpTrafficLightData");                                // .cpp:297

    if (mbCountdownLights)
    {
        CGS_ASSERT(meCountdownState != E_TRAFFICLIGHTSTATE_COUNT,
                   "meCountdownState != E_TRAFFICLIGHTSTATE_COUNT");                      // .cpp:301
        const u32 luActiveStates = 1u << meCountdownState;

        for (u32 luInstance = lpHull->muFirstTrafficLight; luInstance < lpHull->muLastTrafficLight; ++luInstance)
        {
            // The array still holds the 8-byte placeholder record (see the header); the reads go through the
            // attested record, as Construct / ChangeLightState do.
            const TrafficLightRuntimeState* lpLightState =
                reinterpret_cast<const TrafficLightRuntimeState*>(GetLightState(luInstance));
            CGS_ASSERT(lpLightState, "lpLightState");                                      // .cpp:309

            ReportTrafficLightCorona(luInstance, false, true, lpLightState->IsSmashed(),
                                     luActiveStates);                                     // [DIAG] NOT X360
            if (!lpLightState->IsSmashed())
            {
                lpTrafficLightData->RenderCoronasForInstance(luInstance, luActiveStates, lpCoronaSubmissionInterface,
                                                             lCameraPosition, lCameraDirection, lfCullDistSq);
            }
        }
    }
    else
    {
        for (u32 luInstance = lpHull->muFirstTrafficLight; luInstance < lpHull->muLastTrafficLight; ++luInstance)
        {
            const TrafficLightRuntimeState* lpLightState =
                reinterpret_cast<const TrafficLightRuntimeState*>(GetLightState(luInstance));
            CGS_ASSERT(lpLightState, "lpLightState");                                      // .cpp:327

            ReportTrafficLightCorona(luInstance, false, false, lpLightState->IsSmashed(),
                                     lpLightState->GetLightState());                      // [DIAG] NOT X360
            if (!lpLightState->IsSmashed())
            {
                lpTrafficLightData->RenderCoronasForInstance(luInstance, lpLightState->GetLightState(),
                                                             lpCoronaSubmissionInterface, lCameraPosition,
                                                             lCameraDirection, lfCullDistSq);
            }
        }
    }
}

// -- RenderAllLightsToBeInStateForHull @ 0x8275DE50 -- DWARF .cpp:353 ---------------------------------------------
//   r4 lpHull (r26) ; r5 lpTrafficLightData (r24) ; r6 luActiveStates (r23) ; r7 lpCoronaSubmissionInterface (r22) ;
//   v1 / v2 / v3 as above
//   cmplwi lpHull ; bne              else the .cpp:359 (0x167) tripwire "lpHull"             0x8275DE98..0x8275DEBC
//   cmplwi lpTrafficLightData ; bne  else the .cpp:360 (0x168) tripwire "lpTrafficLightData" 0x8275DEC0..0x8275DEE0
//   luInstance from lhz +0xA while < lhz +0xC (re-read every pass):
//     GetLightState(luInstance), the const inline (h:217, the 0xD9 tripwire)                0x8275DF14..0x8275DF30
//     cmplwi lpLightState ; bne  else the .cpp:366 (0x16E) tripwire "lpLightState"          0x8275DF34..0x8275DF50
//     IsSmashed() ; bne -> next light                                                       0x8275DF54..0x8275DF60
//     RenderCoronasForInstance(luInstance, luActiveStates, lpCoronaSubmissionInterface, v1, v2, v3)
//                                                                                            (bl 0x827571B8 at 0x8275DF80)
// RenderTrafficLightCoronas passes 1 << E_TRAFFICLIGHTSTATE_GREEN (li r6, 4 at 0x8271F244): the lights of every
// hull the player's simulation does not run show GREEN.
void TrafficLightManager::RenderAllLightsToBeInStateForHull(const Hull* lpHull,
                                                            const TrafficLightCollection* lpTrafficLightData,
                                                            u32 luActiveStates,
                                                            BrnCoronaManager::BrnSubmissionInterface*
                                                                lpCoronaSubmissionInterface,
                                                            Vector3 lCameraPosition, Vector3 lCameraDirection,
                                                            VecFloat lfCullDistSq) const
{
    CGS_ASSERT(lpHull, "lpHull");                                                        // .cpp:359
    CGS_ASSERT(lpTrafficLightData, "lpTrafficLightData");                                // .cpp:360

    for (u32 luInstance = lpHull->muFirstTrafficLight; luInstance < lpHull->muLastTrafficLight; ++luInstance)
    {
        const TrafficLightRuntimeState* lpLightState =
            reinterpret_cast<const TrafficLightRuntimeState*>(GetLightState(luInstance));
        CGS_ASSERT(lpLightState, "lpLightState");                                          // .cpp:366

        ReportTrafficLightCorona(luInstance, true, false, lpLightState->IsSmashed(),
                                 luActiveStates);                                         // [DIAG] NOT X360
        if (!lpLightState->IsSmashed())
        {
            lpTrafficLightData->RenderCoronasForInstance(luInstance, luActiveStates, lpCoronaSubmissionInterface,
                                                         lCameraPosition, lCameraDirection, lfCullDistSq);
        }
    }
}

}
