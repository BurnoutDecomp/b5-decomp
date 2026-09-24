// ============================================================================
// BrnGameState::RumbleManager -- controller rumble / force-feedback manager.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (unity TU "gamesource\unity\../GameState/
// RumbleManager/BrnRumbleManager.cpp"), names/shape from the DecFIGS DWARF, cross-checked
// against the PS3 twins in .ida-exports/DecFIGS_Burnout_Internal_PS3.ELF:
//
//   Construct                @ 0x82378A70   (PS3 0x241EFC)
//   Prepare                  @ 0x823648D0   (PS3 0x246F88)
//   Update                   @ 0x82386A98   (PS3 0x26FE98)
//   UpdatePauseState         @ 0x8236E728   (PS3 0x294520)
//   OnVehicleAggressorImpact @ 0x823795C8   (PS3 0x240050)
//   OnVehicleVictimImpact      ICF-folded onto 0x823795C8 (PS3 0x2401D4, a thunk)
//   UpdateImpacts            @ 0x82379370   (PS3 0x248318)
//   PlayJolt                 @ 0x8236E7F8   (PS3 0x23FEB8)
//
// ⭐ [FX-RUMBLE 2026-09-22, crash-parity G10-D1/D2/D3/D5/D7] Construct / Update / UpdateImpacts
// / OnVehicle*Impact / PlayJolt were "BLOCKED" here on the claim that their 48-byte JoltEffect
// templates "are not recovered". They are ordinary INITIALISED data, read straight out of the
// image with tools/re/x360rd.py, and tools/re/findinit.py finds no CRT writer for any of them
// (only the reader sites 0x82379500 / 0x823795E8 / 0x82386BA4 / 0x82386B98), so the image
// bytes ARE the source initialisers. Every value below is quoted with its address.
//
//   BridgeRumbleToInput      @ 0x82364978   (PS3 0x270044)  -- FX-RUMBLE3 2026-09-24, G10-D4
//
// ⭐ [FX-RUMBLE3 2026-09-24, G10-D4] THE QUEUES ARE DRAINED. Until BridgeRumbleToInput landed nothing
// emptied the four event queues: PlayJolt's AddEventSafe (the bounds-GATED append) returned false
// once the jolt queue held four events, and no request ever reached the pad. The drain is now called
// once per update step from BrnGameModule::DoUpdate_InputPreWorld (through the GameStateModule forward),
// and the input module's ProcessRumbleRequests plays what it hands over.
//
//   UpdateSurfaceRumble      @ 0x82378AE0   (PS3 0x26F01C)  -- FX-RUMBLE3 2026-09-24, G10-D6
//   PlayRumble / ChangeRumbleVolume / StopRumble -- DWARF-only (PS3 0x256B88 / 0x25687C / 0x256E4C),
//                                                   inlined into UpdateSurfaceRumble on the X360
// ============================================================================

#include "GameSource/GameState/RumbleManager/BrnRumbleManager.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h" // BrnPhysics::Vehicle::EVehicleDrivenWheel
#include "GameSource/World/BrnEntityTypes.h"                                           // BrnWorld::E_ENTITYTYPE_WORLD (UpdateImpacts' world-contact scale)
#include "GameShared/GameClasses/Core/CgsAssert.h"                                     // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                             // [DIAG] CgsDev::Log::gpDebugPrint (PlayJolt witness)
#include "rw/math/vpu/vector3_operation.h"                                             // rw::math::vpu::Magnitude (UpdateImpacts)
#include "rw/math/fpu/scalar_operation.h"                                              // rw::math::fpu::Clamp / Max / Abs (UpdateImpacts, UpdateSurfaceRumble)
#include "GameSource/AttribSys/Generated/classes/surface.h"                             // Attrib::Gen::surface (UpdateSurfaceRumble)
#include "GameSource/AttribSys/Generated/classes/rumblesurface.h"                       // Attrib::Gen::rumblesurface (UpdateSurfaceRumble)
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h"      // Attrib::StringToKey (the surface-list collection key)

#include <stdlib.h>                                                                    // [DIAG] getenv (BRN_RUMBLE_DIAG)

namespace BrnGameState
{
    // ------------------------------------------------------------------------
    // File-scope constants (DWARF BrnRumbleManager.cpp:30..53). Only the ones a console body
    // actually loads are defined; KF_CRASH_RUMBLE_TIME / KI_CRASH_RUMBLE_AMOUNT /
    // KF_CHECK_RUMBLE_TIME / KU_CHECK_*_RUMBLE_AMOUNT / KF_MIN_RUMBLE_SEPARATION_TIME /
    // KF_MIN_RUMBLE_IMPACT_STRENGTH are read by no X360 function this TU reconstructs (and
    // UpdateImpacts has NO minimum-strength or separation-time gate -- see its body).
    // ------------------------------------------------------------------------

    // DWARF :48; the PS3 names the load `_ZN12BrnGameState28KF_RUMBLE_IMPACT_WORLD_SCALEE` (0x24866C).
    // X360 .rdata 0x8202ADF8 = 0x3DCCCCCD (the `lvlx` + `vspltw` at 0x823794AC).
    const f32 KF_RUMBLE_IMPACT_WORLD_SCALE = 0.1f;
    // DWARF :45 / :46. X360 .rdata 0x8202ADF0 = 0x37A7C5AC (2e-5, `lfs f13, 0(r27)` 0x82379518,
    // scales the LOW-frequency envelope) and 0x8202ADF4 = 0x3A83126F (0.001, `lfs f0, 4(r27)`
    // 0x82379510, scales the HIGH-frequency envelope). Declaration order and address order agree.
    const f32 KF_RUMBLE_IMPACT_SCALE_LOW  = 0.00002f;
    const f32 KF_RUMBLE_IMPACT_SCALE_HIGH = 0.001f;
    // DWARF :50..:53, the `li r4, <prio>` immediates at every PlayJolt / AddEventSafe site.
    const s32 KN_RUMBLE_IMPACT_PRIORITY         = 1000;   // 0x3E8 @0x823795A4
    const s32 KN_RUMBLE_VEHICLE_IMPACT_PRIORITY = 1001;   // 0x3E9 @0x823796DC
    const s32 KN_LANDING_RUMBLE_PRIORITY        = 1002;   // 0x3EA @0x82386B94
    const s32 KN_CRASH_RUMBLE_PRIORITY          = 1010;   // 0x3F2 @0x82386B24

    // The entity word's owner (entity-type) byte is bits [24..31]. UpdateImpacts reads it as a
    // BIG-ENDIAN byte load of the contact's mEntityIdB (`lbz r11, 4(r3)` @0x82379460 -- +4 is the
    // word's most significant byte on the X360); on this little-endian host the same byte is the
    // shift, never a byte-offset read (which would land on the LEAST significant byte).
    static const u32 KU_ENTITY_ID_OWNER_SHIFT = 24;

    // The world's surface-list COLLECTION key UpdateSurfaceRumble re-binds mSurfaceList to. The console
    // loads it from qword_82FADEC0 (`ld r4` @0x82378B0C), whose one writer is a CRT dynamic initialiser,
    // 0x82C4D750: `qword_82FADEC0 = Attrib::StringToKey("340654")` (the string at 0x82013F9C) -- the
    // same literal EffectsModule::PostWorldPreparePrepare's own copy (qword_82FAB7A8) is built from; the
    // record is SURFACELIST.BIN's (mKey = StringToKey("340654"), class 42C25F4985B5C4F4). Kept as the
    // console keeps it: one namespace-scope constant, hashed once at start-up.
    static const u64 KU_WORLD_SURFACELIST_COLLECTION_KEY = Attrib::StringToKey("340654");

    // Attrib::DefaultDataArea(0x18) -- the zeroed RefSpec UpdateSurfaceRumble falls back to when the
    // surface list has no element at an index (`li r3, 0x18` @0x82378B60 / 0x82378CF4 / 0x82378EC8 /
    // 0x82379174). sizeof(Attrib::RefSpec) on both builds.
    static const u32 KU_SURFACE_REFSPEC_BYTES = 0x18;

    // A wheel's surface id is bits 4..9 of the LOW half of its road contact's collision tag: `lhz -2(r27)`
    // (wheel +0x26, the low half of the big-endian tag word at +0x24) ; `srwi 4` ; `clrlwi 26`
    // (0x82378CCC..0x82378CDC) -- EffectsModule's (tag >> 4) & 0x3F.
    static const u32 KU_SURFACE_ID_SHIFT = 4;
    static const u32 KU_SURFACE_ID_MASK  = 0x3F;

