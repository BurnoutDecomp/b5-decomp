#pragma once

// BrnPhysics::Vehicle::TrafficPhysics -- the lightweight ("traffic") driver that wraps the full
// VehiclePhysics solver with scripted control input and a "freak-out" panic FSM. Traffic cars are
// NOT a separate integrator: PreparePhysical forwards into VehiclePhysics::Prepare and Update
// halves the scripted controls over a speed gate, advances the freak-out FSM, then forwards into
// the SAME VehiclePhysics::UpdateShunt + UpdateCrashing the player uses.
//
// Member SEQUENCE verbatim from the DecFIGS DWARF (TrafficPhysics.h:117-122).
//
// ==================================================================================================
// ⭐⭐ THE OWN BLOCK IS DERIVED, NOT GUESSED, AND IT CLOSES WITH ZERO SLACK AT BOTH ENDS
// (2026-08-03, the Construct wave -- from the X360 asm of Construct @0x8262E980, pulled out of
// BURNOUT_X360_ARTIST.XEX.i64 with headless IDA 9.3 because that address is an `.ida-exports` hole:
// the JSON set jumps 0x8262E848 -> 0x8262EBE8, and IDA confirms the gap is one whole function,
// 0x8262E980..0x8262EBE8, 154 instructions, named BrnPhysics::Vehicle::TrafficPhysics::Construct).
//
//     start   0x13F0 == X360Layout::KU_VP_SIZEOF -- where VehiclePhysics's own block closes, an
//                       anchor a DIFFERENT wave derived from DIFFERENT functions
//     mOwnerID              @0x13F0  (5104)   EntityId, 4
//     mu8FreakOutState      @0x13F4  (5108)   u8      -- asm-literal: SetFreakedOut @0x825B8948
//                                                        `stb r11, 0x13F4(r3)`
//     mfFreakOutDirection   @0x13F8  (5112)   f32     -- asm-literal: SetFreakedOut
//                                                        `stfs f1, 0x13F8(r3)`
//     mfFreakOutTime        @0x13FC  (5116)   f32
//     mRandom               @0x1400  (5120)   CgsNumeric::Random, 0x30, 16-aligned
//                                             -- asm-literal: Construct `addi r11, r31, 0x1400`,
//                                                then `stw 0x3F800000, 0(r11)` / `std seed, 0x20(r11)`
//                                                / `stw idx, 0x28(r11)`
//     end     0x1400 + 0x30 == 0x1430 == the per-element stride PhysicalTrafficManager::Construct
//                       @0x82636CA8 uses for maFullTrafficPhysics[20] (`mulli r11, r29, 0x1430`)
//
// Every one of the four data seats is forced: mOwnerID at the base end, the two SetFreakedOut
// literals, mRandom's own 16-alignment landing it exactly on the Construct literal 0x1400, and the
// manager's 0x1430 stride closing it. Gated by TrafficPhysics_layout_check.cpp (tamper-tested).
//
// ⚠️⚠️ TWO LOCAL TYPE FORKS RETIRED HERE, 2026-08-03. This header used to declare, at namespace
// scope in BrnPhysics::Vehicle, its own private copies of two names that already have real homes:
//
//   (1) `typedef u32 EntityId;` -- a SHADOW of the global `struct EntityId { u32 muValue; }`
//       (BrnCommonTypes.h:27). It did not merely rename a type: because it sits in
//       BrnPhysics::Vehicle, it silently re-typed EVERY `EntityId` spelled in that namespace by any
//       header included AFTER it -- including `CreatePhysicalTrafficEvent::mCrasherID`
//       (SharedIO/BrnVehicleEvents.h:226). The same header text therefore meant a 4-byte struct or a
//       raw u32 depending on include order, which is why `mOwnerID = lpEvent->mCrasherID;` compiled
//       at all. Retired; mOwnerID is the real ::EntityId, the same type maTrafficEntityIDs uses.
//
//   (2) `struct Random { u32 muState; };` -- a 4-byte stand-in for CgsNumeric::Random, which has an
//       owning home (GameShared/GameClasses/Numeric/CgsRandom.h) and is **48 bytes, 16-aligned**.
//       ⭐ The X360 asm settles it beyond argument: Construct's tail is CgsNumeric::Random::Construct
//       INLINED, statement for statement --
//           muSeed = 0xC87CD8C91AD0891B          (`insrdi` of 0xC87CD8C9 : 0x1AD0891B, `std 0x20`)
//           muOldestBufferIndex = 0              (`stw r30, 0x28(r11)`)
//           mauIntegerBuffer[0] = 0x3F800000     (`stw r7, 0(r11)`, r7 = `lis 0x3F80`, exactly 1.0f)
//           7 x AddRandomFloatToBuffer()         (7 unrolled copies of: read seed+index, take the
//                                                OLD seed's high word, `inslwi rX, hi, 23, 9` into a
//                                                0x3F800000 base == 1.0f | (hi >> 9), seed =
//                                                seed * 0x5851F42D4C957F2D + 1, index = (index+1)&7,
//                                                `stwx` at index*4)
//           muOldestBufferIndex = (muOldestBufferIndex + 1) & 7    (the trailing lwz/addi/clrlwi/stw)
//       -- which is CgsRandom.h's committed Construct() body character for character, including the
//       "first slot is exactly 1.0f, then SEVEN refills, then one extra index bump" asymmetry. At the
//       4-byte stand-in the object was 44 bytes short, so the class closed on 0x1404 instead of
//       0x1430 and `TrafficPhysics::Update`'s `mRandom.muState` LCG step was writing at +0x1400,
//       i.e. into mauIntegerBuffer[0], not into the seed.
// ==================================================================================================
//
// C11_simple_traffic_attribs group: TrafficPhysics::PreparePhysical is bodied (TrafficPhysics.cpp);
// TrafficPhysics::Update is PARTIAL -- the control-halving + freak-out FSM are bodied, but the
// inlined per-axis VMX128 `vlogefp`/`vexptefp` angular-velocity damping curve (the crashing path,
// driven by the un-homed rodata coefficient tables unk_82014AC0..AF0) is delegated to the committed
// VehiclePhysics::UpdateCrashing entry rather than fabricated.
//
// ⚠️ TU SPLIT (same precedent as RaceCarPhysics_Construct.cpp / BrnSimpleVehiclePhysics_Construct.cpp):
// Construct + SetFreakedOut live in TrafficPhysics_Construct.cpp, which IS mounted; PreparePhysical
// and Update stay in TrafficPhysics.cpp, which is NOT, because they call VehiclePhysics::Prepare /
// UpdateShunt / UpdateCrashing and all three are still declare-only.

