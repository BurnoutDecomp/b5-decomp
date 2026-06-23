#ifndef BRN_SOUND_VEHICLES_ENGINES_BRN_ENGINES_H
#define BRN_SOUND_VEHICLES_ENGINES_BRN_ENGINES_H

#include "types.hpp"

// =============================================================================
// BrnSound::Vehicles::Engines  free/namespace-scope helpers
//   GameSource/Sound/Vehicles/Engines/BrnEngines.h (home) +
//   GameSource/Sound/Vehicles/Engines/BrnEngines.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. This home models the slice of the
// engine-sound support helpers exercised by the start-line-reving update path:
//   BoostEf  @ 0x826852E8 -- returns the address of the boost-effect sub-object.
//   Physic   @ 0x82684510 -- pseudo-random pick of one engine "physic" sample
//                            record from a fixed 6-entry table, advancing a 64-bit
//                            LCG seed that lives in the logic module.
//   PhysicsC @ 0x826C8708 -- evaluate a 3-channel engine envelope at the current
//                            (dt-advanced) time and write {time, lerp(ch2), lerp(ch1)}.
//
// These are namespace-scope functions in the X360 image (BrnSound::Vehicles::
// Engines::BoostEf / ::Physic / ::PhysicsC), not members of one class. The
// parameter objects (the PhysicsControl that owns the seed, the boost-effect owner)
// are large un-homed engine types; only the named fields these functions touch are
// modelled here, BY NAME, on minimal local structs. The absolute X360 offsets
// (+112, +79280, the +604 envelope) assume 32-bit pointers and are documented but
// NOT asserted across the 32/64 boundary.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

// --- Physic: the sampled "physic" record and its fixed table -----------------

// One engine-physic sample record. 16 bytes (the X360 copies 4 dwords from the
// table entry into the result). The four fields are stored verbatim; their
// engine meaning (the rodata table @ unk_82F2F508 is not in the function export)
// is not recoverable, so they are modelled as raw 32-bit words by name.
struct PhysicValue
{
    u32 mau32Word[4]; // +0,+4,+8,+0xC -- the 16 bytes copied from the table entry
};

// Number of entries in the physic sample table (the LCG output is reduced mod 6).
const u32 KU_NUM_PHYSIC_SAMPLES = 6u;

// FLAG (UNRECOVERABLE RODATA -> honest placeholder): the X360 indexes a fixed
// 6 x 16-byte table at unk_82F2F508 to pick a sample. That table's bytes are NOT
// present in the .ida-exports (it is rodata, not a function export), so its values
// cannot be reconstructed. This array is therefore a zero-filled PLACEHOLDER: the
// Physic control flow (LCG advance + mod-6 index + 16-byte copy) is reproduced
// faithfully, but the picked values are zeros, NOT the real table. Do not treat
// these as X360 fact. Defined in BrnEngines.cpp.
extern const PhysicValue gakPhysicSampleTable[KU_NUM_PHYSIC_SAMPLES];

// Minimal carrier of the 64-bit LCG seed Physic advances. The X360 reaches the
// seed via control->mpLogicModule (+44) then a deep field at +79280; that whole
// type is un-homed. Here the function takes the seed BY NAME through this carrier
// so no raw-offset member access is needed.
struct PhysicSeedHolder
{
    u64 mu64LcgSeed; // the 64-bit linear-congruential PRNG state
};

// 0x82684510. Advance the LCG seed in arSeed (seed = seed*0x5851F42D4C957F2D + 1),
// reduce the *previous* seed's high 32 bits modulo KU_NUM_PHYSIC_SAMPLES, and copy
// the selected table record into arResult. Returns &arResult.
PhysicValue* Physic(PhysicValue& arResult, PhysicSeedHolder& arSeed);

// --- PhysicsC: 3-channel engine envelope evaluation --------------------------

// One envelope keyframe: three float channels. ch0 is the time axis; ch1 and ch2
// are the interpolated outputs. 12 bytes (the X360 reads +0,+4,+8 per point and
// strides 12 between points).
struct EngineEnvelopePoint
{
    f32 mfCh0; // +0  time-axis value at this keyframe
    f32 mfCh1; // +4  output channel 1
    f32 mfCh2; // +8  output channel 2
};

// The envelope descriptor PhysicsC evaluates (the X360 passes control + 0x25C).
// Reads miNumPoints (+0), mpPoints (+4), mfTime (+8), miIndex (+0xC).
struct EngineEnvelope
{
    s32                  miNumPoints; // +0  number of keyframe points
    EngineEnvelopePoint* mpPoints;    // +4  keyframe array
    f32                  mfTime;      // +8  accumulated playhead time
    s32                  miIndex;     // +0xC current segment index
};

// The value PhysicsC writes: {time, lerp(ch2), lerp(ch1)} = 12 bytes. The trailing
// +0xC..+0x14 of the caller's buffer (it reserves 16) is not written here.
struct EngineEnvelopeValue
{
    f32 mfTime; // +0  the (dt-advanced) playhead time
    f32 mfOut1; // +4  ch2 interpolated output
    f32 mfOut2; // +8  ch1 interpolated output
};

// 0x826C8708. Advance arEnvelope.mfTime by afDeltaTime, find the current segment
// fraction, lerp the two output channels, and write the result. When the envelope
// is empty or the playhead is past the end, snaps to the boundary keyframe.
EngineEnvelopeValue* PhysicsC(EngineEnvelopeValue& arResult,
                              EngineEnvelope&      arEnvelope,
                              f32                  afDeltaTime);

// --- BoostEf -----------------------------------------------------------------

// Owner block whose boost-effect sub-object begins at +112 (X360 +0x70). The full
// boost-effect type is un-homed; here the sub-object is modelled as a named byte
// span so BoostEf can return its address BY NAME at the right place rather than by
// raw pointer arithmetic. (The X360 simply returns base + 0x70.)
struct EnginesBoostOwner
{
    u8 mau8Header[112]; // +0    opaque header preceding the boost sub-object
    u8 mau8BoostEffect[1]; // +112 start of the boost-effect sub-object (X360 +0x70)
};

// 0x826852E8. Return the address of the owner's boost-effect sub-object.
void* BoostEf(EnginesBoostOwner& arOwner);

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENGINES_BRN_ENGINES_H