    // unk_82029BA4 (x360rd 0x34000000 == FLT_EPSILON), the splat the surface-list sanity check's
    // `vcmpgtfp.` compares each |lane| of surface 1's DebugRenderColor against (0x82378B98..0x82378BA8).
    static const f32 KF_SURFACE_COLOUR_EPSILON = 1.1920929e-07f;

    // flt_82001C98 (1.0f): the floor of UpdateSurfaceRumble's speed band (`fsel f11, f9, f11, f30`
    // with f30 = 1.0, @0x82378F9C / 0x82379308) -- Max(Max - Min, 1.0).
    static const f32 KF_MIN_SPEED_BAND = 1.0f;

    // ------------------------------------------------------------------------
    // @ 0x82378A70 -- BrnGameState::RumbleManager::Construct()
    //
    //     stb 0 -> +0x399 (mbRumblePaused)        stb 1 -> +0x398 (mbRumbleEnabled)
    //     stb 0 -> +0x39A..+0x39E                 stb 1 -> +0x39F (mbWheelForceFeedback)
    //     bl EventQueue<PlayJoltEffectEvent,4>::Construct          (+0x010, 0x823736F0)
    //     bl EventQueue<PlayRumbleEffectEvent,4>::Construct        (+0x10C, 0x82373760)
    //     bl EventQueue<ChangeVolumeRumbleEffectEvent,4>::Construct (+0x228, 0x823737D0)
    //     bl EventQueue<StopRumbleEffectEvent,4>::Construct        (+0x334, 0x82373840)
    // Each queue Construct binds mpEvents to the inline storage, max length 4, length 0 (verified
    // in 0x823736F0) -- without it mpEvents stays NULL and the first AddEventSafe asserts.
    // mSurfaceList and the per-wheel arrays are NOT touched here (Prepare seeds the arrays).
    // ------------------------------------------------------------------------
    void RumbleManager::Construct()
    {
        mbRumblePaused                = false;
        mbRumbleEnabled               = true;
        mbGameWasPaused               = false;
        mbInPictureParadise           = false;
        mbPlayerIsCrashing            = false;
        mbPlayerHasJustCheckedTraffic = false;
        mbPlayerIsInAir               = false;
        mbWheelForceFeedback          = true;

        mPlayJoltEffectEventQueue.Construct();
        mPlayRumbleEffectEventQueue.Construct();
        mChangeVolumeRumbleEffectEventQueue.Construct();
        mStopRumbleEffectEventQueue.Construct();
    }

    // ------------------------------------------------------------------------
    // @ 0x823648D0 -- BrnGameState::RumbleManager::Prepare()
    //
    // Reset the per-driven-wheel surface/rumble tracking arrays to their empty state,
    // then drop any queued rumble events. Returns true.
    // ------------------------------------------------------------------------
    bool RumbleManager::Prepare()
    {
        // The X360 walks the driven-wheel enum front-left..rear-right; the inlined
        // BrnPhysics::Vehicle::operator++(EVehicleDrivenWheel&) (BrnSimpleVehiclePhysics.h:332)
        // asserts the post-increment index stays within eNumDrivenWheels. Reproduced inline
        // here because that operator is not yet homed -- it never trips for the fixed 4-wheel
        // walk, but the asm fires it, so the assert is preserved.
        BrnPhysics::Vehicle::EVehicleDrivenWheel leWheelIndex = BrnPhysics::Vehicle::eFrontLeftWheel;
        do
        {
            mau8SurfaceID[leWheelIndex]          = 0xFF;
            mau8NumWheelsOnSurface[leWheelIndex] = 0;
            mafRumbleVolume[leWheelIndex]        = 0.0f;
            manRumbleID[leWheelIndex]            = -1;

            leWheelIndex = static_cast<BrnPhysics::Vehicle::EVehicleDrivenWheel>(leWheelIndex + 1);
            CGS_ASSERT(leWheelIndex <= BrnPhysics::Vehicle::eNumDrivenWheels,
                       "leEnumIndex <= eNumDrivenWheels");
        }
        while (leWheelIndex < BrnPhysics::Vehicle::eNumDrivenWheels);

        mPlayJoltEffectEventQueue.Clear();
        mPlayRumbleEffectEventQueue.Clear();
        mChangeVolumeRumbleEffectEventQueue.Clear();
        mStopRumbleEffectEventQueue.Clear();
        return true;
    }

    // ------------------------------------------------------------------------
    // @ 0x82386A98 -- BrnGameState::RumbleManager::Update()
    //
    // The console body, statement for statement (r31 == this, r30 == the interface, r29 == the
    // player index argument, r28 == the contact spy):
    //     UpdateSurfaceRumble(this, itf, idx)                      0x82386AB8   (FX-RUMBLE3 G10-D6)
    //     UpdateImpacts(this, itf, idx, spy)                       0x82386ACC
    //     crash latch    itf->IsPlayerCarCrashing()                0x82386AD0..0x82386B38
    //                    (lwz 0x2858 == the interface's OWN player index, NOT the r6 argument;
    //                     lbz 0x77A == maRaceCarStates(+0x330)[idx].mbCrashing(+0x44A))
    //     air latch      itf->TimePlayerInAir() > 0.1f             0x82386B3C..0x82386B60
    //                    (lfs 0x734 == states[idx].mfTimeInAir(+0x404); flt_82004014 == 0.1)
    //     landing jolt   kLandedJoltEffect low sustain = 0.3 * TimePlayerInAir()
    //                    (stfs into 0x82CDBE4C -- IN PLACE in the function static), 0x82386B70..
    //     stb 0 -> +0x39D (mbPlayerHasJustCheckedTraffic) on BOTH exits (0x82386BB8 / 0x82386BC8)
    // r5 (lpCrashEventQueue) is overwritten by `mr r5, r29` before the first call and f1
    // (lfGameTimeStep) is never read: both are dead in the console body too.
    //
    // (FX-RUMBLE3 2026-09-24: the first statement -- UpdateSurfaceRumble, the per-wheel road-surface
    // rumble -- is back; it was a [FLAG G10-D6] hole while its body's types were missing.)
    // ------------------------------------------------------------------------
    void RumbleManager::Update(
            BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
            CgsModule::EventQueue<BrnPhysics::Vehicle::RaceCarCrashEvent, 8>*     /*lpCrashEventQueue*/,
            EActiveRaceCarIndex                                                    lePlayerCarIndex,
            BrnPhysics::ContactSpy::ContactSpyInterface*                           lpContactSpyInterface,
            f32                                                                    /*lfGameTimeStep*/)
    {
        UpdateSurfaceRumble(lpActiveRaceCarInterface, lePlayerCarIndex);                    // 0x82386AB8
        UpdateImpacts(lpActiveRaceCarInterface, lePlayerCarIndex, lpContactSpyInterface);   // 0x82386ACC

        // DWARF :158 `const JoltEffect kCrashJoltEffect` -- a const function static in .rdata
        // 0x82030F0C: {0, 0, 0.5, 1, 1, 1 | 0, 0, 0.5, 1, 1, 1} (x360rd; no CRT writer).
        static const CgsInput::InputIO::JoltEffect kCrashJoltEffect =
        {
            { 0.0f, 0.0f, 0.5f, 1.0f, 1.0f, 1.0f },   // mLowFreqJoltData   (0x82030F0C..0x82030F20)
            { 0.0f, 0.0f, 0.5f, 1.0f, 1.0f, 1.0f },   // mHighFreqJoltData  (0x82030F24..0x82030F38)
        };

        if (lpActiveRaceCarInterface->IsPlayerCarCrashing())
        {
            if (!mbPlayerIsCrashing)
            {
                mbPlayerIsCrashing = true;                                  // stb 1 -> +0x39C @0x82386B14
                if (!mbRumblePaused)                                       // lbz +0x399 @0x82386B10
                {
                    PlayJolt(KN_CRASH_RUMBLE_PRIORITY, kCrashJoltEffect);   // 0x82386B30
                }
            }
        }
        else
        {
            mbPlayerIsCrashing = false;                                     // stb 0 -> +0x39C @0x82386B38
        }

        if (lpActiveRaceCarInterface->TimePlayerInAir() > 0.1f)             // flt_82004014, bgt @0x82386B60
        {
            mbPlayerIsInAir = true;                                         // stb 1 -> +0x39E @0x82386BC4
        }
        else if (mbPlayerIsInAir)                                          // lbz +0x39E @0x82386B64
        {
            // DWARF :193..:196 -- both are FUNCTION STATICS in writable .data (the PS3 names them
            // `Update(...)::krRumbleTimeFactor` / `::kLandedJoltEffect`), and the console writes
            // the sustain lane IN PLACE, not into a copy:
            //     krRumbleTimeFactor  .data 0x82CDBE40 = 0x3E99999A (0.3)
            //     kLandedJoltEffect   .data 0x82CDBE44 = {0, 0.2, <runtime>, 0.3, 0.6, 0.3 |
            //                                             0, 0, 0.1, 0.1, 1.0, 0.7}
            // The sustain lane's image value is 0.0; it is overwritten before every use.
            // ⚠️ THE AIR TIME IS READ ON THE LANDING FRAME (the same TimePlayerInAir() that just
            // failed the > 0.1 test), exactly as the console does -- NOT a cached peak.
            static f32 krRumbleTimeFactor = 0.3f;
            static CgsInput::InputIO::JoltEffect kLandedJoltEffect =
            {
                { 0.0f, 0.2f, 0.0f, 0.3f, 0.6f, 0.3f },   // mLowFreqJoltData   (0x82CDBE44..0x82CDBE58)
                { 0.0f, 0.0f, 0.1f, 0.1f, 1.0f, 0.7f },   // mHighFreqJoltData  (0x82CDBE5C..0x82CDBE70)
            };

            const f32 lrRumbleTime = krRumbleTimeFactor * lpActiveRaceCarInterface->TimePlayerInAir(); // fmuls @0x82386BA8
            kLandedJoltEffect.mLowFreqJoltData.mfSustainTime = lrRumbleTime;                          // stfs 0x82CDBE4C @0x82386BAC
            PlayJolt(KN_LANDING_RUMBLE_PRIORITY, kLandedJoltEffect);                                   // 0x82386BB0
            mbPlayerIsInAir = false;                                                                   // stb 0 -> +0x39E @0x82386BB4
        }

        mbPlayerHasJustCheckedTraffic = false;                              // stb 0 -> +0x39D (both exits)
    }

