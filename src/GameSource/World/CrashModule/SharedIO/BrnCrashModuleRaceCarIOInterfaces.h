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

namespace BrnWorld
{
namespace CrashIO
{
    struct alignas(16) RaceCarOutputInterface
    {
        unsigned char maReserved[256];
    };

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
}
}
