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
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsBitArray.h"   // CgsContainers::BitArray

namespace BrnPhysics
{
namespace Deformation
{
    // One per-sensor runtime record. FLAG (opaque element): 80-byte (0x50) stride is
    // asm-attested (CarState::GetSensor index math); its interior is not recovered in this
    // pass. GROW into real members when a sensor-record TU lands.
    struct CarSensorState
    {
        u8 maOpaque[0x50];   // 80 bytes (asm-attested per-sensor stride)
    };

    struct CarState
    {
        // FLAG (best-effort capacity): the sensor array base is this+0 and the next field
        // reached by asm (mu8NumSensors) is console +0x6A4 (1700). At an 80-byte stride that
        // bounds the array at floor(1700/80) == 21 records plus a trailing opaque span; no
        // asm/DWARF pins the exact maximum, so the bounding capacity is used and the count
        // byte is held at its console offset by a trailing reserved span. mu8NumSensors is
        // the authoritative live count.
        static const u32 KU_MAX_SENSORS = 21;

        // console +0x00 -- per-sensor record array (80-byte stride per sensor).
        CarSensorState maSensors[KU_MAX_SENSORS];

        // Opaque span between the sensor array end (+0x690) and the count (+0x6A4).
        u8 maReserved0[0x6A4 - KU_MAX_SENSORS * 0x50];

        u8 mu8NumSensors;   // console +0x6A4 (1700) -- live sensor count

        // 0x825B3678 -- checked sensor accessor. Asserts luSensorIndex < mu8NumSensors,
        // then returns the sensor record at maSensors[luSensorIndex].
        CarSensorState& GetSensor(u8 luSensorIndex);
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
    // FLAG (opaque element): the 1712-byte CarStateRecord interior is not recovered by this
    // slice (no DWARF; GetCarStateF only takes a record's address). It is modelled as an
    // honestly-FLAGGED opaque 1712-byte POD so the per-record stride and the derived table
    // offsets are exact; GROW it into named members when a record-producing/consuming TU lands.
    static const u32 KU_MAX_DEFORMATION_MODELS = 28;

    // One per-car deformation record. FLAG (opaque element): 1712-byte (0x6B0) stride is
    // asm-attested (GetCarStateF return math); its interior is not recovered in this pass.
    struct CarStateRecord
    {
        u8 maOpaque[0x6B0];   // 1712 bytes (asm-attested per-record stride)
    };

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
        CarStateRecord                      maCarStates[KU_MAX_DEFORMATION_MODELS]; // +0      (1712 stride)
        u32                                 maCarIds[KU_MAX_DEFORMATION_MODELS];    // +47936
        CgsContainers::BitArray<KU_MAX_DEFORMATION_MODELS> mxLiveSlots;             // +48048
    };
}
}