    // ------------------------------------------------------------------------
    // @ 0x8236E728 -- BrnGameState::RumbleManager::UpdatePauseState()
    //
    // Latch the game-paused transition. On the rising edge (not-was-paused -> paused) the
    // rumble is paused; on the falling edge (was-paused -> unpaused) it is resumed; both edges
    // return immediately. When the paused/requested state is unchanged, the game-action queue
    // is scanned: a pause action (type 37) forces mbRumblePaused true, while a resume action
    // (type 39) mirrors the current request into mbRumblePaused.
    // ------------------------------------------------------------------------
    void RumbleManager::UpdatePauseState(bool lbPaused,
                                         CgsModule::VariableEventQueue<13312, 16>* lpGameActionQueue)
    {
        if (!mbGameWasPaused)
        {
            if (lbPaused)
            {
                mbGameWasPaused = true;
                mbRumblePaused  = true;
                return;
            }
        }
        else if (!lbPaused)
        {
            mbGameWasPaused = false;
            mbRumblePaused  = false;
            return;
        }

        mbGameWasPaused = lbPaused;

        // Game-action event-type discriminants (the queue returns the action id as the type):
        //   37 -> the pause action  (breaks the scan and forces the rumble paused)
        //   39 -> the resume action (mirrors the current pause request into mbRumblePaused)
        const CgsModule::Event* lpEvent = nullptr;
        s32 liSize = 0;
        s32 liType = lpGameActionQueue->GetFirstEvent(&lpEvent, &liSize);
        if (lpEvent != nullptr)
        {
            while (liType != 37)
            {
                if (liType == 39)
                    mbRumblePaused = lbPaused;

                liType = lpGameActionQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
                if (lpEvent == nullptr)
                    return;
            }
            mbRumblePaused = true;
        }
    }

