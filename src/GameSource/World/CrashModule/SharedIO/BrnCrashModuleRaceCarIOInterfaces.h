#pragma once

// MINIMAL SLICE for the RaceCarEntityModuleIO IO-buffer unlock; full layout reconstructed
// by RaceCarOutputInterface's own TU (DWARF home GameSource/World/CrashModule/SharedIO/
// BrnCrashModuleRaceCarIOInterfaces.h). Size 256 (NOMINAL).
//
// Per the DecFIGS DWARF, BrnWorld::CrashIO::RaceCarOutputInterface holds an
// EventQueue<RaceCarCrashCompleteEvent,10> (mRaceCarCrashCompleteEventQueue). The
// RaceCarEntityModuleIO InputBuffer_PostScene references it via its buffer-local typedef
// `CrashInterface` (BrnRaceCarEntityModuleIO.h:92/:200) and only holds/returns it by
// pointer, so a reserved-byte blob suffices here; the full member layout (and the sibling
// CrashIO Traffic/Network interfaces homed in this DWARF file) belongs to this type's own
// ledger TU. alignas(16) for the SIMD-aligned EventQueue payload.
#include "types.hpp"   // u8
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"   // CgsSceneManager::VolumeInstanceId
#include "GameShared/GameClasses/Module/CgsEventQueue.h"               // CgsModule::EventQueue<T,N>

namespace BrnWorld
{
namespace CrashIO
{
    // Forward-declared here so the interface below can name its queue; defined right after.
    struct RaceCarCrashCompleteEvent;

    // RaceCar-output event: a race-car crash has finished resolving. 16 bytes. The producer
    // (CrashModule::ResetRaceCarFromCrashIndex @0x827C6C40) copies the crashed car's
    // VolumeInstanceId (the qword RaceCarCrash item) into +0 and a one-byte recycle/remove flag
    // (the `char a4` parameter) into +8, then forwards a 16-byte element to AddEvent; the X360
    // AddEvent (@0x827C2AC8) copies the element as two 8-byte stores at a 16-byte stride. The
    // queue base subobject pads 12->16 (8-byte element alignment), giving the maEvents offset
    // 0x10 the EventQueue<...,10>::Construct attests (@0x822E3600).
    struct RaceCarCrashCompleteEvent
    {
        CgsSceneManager::VolumeInstanceId mRaceCarVolumeInstanceId;   // +0x00 (the RaceCarCrash item)
        bool                              mbRemoveRaceCar;            // +0x08 (the `char a4` flag)
    };

    // ⭐ [crash exit 2026-08-25] PROMOTED from `unsigned char maReserved[256]` (a NOMINAL blob) to
    // the real queue. This is the record that carries "race car N has finished crashing" out of
    // the crash module, and while it was opaque the whole crash-exit path was unreachable by name.
    //
    // THE QUEUE IS AT OFFSET 0, and that is asm-attested rather than assumed:
    // WorldModule::BridgeCrashModuleToPropModule_PostScene @0x827AAD78 calls
    // OutputBuffer_PreScene::GetRaceCarOutputInterface (which returns the INTERFACE pointer) and
    // passes that pointer UNADJUSTED as the source argument of
    // EventQueue<RaceCarCrashCompleteEvent,10>::Append @0x827A7D70 -- no `addi` between the two
    // calls. A queue at any non-zero offset would need one. (Contrast the destination side in the
    // same body: `addi r3, r31, 8` before the call, because the prop buffer's queue really is at
    // +8 behind its IOBuffer status byte.)
    // The element type and depth are the DecFIGS DWARF's (an
    // EventQueue<RaceCarCrashCompleteEvent,10> named mRaceCarCrashCompleteEventQueue), and they
    // match the queue the prop side already spells out for itself in BrnPropEntityModuleIO.h:1119.
    //
    // ⚠️ The old 256-byte NOMINAL size was a guess and is now gone: the real host size is
    // 16 + 10*16 == 176 (12 + 10*16 == 172 on the console, mpEvents widening 4->8) -- the same
    // arithmetic the prop buffer's own banner records for the identical queue. alignas(16) is
    // kept: the EventQueue payload is SIMD-aligned.
    struct alignas(16) RaceCarOutputInterface
    {
        typedef CgsModule::EventQueue<RaceCarCrashCompleteEvent, 10> RaceCarCrashCompleteEventQueue;

        const RaceCarCrashCompleteEventQueue* GetRaceCarCrashCompleteEventQueue() const
        {
            return &mRaceCarCrashCompleteEventQueue;
        }
        RaceCarCrashCompleteEventQueue* GetRaceCarCrashCompleteEventQueue()
        {
            return &mRaceCarCrashCompleteEventQueue;
        }

    private:
        RaceCarCrashCompleteEventQueue mRaceCarCrashCompleteEventQueue;   // +0x00 (see the banner)
    };
}
}
