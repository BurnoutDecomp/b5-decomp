// FX-SCENEMGR (crash parity 2026-09-24, H-SM1): CgsSceneManager::SceneManagerIO::InSceneUpdateInterface::
// ReplaceDynamicVolume (ARTIST 0x822B15F8; DWARF CgsSceneManagerIO_SceneUpdate.h:458
// `void ReplaceDynamicVolume(VolumeId, const VolRef::Volume *)`), extracted VERBATIM from
// CgsSceneManagerIO_SceneUpdate.cpp by run_fxscenemgr_replace_dynamic_volume.py. The console body:
//   0x822B1604 mr r11, r4 ; 0x822B1618 std r11 -> event +0x00      the WHOLE 64-bit key
//   0x822B161C memcpy(event +0x10, r5, 0x80)                       the 128-byte volume image
//   0x822B1630..3C miLength >= miMaxLength -> "SceneManager.mReplaceDynamicVolumeQueue too small, ..."
//                                                                  (a tripwire: it does not gate)
//   0x822B16C4 BaseEventQueue<InEventReplaceDynamicVolume>::AddEvent(this + 0xB1D30, &event)
// Its only console caller, RaceCarEntityModule::UpdatePropBoundingBoxes_PreScene @0x822F5668, hands it a
// race car's mHandlingBodyVolumeId (`ld r11, 0xD0`): entity word in the HIGH dword, low dword 0
// (ActiveRaceCar::Attach: muId = 0, SetEntityIDOwner(E_ENTITYTYPE_RACECAR), SetEntityIDEntityIndex(slot)).
// The pre-fix tree has only a fitted 32-bit EntityId form; a caller holding the 64-bit handle reaches it
// only through the implicit u64 -> u32 conversion, which is what the RED side replays.
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"
#include "GameShared/GameClasses/SceneManager/CgsVolumeId.h"
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventReplaceDynamicVolume.h"
#include <cstdio>
#include <cstring>

static unsigned guAssertions = 0;
static unsigned guTooSmall = 0;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int)
{
    ++guAssertions;
    if (lpcMessage && std::strstr(lpcMessage, "SceneManager.mReplaceDynamicVolumeQueue too small")) ++guTooSmall;
    return 0;
}
void* EndAssert() { return nullptr; }
}
}

namespace CgsSceneManager {
namespace SceneManagerIO {
struct InSceneUpdateInterface
{
    // DWARF :458 (the fix) and the pre-fix tree's fitted 32-bit form. The runner pastes whichever body
    // the source file has; the other declaration is never called (FXSM_RDV_WIDE selects the call).
    void ReplaceDynamicVolume(VolumeId lVolumeId, const void* lpVolumeImage);
    void ReplaceDynamicVolume(CgsSceneManager::EntityId lEntityId, const void* lpVolumeImage);

    CgsModule::EventQueue<InEventReplaceDynamicVolume, 64> mReplaceDynamicVolumeQueue;   // X360 +0xB1D30
};
#include "fxsm_rdv_body.inc"
}
}

#include "fxsm_rdv_form.inc"   // FXSM_RDV_WIDE, gbHeaderWide, gbHeaderNoNarrow (computed by the runner)

using CgsSceneManager::SceneManagerIO::InEventReplaceDynamicVolume;
using CgsSceneManager::SceneManagerIO::InSceneUpdateInterface;

// A caller holding the whole 64-bit handle, as UpdatePropBoundingBoxes_PreScene does.
static void Post(InSceneUpdateInterface& lrScene, u64 luKey, const void* lpImage)
{
#if FXSM_RDV_WIDE
    lrScene.ReplaceDynamicVolume(CgsSceneManager::VolumeId(luKey), lpImage);
#else
    lrScene.ReplaceDynamicVolume(CgsSceneManager::EntityId(static_cast<u32>(luKey)), lpImage);
#endif
}

// ActiveRaceCar::Attach's seed of mHandlingBodyVolumeId (BrnActiveRaceCar.cpp, 0x822BEF04..): the real
// VolumeInstanceId setters (CgsVolumeInstanceId.cpp, 0x822B0E00 / 0x822B0E70), compiled alongside.
static u64 RaceCarHandle(u32 luSlot)
{
    CgsSceneManager::VolumeInstanceId lId;
    lId.muId = 0;
    lId.SetEntityIDOwner(1u);            // BrnWorld::E_ENTITYTYPE_RACECAR (BrnEntityTypes.h:34)
    lId.SetEntityIDEntityIndex(luSlot);
    return lId.muId;
}

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}