    // ------------------------------------------------------------------------
    // @ 0x82378AE0 -- BrnGameState::RumbleManager::UpdateSurfaceRumble()  (DWARF h:79, cpp :362..)
    //
    // The road-surface rumble, in the console's order (r21 == this, r16 == the player car's state):
    //   0x82378B00  lePlayerCarIndex == -1 -> return
    //   0x82378B08..0x82378B30  mSurfaceList re-bound: FindCollectionWithDefault(surfacelist,
    //               StringToKey("340654")) + Instance::Change -- surfacelist::ChangeWithDefault inlined
    //   0x82378B34..0x82378C30  :374..:376 the list's sanity check: surface 1 (Surfaces(1) or the zeroed
    //               RefSpec) through RefSpec::GetCollection + the Collection* surface ctor (0x8227FAB0);
    //               its DebugRenderColor all-lanes-IsZero (|lane| > FLT_EPSILON in none) asserts
    //               "Surface list appears to be corrupt"
    //   0x82378C3C  lpRaceCarState = GetRaceCarState(lePlayerCarIndex)
    //   0x82378C60..0x82378C98  :382 the four surface slots cleared: id 0xFF, count 0
    //   0x82378CB4..0x82378DF8  :391 per wheel with mbLineTestIsValid (+0x2B) && mbIsOnGround (+0x28):
    //               surface id = (tag low half >> 4) & 0x3F; surface (RefSpec ctor sub_8227FB58) ->
    //               rumblesurface(surface +0x28, 0x82364828); RumblePriority >= 0 -> the id takes the
    //               first free slot or bumps its own slot's wheel count (:407), else "leSurfaceListIndex
    //               < eNumDrivenWheels" (:433)
    //   0x82378E14..0x8237901C  :441 per live rumble id: found among the counted slots, and not paused
    //               (+0x399), not in Picture Paradise (+0x39B), the player driving (meDriverType +0x458
    //               == PLAYER) -> ChangeRumbleVolume(id, volume, envelope) [asserting AddEvent +0x228];
    //               else StopRumble(id) [+0x334] and the id -> -1
    //   0x82379020..0x82379358  the same three gates, then :505 per counted slot with no rumble yet:
    //               the first free id (-1) slot (:541 "leRumbleListIndex < eNumDrivenWheels"), id = the
    //               surface, PlayRumble(id, volume, RumblePriority, envelope) [+0x10C]
    // volume = (Clamp(|mfSpeedMPH| (+0x3CC), Min, Max) - Min) / Max(Max - Min, 1.0) -- the fsel pairs at
    // 0x82378EF4..0x82378FA4 / 0x8237929C..0x82379310, each rw::math::fpu Min/Max/Clamp exactly (DWARF
    // locals lfSpeed :469/:548, lfSpeedDifference :473/:552, lfVolume :474/:553). The envelope is the
    // rumblesurface's Left (low) / Right (high) motor ADSR, lane for lane (see rumblesurface.h).
    // Every driven-wheel walk carries the inlined BrnPhysics::Vehicle::operator++ (BrnSimpleVehiclePhysics.h
    // :332, "leEnumIndex <= eNumDrivenWheels") -- spelled out as in Prepare, the operator being unhomed.
    // ------------------------------------------------------------------------
    void RumbleManager::UpdateSurfaceRumble(
            BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
            EActiveRaceCarIndex                                                    lePlayerCarIndex)
    {
        using BrnPhysics::Vehicle::EVehicleDrivenWheel;
        using BrnPhysics::Vehicle::eFrontLeftWheel;
        using BrnPhysics::Vehicle::eNumDrivenWheels;

        if (lePlayerCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID)
        {
            return;
        }

        mSurfaceList.ChangeWithDefault(KU_WORLD_SURFACELIST_COLLECTION_KEY);

        {
            void* lpSampleSurfaceRef = mSurfaceList.Surfaces(1);
            if (lpSampleSurfaceRef == nullptr)
            {
                lpSampleSurfaceRef = Attrib::DefaultDataArea(KU_SURFACE_REFSPEC_BYTES);
            }
            Attrib::Gen::surface lSampleSurface(
                const_cast<Attrib::Collection*>(static_cast<Attrib::RefSpec*>(lpSampleSurfaceRef)->GetCollection()), 0);
            const f32 (&lSampleColour)[4] = lSampleSurface.DebugRenderColor();
            const bool lbSampleColourIsZero = !(rw::math::fpu::Abs(lSampleColour[0]) > KF_SURFACE_COLOUR_EPSILON)
                                           && !(rw::math::fpu::Abs(lSampleColour[1]) > KF_SURFACE_COLOUR_EPSILON)
                                           && !(rw::math::fpu::Abs(lSampleColour[2]) > KF_SURFACE_COLOUR_EPSILON)
                                           && !(rw::math::fpu::Abs(lSampleColour[3]) > KF_SURFACE_COLOUR_EPSILON);
            CGS_ASSERT(!lbSampleColourIsZero, "Surface list appears to be corrupt");   // :376
        }

        // [DIAG] BRN_RUMBLE_DIAG -- NOT IN THE X360 BINARY. The surface witness, part 1: ONCE, the list's
        // rumble table -- every surface's rumblesurface priority and speed band, and whether its RumbleSurface
        // ref resolved to a collection (a surface with priority < 0 never rumbles; an unresolved ref reads the
        // zeroed default layout, priority 0). Read-only: the same accessors the passes below use.
        static const bool sbSurfaceTableDiag = (getenv("BRN_RUMBLE_DIAG") != 0);
        static bool       sbSurfaceTableLogged = false;
        if (sbSurfaceTableDiag && !sbSurfaceTableLogged && CgsDev::Log::gpDebugPrint != 0)
        {
            sbSurfaceTableLogged = true;
            const s32 liNumSurfaces = mSurfaceList.Num_Surfaces();
            *CgsDev::Log::gpDebugPrint << "[rumble] surface table surfaces=" << liNumSurfaces << "\n";
            for (s32 liSurface = 0; liSurface < liNumSurfaces; ++liSurface)
            {
                void* lpTableRef = mSurfaceList.Surfaces(static_cast<u32>(liSurface));
                if (lpTableRef == nullptr)
                {
                    continue;
                }
                Attrib::Gen::surface       lTableSurface(*static_cast<const Attrib::RefSpec*>(lpTableRef), 0);
                Attrib::Gen::rumblesurface lTableRumble(lTableSurface.RumbleSurface(), 0);
                *CgsDev::Log::gpDebugPrint
                    << "[rumble] surface table id=" << liSurface << " prio=" << lTableRumble.RumblePriority()
                    << " min=" << lTableRumble.MinSpeedForRumble() << " max=" << lTableRumble.MaxSpeedForRumble()
                    << " resolved=" << static_cast<s32>(lTableSurface.RumbleSurface().HasResolvedCollection() ? 1 : 0)
                    << "\n";
            }
        }

        const BrnPhysics::Vehicle::RaceCarState* lpRaceCarState =
            lpActiveRaceCarInterface->GetRaceCarState(lePlayerCarIndex);

        for (EVehicleDrivenWheel leWheelIndex = eFrontLeftWheel; leWheelIndex < eNumDrivenWheels; )
        {
            mau8SurfaceID[leWheelIndex]          = 0xFF;
            mau8NumWheelsOnSurface[leWheelIndex] = 0;
            leWheelIndex = static_cast<EVehicleDrivenWheel>(leWheelIndex + 1);
            CGS_ASSERT(leWheelIndex <= eNumDrivenWheels, "leEnumIndex <= eNumDrivenWheels");
        }

        for (EVehicleDrivenWheel leWheelIndex = eFrontLeftWheel; leWheelIndex < eNumDrivenWheels; )
        {
            const BrnPhysics::Vehicle::Wheel::RoadContact& lrRoadContact = lpRaceCarState->maWheels[leWheelIndex].mRoadContact;
            if (lrRoadContact.mbLineTestIsValid && lrRoadContact.mbIsOnGround)
            {
                const u8 lu8SurfaceId = static_cast<u8>(
                    (static_cast<u16>(lrRoadContact.mCollisionTag.muValue) >> KU_SURFACE_ID_SHIFT) & KU_SURFACE_ID_MASK);

                void* lpSurfaceRef = mSurfaceList.Surfaces(lu8SurfaceId);
                if (lpSurfaceRef == nullptr)
                {
                    lpSurfaceRef = Attrib::DefaultDataArea(KU_SURFACE_REFSPEC_BYTES);
                }
                Attrib::Gen::surface       lSurface(*static_cast<const Attrib::RefSpec*>(lpSurfaceRef), 0);
                Attrib::Gen::rumblesurface lRumbleSurface(lSurface.RumbleSurface(), 0);

                // [DIAG] BRN_RUMBLE_DIAG -- NOT IN THE X360 BINARY. The surface witness, part 2: one line
                // whenever a grounded wheel's surface id changes (budgeted), with that surface's priority --
                // the proof the wheel pass is dispatched on live data.
                {
                    static const bool sbWheelDiag = (getenv("BRN_RUMBLE_DIAG") != 0);
                    static s32        saiLastSurface[4] = { -1, -1, -1, -1 };
                    static s32        siWheelDiagLines = 0;
                    if (sbWheelDiag && siWheelDiagLines < 64 && CgsDev::Log::gpDebugPrint != 0
                        && saiLastSurface[leWheelIndex] != static_cast<s32>(lu8SurfaceId))
                    {
                        saiLastSurface[leWheelIndex] = static_cast<s32>(lu8SurfaceId);
                        ++siWheelDiagLines;
                        *CgsDev::Log::gpDebugPrint
                            << "[rumble] surface wheel=" << static_cast<s32>(leWheelIndex)
                            << " id=" << static_cast<s32>(lu8SurfaceId)
                            << " prio=" << lRumbleSurface.RumblePriority()
                            << " mph=" << lpRaceCarState->mfSpeedMPH << "\n";
                    }
                }

                if (lRumbleSurface.RumblePriority() >= 0)
                {
                    EVehicleDrivenWheel leSurfaceListIndex = eFrontLeftWheel;
                    while (leSurfaceListIndex < eNumDrivenWheels)
                    {
                        if (mau8NumWheelsOnSurface[leSurfaceListIndex] == 0)
                        {
                            mau8SurfaceID[leSurfaceListIndex]          = lu8SurfaceId;
                            mau8NumWheelsOnSurface[leSurfaceListIndex] = 1;
                            break;
                        }
                        if (lu8SurfaceId == mau8SurfaceID[leSurfaceListIndex])
                        {
                            ++mau8NumWheelsOnSurface[leSurfaceListIndex];
                            break;
                        }
                        leSurfaceListIndex = static_cast<EVehicleDrivenWheel>(leSurfaceListIndex + 1);
                        CGS_ASSERT(leSurfaceListIndex <= eNumDrivenWheels, "leEnumIndex <= eNumDrivenWheels");
                    }
                    CGS_ASSERT(leSurfaceListIndex < eNumDrivenWheels,
                               "leSurfaceListIndex < BrnPhysics::Vehicle::eNumDrivenWheels");   // :433
                }
            }
            leWheelIndex = static_cast<EVehicleDrivenWheel>(leWheelIndex + 1);
            CGS_ASSERT(leWheelIndex <= eNumDrivenWheels, "leEnumIndex <= eNumDrivenWheels");
        }

        for (EVehicleDrivenWheel leRumbleListIndex = eFrontLeftWheel; leRumbleListIndex < eNumDrivenWheels; )
        {
            if (manRumbleID[leRumbleListIndex] >= 0)
            {
                bool lbRumbleFound = false;
                for (EVehicleDrivenWheel leSurfaceListIndex = eFrontLeftWheel; leSurfaceListIndex < eNumDrivenWheels; )
                {
                    if (mau8NumWheelsOnSurface[leSurfaceListIndex] != 0
                        && manRumbleID[leRumbleListIndex] == static_cast<s32>(mau8SurfaceID[leSurfaceListIndex]))
                    {
                        lbRumbleFound = true;
                        break;
                    }
                    leSurfaceListIndex = static_cast<EVehicleDrivenWheel>(leSurfaceListIndex + 1);
                    CGS_ASSERT(leSurfaceListIndex <= eNumDrivenWheels, "leEnumIndex <= eNumDrivenWheels");
                }

                if (!lbRumbleFound || mbRumblePaused || mbInPictureParadise
                    || lpRaceCarState->meDriverType != BrnPhysics::Vehicle::E_DRIVER_TYPE_PLAYER)
                {
                    StopRumble(manRumbleID[leRumbleListIndex]);
                    manRumbleID[leRumbleListIndex] = -1;
                }
                else
                {
                    void* lpSurfaceRef = mSurfaceList.Surfaces(static_cast<u32>(manRumbleID[leRumbleListIndex]));
                    if (lpSurfaceRef == nullptr)
                    {
                        lpSurfaceRef = Attrib::DefaultDataArea(KU_SURFACE_REFSPEC_BYTES);
                    }
                    Attrib::Gen::surface       lSurface(*static_cast<const Attrib::RefSpec*>(lpSurfaceRef), 0);
                    Attrib::Gen::rumblesurface lRumbleSurface(lSurface.RumbleSurface(), 0);

                    const f32 lfSpeed           = rw::math::fpu::Abs(lpRaceCarState->mfSpeedMPH);
                    const f32 lfSpeedDifference = rw::math::fpu::Max(lRumbleSurface.MaxSpeedForRumble()
                                                                     - lRumbleSurface.MinSpeedForRumble(), KF_MIN_SPEED_BAND);
                    const f32 lfVolume = (rw::math::fpu::Clamp(lfSpeed, lRumbleSurface.MinSpeedForRumble(),
                                                               lRumbleSurface.MaxSpeedForRumble())
                                          - lRumbleSurface.MinSpeedForRumble()) / lfSpeedDifference;
                    const CgsInput::InputIO::JoltEffect lJoltEffect =
                    {
                        { lRumbleSurface.LeftMotorAttackTime(),   lRumbleSurface.LeftMotorDecayTime(),
                          lRumbleSurface.LeftMotorSustainTime(),  lRumbleSurface.LeftMotorReleaseTime(),
                          lRumbleSurface.LeftMotorPeakSpeed(),    lRumbleSurface.LeftMotorSustainSpeed() },
                        { lRumbleSurface.RightMotorAttackTime(),  lRumbleSurface.RightMotorDecayTime(),
                          lRumbleSurface.RightMotorSustainTime(), lRumbleSurface.RightMotorReleaseTime(),
                          lRumbleSurface.RightMotorPeakSpeed(),   lRumbleSurface.RightMotorSustainSpeed() },
                    };
                    ChangeRumbleVolume(manRumbleID[leRumbleListIndex], lfVolume, lJoltEffect);
                }
            }
            leRumbleListIndex = static_cast<EVehicleDrivenWheel>(leRumbleListIndex + 1);
            CGS_ASSERT(leRumbleListIndex <= eNumDrivenWheels, "leEnumIndex <= eNumDrivenWheels");
        }

        if (!mbRumblePaused && !mbInPictureParadise
            && lpRaceCarState->meDriverType == BrnPhysics::Vehicle::E_DRIVER_TYPE_PLAYER)
        {
            for (EVehicleDrivenWheel leSurfaceListIndex = eFrontLeftWheel; leSurfaceListIndex < eNumDrivenWheels; )
            {
                if (mau8NumWheelsOnSurface[leSurfaceListIndex] != 0)
                {
                    bool lbRumbleFound = false;
                    for (EVehicleDrivenWheel leRumbleListIndex = eFrontLeftWheel; leRumbleListIndex < eNumDrivenWheels; )
                    {
                        if (manRumbleID[leRumbleListIndex] == static_cast<s32>(mau8SurfaceID[leSurfaceListIndex]))
                        {
                            lbRumbleFound = true;
                            break;
                        }
                        leRumbleListIndex = static_cast<EVehicleDrivenWheel>(leRumbleListIndex + 1);
                        CGS_ASSERT(leRumbleListIndex <= eNumDrivenWheels, "leEnumIndex <= eNumDrivenWheels");
                    }

                    if (!lbRumbleFound)
                    {
                        EVehicleDrivenWheel leRumbleListIndex = eFrontLeftWheel;
                        while (leRumbleListIndex < eNumDrivenWheels)
                        {
                            if (manRumbleID[leRumbleListIndex] == -1)
                            {
                                break;
                            }
                            leRumbleListIndex = static_cast<EVehicleDrivenWheel>(leRumbleListIndex + 1);
                            CGS_ASSERT(leRumbleListIndex <= eNumDrivenWheels, "leEnumIndex <= eNumDrivenWheels");
                        }
                        CGS_ASSERT(leRumbleListIndex < eNumDrivenWheels,
                                   "leRumbleListIndex < BrnPhysics::Vehicle::eNumDrivenWheels");   // :541

                        manRumbleID[leRumbleListIndex] = mau8SurfaceID[leSurfaceListIndex];

                        void* lpSurfaceRef = mSurfaceList.Surfaces(mau8SurfaceID[leSurfaceListIndex]);
                        if (lpSurfaceRef == nullptr)
                        {
                            lpSurfaceRef = Attrib::DefaultDataArea(KU_SURFACE_REFSPEC_BYTES);
                        }
                        Attrib::Gen::surface       lSurface(*static_cast<const Attrib::RefSpec*>(lpSurfaceRef), 0);
                        Attrib::Gen::rumblesurface lRumbleSurface(lSurface.RumbleSurface(), 0);

                        const f32 lfSpeed           = rw::math::fpu::Abs(lpRaceCarState->mfSpeedMPH);
                        const f32 lfSpeedDifference = rw::math::fpu::Max(lRumbleSurface.MaxSpeedForRumble()
                                                                         - lRumbleSurface.MinSpeedForRumble(), KF_MIN_SPEED_BAND);
                        const f32 lfVolume = (rw::math::fpu::Clamp(lfSpeed, lRumbleSurface.MinSpeedForRumble(),
                                                                   lRumbleSurface.MaxSpeedForRumble())
                                              - lRumbleSurface.MinSpeedForRumble()) / lfSpeedDifference;
                        const CgsInput::InputIO::JoltEffect lJoltEffect =
                        {
                            { lRumbleSurface.LeftMotorAttackTime(),   lRumbleSurface.LeftMotorDecayTime(),
                              lRumbleSurface.LeftMotorSustainTime(),  lRumbleSurface.LeftMotorReleaseTime(),
                              lRumbleSurface.LeftMotorPeakSpeed(),    lRumbleSurface.LeftMotorSustainSpeed() },
                            { lRumbleSurface.RightMotorAttackTime(),  lRumbleSurface.RightMotorDecayTime(),
                              lRumbleSurface.RightMotorSustainTime(), lRumbleSurface.RightMotorReleaseTime(),
                              lRumbleSurface.RightMotorPeakSpeed(),   lRumbleSurface.RightMotorSustainSpeed() },
                        };
                        PlayRumble(manRumbleID[leRumbleListIndex], lfVolume, lRumbleSurface.RumblePriority(), lJoltEffect);
                    }
                }
                leSurfaceListIndex = static_cast<EVehicleDrivenWheel>(leSurfaceListIndex + 1);
                CGS_ASSERT(leSurfaceListIndex <= eNumDrivenWheels, "leEnumIndex <= eNumDrivenWheels");
            }
        }
    }