#include "types.hpp"          // f32, u8, u32
#include "BrnCommonTypes.h"   // Vector3, Matrix44Affine, EntityId
#include "GameShared/GameClasses/Numeric/CgsRandom.h"   // CgsNumeric::Random -- mRandom's REAL type
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"   // base
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"  // AxisAlignedBox

namespace BrnPhysics
{
namespace Vehicle
{
    // VehicleAttribs / BrnPlayerDriverControls / StreamedDeformationSpec are already declared by the
    // included VehiclePhysics.h (the per-TU minimal-slice pattern). Only the spawn-event type is new:
    struct CreatePhysicalTrafficEvent;     // the spawn event (mOwnerID read from it @ +8)

    class TrafficPhysics : public VehiclePhysics
    {
    public:
        // DWARF TrafficPhysics.h:108 -- the panic state machine.
        enum EFreakOutState
        {
            E_FREAK_OUT_STATE_OFF           = 0,
            E_FREAK_OUT_STATE_INITIAL       = 1,
            E_FREAK_OUT_STATE_TURN_AND_ROLL = 2,
            E_FREAK_OUT_STATE_SPIN_OUT      = 3,
            E_NUM_FREAK_OUT_STATES          = 4,
        };

        // @0x8262E980 (DWARF TrafficPhysics.cpp:69). One-shot construction of a traffic body:
        // base-construct, base-reset, unfreeze, clear the car-car-response lane and prime the
        // per-car random ring. Bodied in TrafficPhysics_Construct.cpp; see the banner above for
        // the instruction-level decode and the layout it pins.
        //
        // ⚠️ Its ONLY caller in the image is PhysicalTrafficManager::Construct @0x82636CA8
        // (`bl` at 0x82636D80), which runs it over maFullTrafficPhysics[0..19] at a 0x1430 stride.
        void Construct();