int main()
{
    static u8 saImage[128];
    for (int i = 0; i < 128; ++i) saImage[i] = static_cast<u8>(0xA5 ^ (i * 7));

    // ---- a race car's handle: entity word high, low dword 0 ------------------------------------------
    {
        static InSceneUpdateInterface sScene;
        sScene.mReplaceDynamicVolumeQueue.Construct();
        const u64 luKey = RaceCarHandle(3u);
        Check((luKey >> 32) != 0u && static_cast<u32>(luKey) == 0u,
              "fixture: a race car's handle has its entity word in the HIGH dword and a zero low dword");
        Post(sScene, luKey, saImage);
        const InEventReplaceDynamicVolume& lrEvent = sScene.mReplaceDynamicVolumeQueue.GetEvent(0);
        std::printf("race-car slot 3 handle %08X:%08X -> posted %08X:%08X\n",
                    static_cast<u32>(luKey >> 32), static_cast<u32>(luKey),
                    static_cast<u32>(lrEvent.muId >> 32), static_cast<u32>(lrEvent.muId));
        Check(lrEvent.muId == luKey,
              "the race car's WHOLE handle reaches the record (std r11 = r4 @0x822B1618), not key 0");
        Check(std::memcmp(lrEvent.maVolumeData, saImage, 128) == 0,
              "the 128-byte volume image is block-copied to record +0x10 (memcpy 0x80 @0x822B161C)");
        Check(sScene.mReplaceDynamicVolumeQueue.GetLength() == 1, "one record appended per call");
    }

    // ---- the eight race cars: eight distinct whole keys, in call order -------------------------------
    {
        static InSceneUpdateInterface sScene;
        sScene.mReplaceDynamicVolumeQueue.Construct();
        for (u32 luSlot = 0; luSlot < 8u; ++luSlot) Post(sScene, RaceCarHandle(luSlot), saImage);
        bool lbWhole = sScene.mReplaceDynamicVolumeQueue.GetLength() == 8;
        for (s32 i = 0; lbWhole && i < 8; ++i)
            lbWhole = sScene.mReplaceDynamicVolumeQueue.GetEvent(i).muId == RaceCarHandle(static_cast<u32>(i));
        bool lbDistinct = sScene.mReplaceDynamicVolumeQueue.GetLength() == 8;
        for (s32 i = 0; lbDistinct && i < 8; ++i)
            for (s32 j = i + 1; lbDistinct && j < 8; ++j)
                lbDistinct = sScene.mReplaceDynamicVolumeQueue.GetEvent(i).muId
                          != sScene.mReplaceDynamicVolumeQueue.GetEvent(j).muId;
        Check(lbWhole, "each of the eight race cars posts its own whole handle, in call order");
        Check(lbDistinct, "the eight posts carry eight distinct keys (the 32-bit form collapses them all to 0)");
    }

    // ---- both dwords populated: nothing is dropped from either half ----------------------------------
    {
        static InSceneUpdateInterface sScene;
        sScene.mReplaceDynamicVolumeQueue.Construct();
        const u64 luKey = 0x0123456789ABCDEFull;
        Post(sScene, luKey, saImage);
        Check(sScene.mReplaceDynamicVolumeQueue.GetEvent(0).muId == luKey,
              "a key with both dwords set is stored whole (64-bit mr/std, no truncation)");
    }

    // ---- the capacity tripwire does not gate the append (blt skips only the assert block) -----------
    {
        static InSceneUpdateInterface sScene;
        static InEventReplaceDynamicVolume saBuffer[2];
        sScene.mReplaceDynamicVolumeQueue.CgsModule::BaseEventQueue<InEventReplaceDynamicVolume>::Construct(saBuffer, 1);
        guTooSmall = 0;
        Post(sScene, RaceCarHandle(1u), saImage);
        const unsigned luAfterFirst = guTooSmall;
        Post(sScene, RaceCarHandle(2u), saImage);
        Check(luAfterFirst == 0u && guTooSmall == 1u,
              "the 'mReplaceDynamicVolumeQueue too small' tripwire fires only when miLength >= miMaxLength");
        Check(sScene.mReplaceDynamicVolumeQueue.GetLength() == 2
              && std::memcmp(saBuffer[1].maVolumeData, saImage, 128) == 0,
              "and the record is still appended (AddEvent @0x822B16C4 is unconditional)");
    }

    Check(gbHeaderWide, "the header declares ReplaceDynamicVolume(VolumeId, ...) (DWARF CgsSceneManagerIO_SceneUpdate.h:458)");
    Check(gbHeaderNoNarrow, "the header has no 32-bit EntityId form (the DWARF has none; no console caller)");

    std::printf("FxScenemgrReplaceDynamicVolume: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