    // ------------------------------------------------------------------------
    // @ 0x82379370 -- BrnGameState::RumbleManager::UpdateImpacts()
    //
    // The console body (r26 == this, r31 == the RaceCarState, r30 == the spy then the run):
    //     if (idx == -1) return;                                            0x82379398
    //     lpState = itf->GetRaceCarState(idx);                              0x823793A4 (0x8227D690)
    //     if (mbRumblePaused || mbInPictureParadise) return;                lbz +0x399 / +0x39B
    //     if (lpState->meDriverType != E_DRIVER_TYPE_PLAYER) return;        lwz +0x458, cmpwi 0
    //     if (spy->mpData == NULL) return;                                  lwz 0(r30), cmplwi
    //     lpCarContacts = spy->GetRaceCarContacts();                        0x823793E0 (0x82277900)
    //     lpRunData = spy->GetRaceCarContactRunList()                       0x823793F0 (0x82355BF0)
    //                    ->GetRunDataWithEntityID(lpState->mEntityId);      0x823793F8 (0x82373C38)
    //     if (lpRunData == NULL) return;
    // then the hardest contact of the run: |mNormalStress| (lvx128 at contact+0x20, vmsum3fp128 +
    // vrsqrtefp + two Newton steps, vsel to 0 for a zero vector -- vmx128.py-decoded), times
    // KF_RUMBLE_IMPACT_WORLD_SCALE when the other entity is the WORLD (lbz contact+4 == 0), kept by
    // vcmpgtfp128 + vsel (strictly greater replaces). If the result is > 0 (flt_82001CC0), copy the
    // function-static template, scale three lanes of each envelope by its clamped volume
    // (fsel-pair Max(0)/Min(1) at 0x8237952C..0x82379548) and queue it -- if either volume is > 0.
    //
    // There is NO minimum-strength and NO separation-time gate in the console body: the DWARF's
    // KF_MIN_RUMBLE_IMPACT_STRENGTH / KF_MIN_RUMBLE_SEPARATION_TIME are never loaded here.
    // ------------------------------------------------------------------------
    void RumbleManager::UpdateImpacts(
            BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
            EActiveRaceCarIndex                                                    lePlayerCarIndex,
            BrnPhysics::ContactSpy::ContactSpyInterface*                           lpContactSpyInterface)
    {
        if (lePlayerCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID)
        {
            return;
        }

        const BrnPhysics::Vehicle::RaceCarState* lpState =
            lpActiveRaceCarInterface->GetRaceCarState(lePlayerCarIndex);

        if (mbRumblePaused || mbInPictureParadise ||
            lpState->meDriverType != BrnPhysics::Vehicle::E_DRIVER_TYPE_PLAYER)
        {
            return;
        }

        if (!lpContactSpyInterface->IsValid())
        {
            return;
        }

        const BrnPhysics::ContactSpy::ContactSpyData::RaceCarContactQueue* lpCarContacts =
            lpContactSpyInterface->GetRaceCarContacts();
        const BrnPhysics::ContactSpy::ContactSpyRunData* lpRunData =
            lpContactSpyInterface->GetRaceCarContactRunList()->GetRunDataWithEntityID(lpState->mEntityId);
        if (lpRunData == nullptr)
        {
            return;
        }

        // DWARF :617 `VecFloat lvHardestImpact` (GetVecFloat_Zero, a splat accumulator on the
        // console); every lane carries the same scalar, so the host keeps the scalar -- the tree's
        // established reduction for vpu magnitudes (vector3_operation.h Magnitude).
        f32 lfHardestImpact = 0.0f;
        for (s32 liIndex = 0; liIndex < lpRunData->GetRunLength(); ++liIndex)
        {
            const BrnPhysics::ContactSpy::BaseContact* lpContact =
                lpCarContacts->GetBaseContact(lpRunData->GetStartIndex() + liIndex);

            f32 lfImpactMagnitude = rw::math::vpu::Magnitude(lpContact->mNormalStress);
            if ((lpContact->mEntityIdB.muValue >> KU_ENTITY_ID_OWNER_SHIFT) ==
                static_cast<u32>(BrnWorld::E_ENTITYTYPE_WORLD))
            {
                lfImpactMagnitude *= KF_RUMBLE_IMPACT_WORLD_SCALE;
            }

            // MaskScalar lMask = lvImpactMagnitude > lvHardestImpact; Select(...) -- vcmpgtfp128 +
            // vsel @0x823794BC/0x823794D0: only a STRICTLY greater magnitude replaces.
            if (lfImpactMagnitude > lfHardestImpact)
            {
                lfHardestImpact = lfImpactMagnitude;
            }
        }

        if (lfHardestImpact > 0.0f)                                         // fcmpu f30, 0.0 @0x823794F0
        {
            // DWARF :647 `JoltEffect kJoltEffect` -- a function static in writable .data
            // 0x82CDBDE0 (the PS3 names it `UpdateImpacts(...)::kJoltEffect`):
            //     {0, 0, 0.5, 0.2, 0.6, 0.5 | 0.05, 0, 0.1, 0.1, 0.6, 0.5}   (x360rd; no CRT writer)
            static CgsInput::InputIO::JoltEffect kJoltEffect =
            {
                { 0.0f,  0.0f, 0.5f, 0.2f, 0.6f, 0.5f },   // mLowFreqJoltData   (0x82CDBDE0..0x82CDBDF4)
                { 0.05f, 0.0f, 0.1f, 0.1f, 0.6f, 0.5f },   // mHighFreqJoltData  (0x82CDBDF8..0x82CDBE0C)
            };

            CgsInput::InputIO::JoltEffect lJoltEffect = kJoltEffect;       // memcpy 0x30 @0x8237950C

            const f32 lfVolumeHigh = rw::math::fpu::Clamp(lfHardestImpact * KF_RUMBLE_IMPACT_SCALE_HIGH, 0.0f, 1.0f);
            const f32 lfVolumeLow  = rw::math::fpu::Clamp(lfHardestImpact * KF_RUMBLE_IMPACT_SCALE_LOW,  0.0f, 1.0f);

            // High-frequency envelope, lanes 11 / 10 / 8 (stfs var_94 / var_98 / var_A0).
            lJoltEffect.mHighFreqJoltData.mfSustainSpeedValue *= lfVolumeHigh;
            lJoltEffect.mHighFreqJoltData.mfPeakSpeedValue    *= lfVolumeHigh;
            lJoltEffect.mHighFreqJoltData.mfSustainTime       *= lfVolumeHigh;
            // Low-frequency envelope, lanes 5 / 4 / 2 (stfs var_AC / var_B0 / var_B8).
            lJoltEffect.mLowFreqJoltData.mfSustainSpeedValue  *= lfVolumeLow;
            lJoltEffect.mLowFreqJoltData.mfPeakSpeedValue     *= lfVolumeLow;
            lJoltEffect.mLowFreqJoltData.mfSustainTime        *= lfVolumeLow;

            if (lfVolumeHigh > 0.0f || lfVolumeLow > 0.0f)                  // bgt @0x82379594 / ble @0x8237959C
            {
                PlayJolt(KN_RUMBLE_IMPACT_PRIORITY, lJoltEffect);          // 0x823795AC
            }
        }
    }

