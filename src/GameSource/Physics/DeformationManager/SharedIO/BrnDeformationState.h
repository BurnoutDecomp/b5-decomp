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
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

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
}
}