        // @0x82639380: prepare a traffic body. Asserts the event/attribs/wheel-position/wheel-radii
        // pointers are non-null (debug). Sums the four streamed wheel positions, folds 1/4 of the
        // sum into the attribs' COM offset, builds a local transform from the spawn event, forwards
        // into VehiclePhysics::Prepare + SetWheelVelocities, then seeds the freak-out fields (state
        // OFF, direction 0, time 0, owner id = *(event+8)). Bodied in TrafficPhysics.cpp.
        bool PreparePhysical(const CreatePhysicalTrafficEvent* lpEvent, VehicleAttribs* lpAttribs,
                             const CgsGeometric::AxisAlignedBox& lrAABB,
                             const StreamedDeformationSpec* lpDeformSpec,
                             const Vector3* lpWheelPositions, const f32* lpafWheelRadii);

        // @0x82639590: the per-frame traffic update. ⭐⭐ RECONCILED FULL 2026-08-09 (crash/shunt
        // wave) -- the flagged stand-ins are gone; bodied in TrafficPhysics_Construct.cpp (457
        // insns read line by line). Param roles are the pass-through map into
        // VehiclePhysics::UpdateCrashing @0x82639C88 (r5/r7/r8/r9 forwarded verbatim); the f2
        // slot is never read by the body.
        virtual void Update(f32 lfTimeStep, f32 lfUnused, const Matrix44Affine* lpCameraMatrix,
                            const BrnPlayerDriverControls* lpControls, bool lbImpactTime,
                            bool lbPlayerAftertouchForceAdditive, bool lbShowtimeAllowed);

        // @0x825B8948 (DWARF TrafficPhysics.cpp:360), 18 instructions -- enter the freak-out FSM.
        // Bodied in TrafficPhysics_Construct.cpp (it is Update's only same-class callee, so it has
        // to be defined wherever the class's vtable is emitted).
        void SetFreakedOut(f32 lfDirection, f32 lfSeverity);

        bool IsFreakedOut() const { return mu8FreakOutState != E_FREAK_OUT_STATE_OFF; }

        // Never called and never defined outside TrafficPhysics_layout_check.cpp -- it exists so
        // that TU can take offsetof() of the private members below. Same shape as
        // VehiclePhysics::_AssertOwnBlockLayout.
        static void _AssertOwnBlockLayout();

    private:
        EntityId           mOwnerID;            // :117  @0x13F0 (5104)
        u8                 mu8FreakOutState;    // :118  @0x13F4 (5108)
        f32                mfFreakOutDirection; // :119  @0x13F8 (5112)
        f32                mfFreakOutTime;      // :120  @0x13FC (5116)
        CgsNumeric::Random mRandom;             // :122  @0x1400 (5120), 0x30 -> class ends 0x1430
    };