    // ------------------------------------------------------------------------
    // @ 0x823795C8 -- BrnGameState::RumbleManager::OnVehicleAggressorImpact()
    //
    //     memcpy(lJoltEffect, kJoltEffect /* .data 0x82CDBE10 */, 0x30)       0x823795F4
    //     CGS_ASSERT(leImpactType != E_IMPACT_NONE)   (:751, li r5 0x2EF)      0x82379600
    //     switch on leImpactType - 1 (cmplwi 7 -- UNSIGNED, so NONE and anything out of range
    //     take the default); jump table 0x8237964C read with x360rd:
    //         TRADING_PAINT(1) / GRINDING(7) / RUBBING(8)  -> 0.25  (flt_82003F40)
    //         NUDGE(2)                                     -> 0.5   (flt_82001DA0)
    //         SLAM(3) / SHUNT(4) / BOOST_SLAM(5) / BOOST_SHUNT(6) -> 1.0 (flt_82001C98)
    //         default                                      -> 0.0   (flt_82001CC0)
    //     low-frequency lanes 5 / 4 / 2 *= lfVolumeLow                          0x82379694..0x823796B8
    //     if (lfVolumeLow > 0) queue it at KN_RUMBLE_VEHICLE_IMPACT_PRIORITY   0x823796BC..0x823796F0
    // The X360 INLINED PlayJolt at that last step ({0, -1, 0x3E9} + memcpy + AddEventSafe on
    // this+0x10); the PS3 twin 0x240050 calls PlayJolt(this, 1001, &lJoltEffect) by name, which is
    // the source form. NO mbRumblePaused gate in either build.
    // ------------------------------------------------------------------------
    void RumbleManager::OnVehicleAggressorImpact(BrnPhysics::Vehicle::EImpactType leImpactType)
    {
        // DWARF :711 `JoltEffect kJoltEffect` -- function static, writable .data 0x82CDBE10:
        //     {0, 0, 0.5, 0.2, 0.1, 0.2 | 0, 0, 0, 0, 0, 0}   (x360rd; no CRT writer)
        static CgsInput::InputIO::JoltEffect kJoltEffect =
        {
            { 0.0f, 0.0f, 0.5f, 0.2f, 0.1f, 0.2f },   // mLowFreqJoltData   (0x82CDBE10..0x82CDBE24)
            { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },   // mHighFreqJoltData  (0x82CDBE28..0x82CDBE3C)
        };

        CgsInput::InputIO::JoltEffect lJoltEffect = kJoltEffect;

        CGS_ASSERT(leImpactType != BrnPhysics::Vehicle::E_IMPACT_NONE,
                   "leImpactType != BrnPhysics::Vehicle::E_IMPACT_NONE");

        f32 lfVolumeLow;
        switch (leImpactType)
        {
        case BrnPhysics::Vehicle::E_IMPACT_TRADING_PAINT:
        case BrnPhysics::Vehicle::E_IMPACT_GRINDING:
        case BrnPhysics::Vehicle::E_IMPACT_RUBBING:
            lfVolumeLow = 0.25f;
            break;
        case BrnPhysics::Vehicle::E_IMPACT_NUDGE:
            lfVolumeLow = 0.5f;
            break;
        case BrnPhysics::Vehicle::E_IMPACT_SLAM:
        case BrnPhysics::Vehicle::E_IMPACT_SHUNT:
        case BrnPhysics::Vehicle::E_IMPACT_BOOST_SLAM:
        case BrnPhysics::Vehicle::E_IMPACT_BOOST_SHUNT:
            lfVolumeLow = 1.0f;
            break;
        default:
            lfVolumeLow = 0.0f;
            break;
        }

        lJoltEffect.mLowFreqJoltData.mfSustainSpeedValue *= lfVolumeLow;
        lJoltEffect.mLowFreqJoltData.mfPeakSpeedValue    *= lfVolumeLow;
        lJoltEffect.mLowFreqJoltData.mfSustainTime       *= lfVolumeLow;

        if (lfVolumeLow > 0.0f)
        {
            PlayJolt(KN_RUMBLE_VEHICLE_IMPACT_PRIORITY, lJoltEffect);
        }
    }

