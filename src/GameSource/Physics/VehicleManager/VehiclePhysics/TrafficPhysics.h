#pragma once

// BrnPhysics::Vehicle::TrafficPhysics -- the lightweight ("traffic") driver that wraps the full
// VehiclePhysics solver with scripted control input and a "freak-out" panic FSM. Traffic cars are
// NOT a separate integrator: PreparePhysical forwards into VehiclePhysics::Prepare and Update
// halves the scripted controls over a speed gate, advances the freak-out FSM, then forwards into
// the SAME VehiclePhysics::UpdateShunt + UpdateCrashing the player uses.
//
// Member SEQUENCE verbatim from the DecFIGS DWARF (TrafficPhysics.h:117-122). The console absolute
// offsets (mOwnerID @ +5104, mu8FreakOutState @ +5108, mfFreakOutDirection @ +5112, mfFreakOutTime
// @ +5116, mRandom @ +5152) are pinned BY NAME -- per project rule the ~0x13F0 bytes of base +
// VehiclePhysics state that precede this region are NOT reproduced as padding.
//
// C11_simple_traffic_attribs group: TrafficPhysics::PreparePhysical is bodied (TrafficPhysics.cpp);
// TrafficPhysics::Update is PARTIAL -- the control-halving + freak-out FSM are bodied, but the
// inlined per-axis VMX128 `vlogefp`/`vexptefp` angular-velocity damping curve (the crashing path,
// driven by the un-homed rodata coefficient tables unk_82014AC0..AF0) is delegated to the committed
// VehiclePhysics::UpdateCrashing entry rather than fabricated.

#include "types.hpp"          // f32, u8, u32
#include "BrnCommonTypes.h"   // Vector3, Matrix44Affine
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"   // base
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"  // AxisAlignedBox

namespace BrnPhysics
{
namespace Vehicle
{
    // VehicleAttribs / BrnPlayerDriverControls / StreamedDeformationSpec are already declared by the
    // included VehiclePhysics.h (the per-TU minimal-slice pattern). Only the spawn-event type is new:
    struct CreatePhysicalTrafficEvent;     // the spawn event (mOwnerID read from it @ +8)

    // EntityId -- the owning traffic-entity handle (DWARF TrafficPhysics.h:117). Carried as a u32.
    typedef u32 EntityId;

    // Random -- the per-car PRNG seed/state used for the freak-out spin coin-flip (LCG multiplier
    // 1148159575). MINIMAL OWNING SLICE (flagged): the full Random type is owned by a core TU; the
    // FSM only reads/writes its 32-bit state, so a single-word slice is carried BY NAME.
    struct Random
    {
        u32 muState;   // the LCG state word the freak-out direction coin-flip advances
    };

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

        // @0x82639380: prepare a traffic body. Asserts the event/attribs/wheel-position/wheel-radii
        // pointers are non-null (debug). Sums the four streamed wheel positions, folds 1/4 of the
        // sum into the attribs' COM offset, builds a local transform from the spawn event, forwards
        // into VehiclePhysics::Prepare + SetWheelVelocities, then seeds the freak-out fields (state
        // OFF, direction 0, time 0, owner id = *(event+8)). Bodied in TrafficPhysics.cpp.
        bool PreparePhysical(const CreatePhysicalTrafficEvent* lpEvent, VehicleAttribs* lpAttribs,
                             const AxisAlignedBox& lrAABB, const StreamedDeformationSpec* lpDeformSpec,
                             const Vector3* lpWheelPositions, const f32* lpafWheelRadii);

        // @0x82639590: the per-frame traffic update. PARTIAL (see file header). Bodied in
        // TrafficPhysics.cpp.
        virtual void Update(f32 lfTimeStep, f32 lfArg2, const Matrix44Affine* lpReferenceTransform,
                            const BrnPlayerDriverControls* lpControls, bool lbArg5, bool lbArg6,
                            bool lbArg7);

        // @ TrafficPhysics.cpp:360 -- enter the freak-out FSM (declare-only; bodied by its own TU).
        void SetFreakedOut(f32 lfDirection, f32 lfSeverity);

        bool IsFreakedOut() const { return mu8FreakOutState != E_FREAK_OUT_STATE_OFF; }

    private:
        EntityId mOwnerID;            // :117  (+5104)
        u8       mu8FreakOutState;    // :118  (+5108)
        f32      mfFreakOutDirection; // :119  (+5112)
        f32      mfFreakOutTime;      // :120  (+5116)
        Random   mRandom;             // :122  (+5152)
    };
}
}