    // ---------------------------------------------------------------------------------------------
    // The freak-out tuning constants, shared by SetFreakedOut (TrafficPhysics_Construct.cpp) and the
    // FSM entry in Update (TrafficPhysics.cpp). All four live in `.data` and read ALL-ZERO in the
    // shipped image; each is written by an IDA-unmarked static-initialiser thunk -- exactly the shape
    // the Engine.cpp wave hit with unk_82FB9110 / flt_82F2A3E0. Recovered by disassembling the
    // thunks (2026-08-03):
    //
    //   0x82C5CEA0   flt_82FB9144 = flt_82F31928 * flt_82092BC8   ==  0.44704f * 120.0f
    //   0x82C5CEB8   flt_82FB8384 = flt_82FB9144 * flt_82FB9144   ==  (0.44704f * 120.0f)^2
    //   0x82C5CE80   flt_82FB8388 = flt_82F31928 * flt_8209D730   ==  0.44704f * 6400.0f
    //   0x82C5CE60   flt_82FB838C = flt_82F31928 * flt_8209D72C   ==  0.44704f * 250000.0f
    //
    // flt_82F31928 is 0.447039992f -- the MPH -> m/s factor VehicleAttribs.cpp already carries as
    // KF_MPH_TO_MPS (it too is `.data`, but it is INITIALISED in the image, so it reads correctly).
    // Left at the image's 0.0f, SetFreakedOut's gate would be `severity > 0.0f`, i.e. ALWAYS TRUE,
    // and Update's two direction magnitudes would both be zero -- which is why they are recovered
    // rather than carried as the zeros the image shows.
    //
    // They are written as products, not as pre-multiplied literals, because that is how the console
    // computes them (one `fmuls` in a static initialiser) and it keeps both recovered factors
    // visible. ⚠️ FLAG: the two Update magnitudes are compared against a SQUARED gate inside
    // SetFreakedOut, which is dimensionally odd (250000 clears it, 6400 does not) -- but that IS what
    // the four thunks and the compare produce, so it is recorded rather than "corrected".
    const f32 KF_MPH_TO_MPS                    = 0.447039992f;               // flt_82F31928
    const f32 KF_FREAKOUT_MIN_SEVERITY_SPEED   = KF_MPH_TO_MPS * 120.0f;     // flt_82FB9144
    const f32 KF_FREAKOUT_MIN_SEVERITY_SQUARED =                             // flt_82FB8384
        KF_FREAKOUT_MIN_SEVERITY_SPEED * KF_FREAKOUT_MIN_SEVERITY_SPEED;
    const f32 KF_FREAKOUT_TURN_AND_ROLL_MIN_SPEED_MPH = 20.0f;               // flt_8208F9D4
    const f32 KF_FREAKOUT_SEVERITY_HIGH_DRAW   = KF_MPH_TO_MPS * 6400.0f;    // flt_82FB8388
    const f32 KF_FREAKOUT_SEVERITY_LOW_DRAW    = KF_MPH_TO_MPS * 250000.0f;  // flt_82FB838C

    namespace X360Layout
    {
        // ---- TrafficPhysics own block (0x13F0 .. 0x1430) -- every value an X360 literal ----------
        const unsigned KU_TP_OWN_BASE          = 0x13F0u;  // == KU_VP_SIZEOF, a different wave's anchor
        const unsigned KU_TP_FREAKOUTSTATE_OFF = 0x13F4u;  // SetFreakedOut `stb r11, 0x13F4(r3)`
        const unsigned KU_TP_FREAKOUTDIR_OFF   = 0x13F8u;  // SetFreakedOut `stfs f1, 0x13F8(r3)`
        const unsigned KU_TP_RANDOM_OFF        = 0x1400u;  // Construct    `addi r11, r31, 0x1400`
        const unsigned KU_TP_RANDOM_SEED_OFF   = 0x1420u;  // Construct    `std  r8,  0x20(r11)`
        const unsigned KU_TP_RANDOM_INDEX_OFF  = 0x1428u;  // Construct    `stw  r30, 0x28(r11)`
        const unsigned KU_TP_SIZEOF            = 0x1430u;  // PhysicalTrafficManager::Construct
                                                           //   `mulli r11, r29, 0x1430`, x20
    }
}
}