    // ------------------------------------------------------------------------
    // BrnGameState::RumbleManager::OnVehicleVictimImpact() -- DWARF BrnRumbleManager.cpp:793.
    //
    // No X360 body of its own: ProcessGameEvents' victim leg `bl`s 0x823795C8 (0x823A27EC), the
    // aggressor body, i.e. the two ICF-folded. The PS3 keeps the symbol (0x2401D4) as a pure thunk
    // onto OnVehicleAggressorImpact -- the pad gets the same jolt whichever side of the hit the
    // player is on.
    // ------------------------------------------------------------------------
    void RumbleManager::OnVehicleVictimImpact(BrnPhysics::Vehicle::EImpactType leImpactType)
    {
        OnVehicleAggressorImpact(leImpactType);
    }

    // ------------------------------------------------------------------------
    // @ 0x82364978 -- BrnGameState::RumbleManager::BridgeRumbleToInput()  (DWARF :97; cpp :811..)
    //
    // The console body, in its order (r31 == this, r27 == the buffer, r21 == the timer interface):
    //   0x82364998..0x823649D4  jolts: the length is read ONCE (lwz 0x18), each event through the
    //                           out-of-line GetEvent @0x8235CF50 -> PostPlayJoltEffectByPlayer(ev+0 player,
    //                           ev+8 priority, ev+0xC jolt)
    //   0x823649D8..0x82364A14  stops: length once (0x33C), GetEvent @0x8235CFF8 ->
    //                           PostStopRumbleEffectByPlayer(ev+0 player, ev+8 rumble id)
    //   0x82364A18..0x82364AE4  plays: bound once (0x114), the inlined GetEvent keeps its three asserts
    //                           (CgsBaseEventQueue.h :292/:294/:295) -> PostPlayRumbleEffectByPlayer(ev+0,
    //                           ev+8, ev+0xC, ev+0x3C id, ev+0x40 volume in f1)
    //   0x82364AE8..0x82364B90  volumes: same shape (0x230, stride 0x40) ->
    //                           PostChangeVolumeRumbleEffectByPlayer(ev+0, ev+8 jolt, ev+0x38 id, ev+0x3C vol)
    //   0x82364B9C              SetTimerStatusInterface(buffer, timer)
    //   0x82364BA0..0x82364BD0  +0x394 = (mbRumblePaused || mbInPictureParadise); +0x395 = mbRumbleEnabled;
    //                           +0x396 = mbWheelForceFeedback
    //   0x82364BD4..0x82364BE0  stw 0 -> the four lengths (+0x18 jolt, +0x114 play, +0x230 volume, +0x33C stop)
    // ------------------------------------------------------------------------
    void RumbleManager::BridgeRumbleToInput(CgsInput::InputIO::PreWorldInputBuffer* lpInputInputBuffer,
                                            const CgsSystem::TimerStatusInterface*  lpTimerStatusInterface)
    {
        s32 liLength = mPlayJoltEffectEventQueue.GetLength();
        for (s32 liIndex = 0; liIndex < liLength; ++liIndex)
        {
            const CgsInput::InputIO::PlayJoltEffectEvent& lJoltEvent = mPlayJoltEffectEventQueue.GetEvent(liIndex);
            lpInputInputBuffer->PostPlayJoltEffectByPlayer(lJoltEvent.miPlayer, lJoltEvent.miRumblePriority,
                                                           lJoltEvent.mJoltEffect);
        }
        const s32 liNumJolts = liLength;

        liLength = mStopRumbleEffectEventQueue.GetLength();
        for (s32 liIndex = 0; liIndex < liLength; ++liIndex)
        {
            const CgsInput::InputIO::StopRumbleEffectEvent& lStopRumbleEvent = mStopRumbleEffectEventQueue.GetEvent(liIndex);
            lpInputInputBuffer->PostStopRumbleEffectByPlayer(lStopRumbleEvent.miPlayer, lStopRumbleEvent.miRumbleId);
        }
        const s32 liNumStops = liLength;

        liLength = mPlayRumbleEffectEventQueue.GetLength();
        for (s32 liIndex = 0; liIndex < liLength; ++liIndex)
        {
            const CgsInput::InputIO::PlayRumbleEffectEvent& lPlayRumbleEvent = mPlayRumbleEffectEventQueue.GetEvent(liIndex);
            lpInputInputBuffer->PostPlayRumbleEffectByPlayer(lPlayRumbleEvent.miPlayer, lPlayRumbleEvent.miRumblePriority,
                                                             lPlayRumbleEvent.mJoltEffect, lPlayRumbleEvent.miRumbleId,
                                                             lPlayRumbleEvent.mfRumbleVolume);
        }
        const s32 liNumPlays = liLength;

        liLength = mChangeVolumeRumbleEffectEventQueue.GetLength();
        for (s32 liIndex = 0; liIndex < liLength; ++liIndex)
        {
            const CgsInput::InputIO::ChangeVolumeRumbleEffectEvent& lChangeVolumeRumbleEvent =
                mChangeVolumeRumbleEffectEventQueue.GetEvent(liIndex);
            lpInputInputBuffer->PostChangeVolumeRumbleEffectByPlayer(lChangeVolumeRumbleEvent.miPlayer,
                                                                     lChangeVolumeRumbleEvent.mJoltEffect,
                                                                     lChangeVolumeRumbleEvent.miRumbleId,
                                                                     lChangeVolumeRumbleEvent.mfRumbleVolume);
        }
        const s32 liNumVolumes = liLength;

        lpInputInputBuffer->SetTimerStatusInterface(*lpTimerStatusInterface);
        lpInputInputBuffer->SetRumblePaused(mbRumblePaused || mbInPictureParadise);
        lpInputInputBuffer->SetRumbleEnabled(mbRumbleEnabled);
        lpInputInputBuffer->SetWheelForceFeedbackEnabled(mbWheelForceFeedback);

        mPlayJoltEffectEventQueue.Clear();
        mPlayRumbleEffectEventQueue.Clear();
        mChangeVolumeRumbleEffectEventQueue.Clear();
        mStopRumbleEffectEventQueue.Clear();

        // [DIAG] BRN_RUMBLE_DIAG -- NOT IN THE X360 BINARY. The dispatch witness for the drain: one
        // line per update step that handed anything to the input buffer (budgeted), with the state
        // flags it published. Pairs with PlayJolt's producer line below and, downstream, the
        // "[rumble] pad-request" line (InputPads::UpdatePadRumble, CgsInputPads.cpp -- the motor
        // speeds the effects ask for) and the "[rumble] motor" line (the PC XInputSetState leaf,
        // CgsInputPadsPC.cpp -- what a connected pad was sent).
        static const bool sbBridgeDiag = (getenv("BRN_RUMBLE_DIAG") != 0);
        static s32        siBridgeDiagLines = 0;
        const s32         KI_BRIDGE_DIAG_MAX_LINES = 64;
        if (sbBridgeDiag && (liNumJolts + liNumStops + liNumPlays + liNumVolumes) > 0
            && siBridgeDiagLines < KI_BRIDGE_DIAG_MAX_LINES && CgsDev::Log::gpDebugPrint != 0)
        {
            ++siBridgeDiagLines;
            *CgsDev::Log::gpDebugPrint
                << "[rumble] bridge jolts=" << liNumJolts << " stops=" << liNumStops
                << " plays=" << liNumPlays << " volumes=" << liNumVolumes
                << " paused=" << static_cast<s32>(lpInputInputBuffer->GetRumblePaused() ? 1 : 0)
                << " enabled=" << static_cast<s32>(lpInputInputBuffer->GetRumbleEnabled() ? 1 : 0)
                << " ff=" << static_cast<s32>(lpInputInputBuffer->GetWheelForceFeedbackEnabled() ? 1 : 0)
                << "\n";
        }
    }

