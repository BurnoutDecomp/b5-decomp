#pragma once

// BrnPhysics::Deformation::CarState — the per-vehicle deformation runtime state. This pass
// models only the slice the CarState::GetSensor accessor reaches (the one TU homed here);
// there is no DecFIGS DWARF for CarState, so members are pinned at their asm-attested
// CONSOLE byte offsets and FLAGGED as best-effort.
//
// X360 ground truth (CarState::GetSensor @ 0x825B3678, authoritative):
//   - reads the sensor count as a BYTE at this+0x6A4 (`lbz r11,0x6A4(r30)`)  -> mu8NumSensors
//   - asserts luSensorIndex < mu8NumSensors  ("luSensorIndex < mu8NumSensors",
//     baked d:\...\gamesource\physics\deformationmanager\sharedio\BrnDeformationState.h:62)
//   - returns the sensor record at this + 80 * luSensorIndex (`index*5<<4` == index*80),
//     i.e. &maSensors[luSensorIndex] with an 80-byte (0x50) per-sensor stride; the array
//     base is the CarState object itself (this+0).
//
// The 80-byte per-sensor element is an opaque runtime record here (no DWARF for its
// interior; the only TU reaching it is the sound-side DeformationEffect::UpdateParams,
// which is not in this group). It is modelled as an honestly-FLAGGED opaque 80-byte POD so
// the stride and the count's console offset are reproduced; GROW it into named members when
// a sensor-record-producing/consuming TU lands. mu8NumSensors is the authoritative live count.
//
// HOST-vs-X360 NOTE: the leading run up to the count is built from byte-exact-width opaque
// storage, so mu8NumSensors stays at its console offset (+0x6A4) on the host; the sensor
// element carries no host-widening members (pure byte storage), so its 80-byte stride holds.

#include "types.hpp"
#include "BrnCommonTypes.h"                                  // Vector3 (the CarState named tail)
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsBitArray.h"   // CgsContainers::BitArray

namespace BrnPhysics
{
namespace Deformation
{
    // One per-sensor runtime record. 80-byte (0x50) stride is asm-attested (CarState::
    // GetSensor index math). Interior recovered 2026-08-24 (deform-land wave) from the
    // writer DeformableObject::OutputState @0x825C1EA8 (the only producer): five 16-byte
    // rows per sensor, written at dst-32/dst-16/dst+0/dst+16/dst+32 around the +32 array
    // cursor (see BrnDeformableObject_GlassState.cpp's OutputState banner):
    //   +0x00  rest position          (sensor spec lead vector, "A")
    //   +0x10  world scalar vector    (sensor's mpWorldSpaceSphere lead row, "C")
    //   +0x20  displacement           (B - A: current minus rest)
    //   +0x30  displacement delta     ((B - A) minus the row's previous +0x20 value)
    //   +0x40  spec scalar (f32 at spec+40) + 12 bytes the writer leaves untouched
    // Names are role-derived from that writer (FLAG: no DWARF for the interior).
    struct alignas(16) CarSensorState
    {
        Vector3 mRestPosition;        // +0x00
        Vector3 mWorldScalarVector;   // +0x10
        Vector3 mDisplacement;        // +0x20 (current - rest)
        Vector3 mDisplacementDelta;   // +0x30 (change since last output)
        f32     mfSpecScalar;         // +0x40 (sensor spec +40)
        u8      maReserved[12];       // +0x44 .. +0x50 (writer leaves untouched)
    };

    struct alignas(16) CarState
    {
        // ⭐ CAPACITY SETTLED 2026-08-24 (deform-land wave): the old "bounding capacity 21"
        // guess is RETIRED. The producer (DeformableObject) owns maDeformationSensors[20]
        // and OutputState copies one 80-byte record per live sensor from +0; the named tail
        // below starts at exactly 20*80 == 1600 (the deformed-bbox copy source in
        // ActiveRaceCar::UpdateDeformationState @0x822D4A58 reads carState+0x640). So the
        // array is 20 records, 0..1600, with NO reserved gap.
        static const u32 KU_MAX_SENSORS = 20;

