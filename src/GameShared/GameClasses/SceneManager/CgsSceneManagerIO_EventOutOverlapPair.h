#pragma once

// Minimal owning home for the SceneManager overlap-OUTPUT element
//   CgsSceneManager::SceneManagerIO::OutOverlapPair
// -- the per-event payload stored in the BaseEventQueue<OutOverlapPair> overlap-output
// queue the scene manager bridges overlap generation onto. This header exists so the
// explicit-instantiation TUs for that queue's AddEvent
// (BaseEventQueue_OutOverlapPair_AddEvent.cpp) and Append
// (BaseEventQueue_OutOverlapPair_Append.cpp) can see a COMPLETE element type (AddEvent/
// Append block-copy sizeof(T)-strided records).
//
// NOTE (distinct type): this 24-byte CgsSceneManager::SceneManagerIO::OutOverlapPair is a
// DIFFERENT type from the 16-byte CgsSceneManager::OverlappingPair (CgsOverlappingPair.h,
// BaseEventQueue<OverlappingPair>::AddEvent @ 0x828B8B08, stride 0x10). Do NOT conflate them.
//
// SIZE / ALIGNMENT (X360-attested):
//   * AddEvent @ 0x828AD390 indexes mpEvents[miLength] with `slwi r9,r11,1; add r11,r11,r9;
//     slwi r11,r11,3` (== miLength*24) and copies the element as three 8-byte qword stores
//     (std @ +0x00/+0x08/+0x10). Append @ 0x827A6FE8 strides its block-copy by the same 24.
//     So sizeof(OutOverlapPair) == 0x18 (24), 8-byte aligned (three qwords; no SIMD lane).
//
// LAYOUT: no field-level DWARF covers this element, and neither AddEvent nor Append reads the
// element interior -- but a CONSUMER now does: VehicleManager::StartVehicleContactGeneration
// @0x8262AEE8 (big-five #2 wave, 2026-08-06) decodes the leading two qwords as packed 64-bit
// volume-instance ids (`ld 0(rec)` / `ld 8(rec)`, then the standard entity-word geometry --
// owner at bits [56..63], 14-bit index at bit 10 of the high dword -- and the FULL qwords are
// forwarded to DeformationManager::AddRaceCar*Pair as CgsSceneManager::VolumeInstanceId values
// per the PS3 mangles). So the first two fields are PROMOTED to real members; the third qword
// stays opaque (nothing in scope reads it). ⚠ FLAG: the two NAMES follow the PotentialContact
// convention (muVolumeInstanceIdA/B) -- the types/geometry are asm-proven, the spellings are not
// (no field DWARF).

#include "types.hpp"
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"   // CgsSceneManager::VolumeInstanceId (8B, u64 muId)

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores
    // events by byte image). Distinctly named so this leaf element home never ODR-clashes
    // with the bases defined by the other per-element queue homes.
    struct EventBaseOutOverlapPair {};

    // ⭐ THE TAIL QWORD WAS NAMED 2026-08-19 (wave Q5 round 4, cluster F1 -- the scene
    // bridges). It is NOT opaque and it is NOT a qword: it is a single 32-bit float
    // followed by 4 bytes of tail padding, and cluster F1 landed the element's ONLY
    // producer, so the field is now attested from both ends:
    //   * DWARF, verbatim -- references/DecFIGS/dwarfdump/GameShared/GameClasses/
    //     SceneManager/CgsSceneManagerModuleIO.h:293
    //         struct CgsSceneManager::SceneManagerIO::OutOverlapPair : public Event {
    //             VolumeInstanceId mVolInstA;   // :295
    //             VolumeInstanceId mVolInstB;   // :296
    //             float32_t        mfPadding;   // :297
    //         }
    //   * X360, the producer -- SceneManagerModule::BridgeOverlapGenerationToOutputBuffer
    //     @0x828BA6A0 stages the record at sp+var_80 and writes exactly three fields:
    //         0x828BA808  std  r11, var_80      <- GetVolumeInstanceIdByIndex(pair.A)  +0x00
    //         0x828BA850  std  r11, var_78      <- GetVolumeInstanceIdByIndex(pair.B)  +0x08
    //         0x828BA844  lfs  f0,  <pair+0x08> ; 0x828BA848  stfs f0, var_70          +0x10
    //     `lfs`/`stfs` is a SINGLE-precision load/store: 4 bytes at +0x10, carrying
    //     OverlappingPair::mfPadding (the broad phase's squared centre separation) straight
    //     through. Bytes +0x14..+0x17 are never written -- they are the struct's tail
    //     padding to the 8-byte alignment the two ids force, which is why the console's
    //     stride stays 24.
    //
    // sizeof is UNCHANGED at 24 (the u64 tail and an f32 + 4 pad bytes occupy the same
    // span), so EventQueue_OverlappingPair_128_Construct.cpp:50's
    // `static_assert(sizeof(OutOverlapPair) == 24)` still holds, and no reader moves:
    // VehicleManager::StartVehicleContactGeneration @0x8262AEE8 only touches +0x00/+0x08.
    //
    // ⚠️ NAME DELTA REPORTED, NOT APPLIED: the DWARF spells the two id members `mVolInstA`
    // / `mVolInstB`, retiring this header's own "⚠ name inferred" flag on them. They are
    // NOT renamed here because VehicleManager reads them under the current spellings and
    // this file is not cluster F1's grant; see scratchpad/waveQ5/f1.owner.md.
    //
    // BaseEventQueue<OutOverlapPair> element. 8-byte aligned, X360-attested stride 24 (0x18).
    struct OutOverlapPair : public EventBaseOutOverlapPair
    {
        VolumeInstanceId muVolumeInstanceIdA;  // +0x00  packed id of overlap volume A (DWARF :295 mVolInstA)
        VolumeInstanceId muVolumeInstanceIdB;  // +0x08  packed id of overlap volume B (DWARF :296 mVolInstB)
        f32              mfPadding;            // +0x10  DWARF :297 (X360 `stfs f0` @0x828BA848)
    };
}
}