    // ------------------------------------------------------------------------
    // DWARF BrnRumbleManager.cpp :894 / :912 / :929 -- PlayRumble / ChangeRumbleVolume / StopRumble
    // (PS3 0x256B88 / 0x25687C / 0x256E4C; parameter names from the PS3 DWARF). No X360 copies: each
    // is inlined into UpdateSurfaceRumble, whose three sites build the event on the stack with
    // miPlayer 0 (`stw r19`, r19 == 0) and miPort -1 (`stw r22`, r22 == -1) and append it through the
    // ASSERTING AddEvent (not PlayJolt's AddEventSafe):
    //   PlayRumble          0x82379250..0x82379318  {0, -1, priority, envelope, id, volume} -> +0x10C
    //   ChangeRumbleVolume  0x82378EF4..0x82378FB8  {0, -1, envelope, id, volume}           -> +0x228
    //   StopRumble          0x82378FD0..0x82378FE8  {0, -1, id}                             -> +0x334
    // The [DIAG] lines (BRN_RUMBLE_DIAG, NOT IN THE X360 BINARY) are the surface-rumble witnesses,
    // budgeted per kind.
    // ------------------------------------------------------------------------
    void RumbleManager::PlayRumble(s32 liRumbleId, f32 lfVolume, s32 liRumblePriority,
                                   const CgsInput::InputIO::JoltEffect& lJoltEffect)
    {
        CgsInput::InputIO::PlayRumbleEffectEvent lEvent;
        lEvent.miPlayer         = 0;
        lEvent.miPort           = -1;
        lEvent.miRumblePriority = liRumblePriority;
        lEvent.mJoltEffect      = lJoltEffect;
        lEvent.miRumbleId       = liRumbleId;
        lEvent.mfRumbleVolume   = lfVolume;
        mPlayRumbleEffectEventQueue.AddEvent(lEvent);

        static const bool sbSurfaceDiag = (getenv("BRN_RUMBLE_DIAG") != 0);
        static s32        siSurfaceDiagLines = 0;
        if (sbSurfaceDiag && siSurfaceDiagLines < 32 && CgsDev::Log::gpDebugPrint != 0)
        {
            ++siSurfaceDiagLines;
            *CgsDev::Log::gpDebugPrint
                << "[rumble] surface play id=" << liRumbleId << " prio=" << liRumblePriority << " vol=" << lfVolume
                << " low{sus=" << lJoltEffect.mLowFreqJoltData.mfSustainTime
                << " peak=" << lJoltEffect.mLowFreqJoltData.mfPeakSpeedValue
                << " susv=" << lJoltEffect.mLowFreqJoltData.mfSustainSpeedValue
                << "} high{sus=" << lJoltEffect.mHighFreqJoltData.mfSustainTime
                << " peak=" << lJoltEffect.mHighFreqJoltData.mfPeakSpeedValue
                << " susv=" << lJoltEffect.mHighFreqJoltData.mfSustainSpeedValue << "}\n";
        }
    }

    void RumbleManager::ChangeRumbleVolume(s32 liRumbleId, f32 lfVolume, const CgsInput::InputIO::JoltEffect& lJoltEffect)
    {
        CgsInput::InputIO::ChangeVolumeRumbleEffectEvent lEvent;
        lEvent.miPlayer       = 0;
        lEvent.miPort         = -1;
        lEvent.mJoltEffect    = lJoltEffect;
        lEvent.miRumbleId     = liRumbleId;
        lEvent.mfRumbleVolume = lfVolume;
        mChangeVolumeRumbleEffectEventQueue.AddEvent(lEvent);

        static const bool sbSurfaceDiag = (getenv("BRN_RUMBLE_DIAG") != 0);
        static s32        siSurfaceDiagLines = 0;
        if (sbSurfaceDiag && siSurfaceDiagLines < 32 && CgsDev::Log::gpDebugPrint != 0)
        {
            ++siSurfaceDiagLines;
            *CgsDev::Log::gpDebugPrint << "[rumble] surface volume id=" << liRumbleId << " vol=" << lfVolume << "\n";
        }
    }

    void RumbleManager::StopRumble(s32 liRumbleId)
    {
        CgsInput::InputIO::StopRumbleEffectEvent lEvent;
        lEvent.miPlayer   = 0;
        lEvent.miPort     = -1;
        lEvent.miRumbleId = liRumbleId;
        mStopRumbleEffectEventQueue.AddEvent(lEvent);

        static const bool sbSurfaceDiag = (getenv("BRN_RUMBLE_DIAG") != 0);
        static s32        siSurfaceDiagLines = 0;
        if (sbSurfaceDiag && siSurfaceDiagLines < 32 && CgsDev::Log::gpDebugPrint != 0)
        {
            ++siSurfaceDiagLines;
            *CgsDev::Log::gpDebugPrint << "[rumble] surface stop id=" << liRumbleId << "\n";
        }
    }

    // ------------------------------------------------------------------------
    // @ 0x8236E7F8 -- BrnGameState::RumbleManager::PlayJolt()
    //
    //     stw 0  -> lEvent.miPlayer         (+0x0)   0x8236E81C
    //     stw -1 -> lEvent.miPort           (+0x4)   0x8236E82C
    //     stw r4 -> lEvent.miRumblePriority (+0x8)   0x8236E828
    //     memcpy(lEvent.mJoltEffect, r5, 0x30)       0x8236E830
    //     BaseEventQueue<PlayJoltEffectEvent>::AddEventSafe(this+0x10, &lEvent)   0x8236E83C
    // (DWARF :950 `PlayJoltEffectEvent lEvent`, its inlined Construct is exactly these stores.)
    // The result of AddEventSafe is discarded, as in the console: a full queue drops the jolt.
    // ------------------------------------------------------------------------
    void RumbleManager::PlayJolt(s32 liRumblePriority, const CgsInput::InputIO::JoltEffect& lJoltEffect)
    {
        CgsInput::InputIO::PlayJoltEffectEvent lEvent;
        lEvent.miPlayer         = 0;
        lEvent.miPort           = -1;
        lEvent.miRumblePriority = liRumblePriority;
        lEvent.mJoltEffect      = lJoltEffect;

        const bool lbQueued = mPlayJoltEffectEventQueue.AddEventSafe(lEvent);

        // [DIAG] BRN_RUMBLE_DIAG -- NOT IN THE X360 BINARY. The dispatch witness for the rumble
        // producers (FX-RUMBLE): one line per jolt the game state asks for, with the priority that
        // names its producer (1000 contact / 1001 race-car impact / 1002 landing / 1010 crash
        // start), whether the jolt queue took it, and the scaled envelope. `queued=0` with len=4 was
        // the missing-consumer hole of G10-D4 (nothing drained the queue on PC before FX-RUMBLE3's
        // BridgeRumbleToInput); with the drain in place the queue empties every update step, so a
        // `queued=0` now means five producers fired inside ONE step -- the console drops the fifth
        // exactly the same way (AddEventSafe). The budget is PER PRODUCER: the contact jolt can fire
        // on every frame of a scrape, and a shared budget would let it starve the rarer crash /
        // landing / race-car-impact lines -- a witness that reports an absence it never observed.
        // The drain and the motor have their own lines ("[rumble] bridge", "[rumble] pad-request",
        // "[rumble] motor").
        static const bool sbRumbleDiag = (getenv("BRN_RUMBLE_DIAG") != 0);
        static s32        saiRumbleDiagLines[5] = { 0, 0, 0, 0, 0 };
        const s32         KI_RUMBLE_DIAG_MAX_LINES_PER_PRODUCER = 16;
        const s32         liSlot = (liRumblePriority == KN_RUMBLE_IMPACT_PRIORITY)         ? 0
                                 : (liRumblePriority == KN_RUMBLE_VEHICLE_IMPACT_PRIORITY) ? 1
                                 : (liRumblePriority == KN_LANDING_RUMBLE_PRIORITY)        ? 2
                                 : (liRumblePriority == KN_CRASH_RUMBLE_PRIORITY)          ? 3
                                 : 4;
        if (sbRumbleDiag && saiRumbleDiagLines[liSlot] < KI_RUMBLE_DIAG_MAX_LINES_PER_PRODUCER &&
            CgsDev::Log::gpDebugPrint != 0)
        {
            ++saiRumbleDiagLines[liSlot];
            *CgsDev::Log::gpDebugPrint
                << "[rumble] jolt prio=" << liRumblePriority
                << " queued=" << static_cast<s32>(lbQueued ? 1 : 0)
                << " len=" << mPlayJoltEffectEventQueue.GetLength()
                << " low{sus=" << lJoltEffect.mLowFreqJoltData.mfSustainTime
                << " peak=" << lJoltEffect.mLowFreqJoltData.mfPeakSpeedValue
                << " susv=" << lJoltEffect.mLowFreqJoltData.mfSustainSpeedValue
                << "} high{sus=" << lJoltEffect.mHighFreqJoltData.mfSustainTime
                << " peak=" << lJoltEffect.mHighFreqJoltData.mfPeakSpeedValue
                << " susv=" << lJoltEffect.mHighFreqJoltData.mfSustainSpeedValue
                << "}\n";
        }
    }
}