        // console +0x00 -- per-sensor record array (80-byte stride per sensor).
        CarSensorState maSensors[KU_MAX_SENSORS];

        // The named tail, offsets attested by BOTH sides of the seam (writer
        // DeformableObject::OutputState @0x825C1EA8; reader ActiveRaceCar::
        // UpdateDeformationState @0x822D4A58):
        //   +0x640 (1600)  deformed-bbox pair (32 bytes; OutputState fills it from
        //                  vehicle+1744, UpdateDeformationState copies it whole into
        //                  ActiveRaceCar::mDeformedBBox)
        //   +0x660 (1632)  the 4 wheel tag-point rows (16-byte stride; OutputState's
        //                  tag loop, UpdateDeformationState -> RenderParams axle rows)
        //   +0x6A0 (1696)  summed squared sensor displacement (OutputState's vaddfp
        //                  accumulator; UpdateDeformationState -> mfDeformationSquared)
        //   +0x6A4 (1700)  live sensor count (GetSensor @0x825B3678 lbz's this byte)
        Vector3 mDeformedBBoxMin;         // +0x640 (1600)
        Vector3 mDeformedBBoxMax;         // +0x650 (1616)
        Vector3 maWheelTagPoints[4];      // +0x660 (1632) .. +0x6A0 (1696)
        f32     mfSummedDisplacementSquared; // +0x6A0 (1696)
        u8      mu8NumSensors;            // +0x6A4 (1700) -- live sensor count
        u8      maTailPad[11];            // +0x6A5 .. +0x6B0 (sizeof == 1712 == the
                                          //  DeformationState per-record stride)

        // 0x825B3678 -- checked sensor accessor. Asserts luSensorIndex < mu8NumSensors,
        // then returns the sensor record at maSensors[luSensorIndex].
        CarSensorState& GetSensor(u8 luSensorIndex);

        // Console-inline reader accessors (no standalone X360 emission -- the one reader,
        // ActiveRaceCar::UpdateDeformationState @0x822D4A58, emits bare offset loads; the
        // wheel-tag read carries the baked assert "luWheel < BrnPhysics::Vehicle::
        // eNumDrivenWheels", BrnDeformationState.h:75 -- THIS header's own line on console).
        f32 GetSummedDisplacementSquared() const { return mfSummedDisplacementSquared; }
        const Vector3& GetWheelTagPoint(u32 luWheel) const
        {
            CGS_ASSERT(luWheel < 4u, "luWheel < BrnPhysics::Vehicle::eNumDrivenWheels");
            return maWheelTagPoints[luWheel];
        }
    };

    // ========================================================================
    // BrnPhysics::Deformation::DeformationState -- the deformation manager's shared-IO state:
    // a fixed pool of up to KU_MAX_DEFORMATION_MODELS per-car deformation records, a parallel
    // table of the car id each live slot belongs to, and a BitArray marking which slots are
    // live. DWARF home BrnDeformationState.h (same file as CarState). The slice modelled here
    // is the one GetCarStateF reaches; the per-car record interior is opaque (no DWARF; the
    // only TU reaching this state is the lookup below, which only takes a record's address).
    //
    // X360 ground truth (DeformationState::GetCarStateF @ 0x822CC340, authoritative):
    //   - the per-car record array base is the DeformationState object itself (this+0), with a
    //     1712-byte (0x6B0) per-record stride (`mulli r11,liCarLoop,0x6B0; add r3,r11,this`).
    //   - the per-slot car-id table is at this + 47936 (0xBB40): the asm loads the candidate id
    //     as a 32-bit word at `4*(liCarLoop + 11984) + this` == this + 47936 + 4*liCarLoop
    //     (`addi r11,liCarLoop,0x2ED0; slwi r11,r11,2; lwzx r11,r11,this`; 0x2ED0 == 11984).
    //   - the live-slot BitArray is at this + 48048 (0xBBB0) -- immediately after the 28-entry
    //     car-id table (47936 + 28*4 == 48048). The asm forms the base as
    //     `addis r19,this,1; addi r19,r19,-0x4450` (this + 0x10000 - 0x4450 == this + 0xBBB0)
    //     and only ever indexes field 0 (it caps `bitIndex>>6 < 1`), confirming BitArray<28>
    //     occupies a single 64-bit field.
    //   - the loop variable is asserted `< (int32_t)KU_MAX_DEFORMATION_MODELS` (baked
    //     BrnDeformationState.h:109), pinning the pool capacity at 28.
    //
    // LAYOUT (X360 offsets, authoritative):
    //   +0      CarStateRecord maCarStates[28]   (1712-byte stride per car)         (47936 bytes)
    //   +47936  u32            maCarIds[28]      (the car id owning each live slot)  (112 bytes)
    //   +48048  BitArray<28>   mxLiveSlots       (which slots are live)             (8 bytes)
    //
    // ⭐ PROMOTED 2026-08-24 (deform-land wave): the per-car record IS CarState -- the
    // record-producing TU (DeformationManager::OutputSensorState @0x82605618 hands
    // &maCarStates[i] to DeformableObject::OutputState @0x825C1EA8, which fills a CarState)
    // landed, closing the old "opaque 1712-byte POD" deferral. sizeof(CarState) == 1712 is
    // static_asserted in BrnDeformationState_DeformationState.cpp, so every derived table
    // offset below is unchanged. CarStateRecord survives as a typedef for the existing
    // consumers (GetCarStateF's return spelling, BrnVehicleManager_PerFrameLeaves.cpp).
    static const u32 KU_MAX_DEFORMATION_MODELS = 28;

    typedef CarState CarStateRecord;

    struct DeformationState
    {
        // 0x822CC340 -- look up the live deformation record owning car id luCarId. Scans the
        // live-slot BitArray; for each live slot whose maCarIds[slot] equals luCarId, returns
        // &maCarStates[slot]. Returns nullptr when no live slot owns that car id. Each visited
        // slot index is asserted < KU_MAX_DEFORMATION_MODELS (a non-gating tripwire).
        // ⭐ CONST-CORRECTED 2026-08-06 (PhysicsModule::Update leaves wave): VehicleManager::
        // ProcessDeformationStates @0x825EA580 calls this through DeformationOutputInterface::
        // mpDeformationState, a `const DeformationState*` (DWARF), and the DWARF's sibling
        // getter GetCarStateFromEntityId is declared const -- so this lookup is const. The
        // record pointer it returns is handed to RaceCarPhysics::UpdateShowtimeBounceModifiers,
        // which takes it as `const void*`, so a const return loses nothing.
        const CarStateRecord* GetCarStateF(u32 luCarId) const;

        static void _AssertLayout();

    private:
        // ⭐ The one WRITER (DeformationManager::OutputSensorState @0x82605618, landed
        // 2026-08-24) fills all three members in place each frame: the live-slot mask is a
        // raw word copy of the manager's mModelsAdded (0x82605644 `ld/std`), then per live
        // slot maCarIds[i] and the &maCarStates[i] handed to DeformableObject::OutputState.
        // The manager owns this object BY VALUE (mStateOutput) and the console emits the
        // writes as bare stores, so the owning manager is a friend rather than growing a
        // public mutator surface no console symbol attests.
        friend class DeformationManager;

        CarStateRecord                      maCarStates[KU_MAX_DEFORMATION_MODELS]; // +0      (1712 stride)
        u32                                 maCarIds[KU_MAX_DEFORMATION_MODELS];    // +47936
        CgsContainers::BitArray<KU_MAX_DEFORMATION_MODELS> mxLiveSlots;             // +48048
    };
}
}
