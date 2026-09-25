// FX-FOLLOWUPS (crash parity 2026-09-25, REVIEW-K on b5 35a5e66c): the deepest-volume query event
// CgsSceneManager::SceneManagerIO::InEventVolumeTestDeepest carries its DWARF members (DecFIGS
// CgsSceneManagerIO_FineQuery.h:101-109) beside the opaque 224-byte payload, and the bytes its producer writes are
// unchanged.
// run_fxfollowups_volume_test_deepest_event.py compiles the revision's event header (shadowed in) and the revision's
// producer SceneQueryInterface::VolumeTestDeepest @0x822170B0 against a recording queue. With --hunk it also compiles a
// by-name rewrite of the producer's stores, renamed ByNameSceneQueryInterface, and compares the two byte images.
//
// The console producer @0x822170B0 builds the event at r1+0x50 (r1+0x180+var_130) and queues it:
//   +0x00..+0x3F  stvx128 @0x82217150 / 0x82217170 / 0x82217180 / 0x8221718C   the four rows of a6 (lvx128 r31)
//   +0x40         stw r27 (a2) @0x82217154          +0x44  stw r26 (a3) @0x82217160
//   +0x48         stw r23 (a7) @0x82217188          +0x4C  stw r30 (a8) @0x82217190
//   +0x50..+0xCF  memcpy(event+0x50, a5, 0x80) @0x82217194
//   +0xD0         stb r25 (a4) @0x82217178
//   then AddEvent(this->+0x10, &event) @0x822171A0. The bytes after +0xD0 are never written (stack garbage).
#include "types.hpp"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <vector>

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventVolumeTestDeepest.h"   // the revision's header
#include "fxfu_vtde_config.inc"                                                             // FXFU_VTDE_BY_NAME

static int giAsserts = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++giAsserts; return 0; }
void* EndAssert() { return nullptr; }
} }

namespace CgsSceneManager { namespace SceneManagerIO {
    enum EExclusionMode { E_EXCLUDE_ENTITY_ONLY = 0, E_EXCLUDE_ALL_CHILD_PARTS = 1 };

    // AddEvent's view: the element as it was handed over, copied whole.
    struct RecordingQueue
    {
        std::vector<InEventVolumeTestDeepest> maEvents;
        bool AddEvent(const InEventVolumeTestDeepest& lrEvent) { maEvents.push_back(lrEvent); return true; }
    };

    class SceneQueryInterface
    {
    public:
        int VolumeTestDeepest(u32 lQueryId, u32 lx32EntityTypeFlags, u8 lxVolumeTypeFlags, const void* lpVolumeData,
                              const void* lpTransform, u32 lExcludeEntityId, EExclusionMode leExclusionMode);
        RecordingQueue* mpFineVolumeTestDeepestQueue = nullptr;
    };
#if FXFU_VTDE_BY_NAME
    class ByNameSceneQueryInterface
    {
    public:
        int VolumeTestDeepest(u32 lQueryId, u32 lx32EntityTypeFlags, u8 lxVolumeTypeFlags, const void* lpVolumeData,
                              const void* lpTransform, u32 lExcludeEntityId, EExclusionMode leExclusionMode);
        RecordingQueue* mpFineVolumeTestDeepestQueue = nullptr;
    };
#endif
} }

#include "fxfu_vtde_producer.inc"   // SceneQueryInterface::VolumeTestDeepest from the revision
#if FXFU_VTDE_BY_NAME
#include "fxfu_vtde_by_name.inc"    // the same body with the --hunk applied, as ByNameSceneQueryInterface
#endif

using namespace CgsSceneManager::SceneManagerIO;

static unsigned guChecks = 0, guFailures = 0;
static void Check(bool lbPass, const char* lpcLabel)
{
    ++guChecks;
    if (!lbPass) ++guFailures;
    std::printf("%s  %s\n", lbPass ? "PASS" : "FAIL", lpcLabel);
}

// One producer call's inputs: distinctive bytes everywhere, so a byte in the wrong place cannot pass.
struct Inputs
{
    u32 muQueryId, mx32Flags, muExclude;
    u8 mxVolumeTypeFlags;
    EExclusionMode meMode;
    alignas(16) u8 macTransform[64];
    alignas(16) u8 macVolume[128];
};

static Inputs MakeInputs(u32 luSeed, EExclusionMode leMode, u8 lxTypeFlags)
{
    Inputs l;
    l.muQueryId = 0x00050000u | (luSeed * 0x0101u);
    l.mx32Flags = 0x1Eu ^ (luSeed << 8);
    l.muExclude = 0x02001405u + luSeed * 0x00100001u;
    l.mxVolumeTypeFlags = lxTypeFlags;
    l.meMode = leMode;
    for (u32 lu = 0; lu < 64; ++lu) l.macTransform[lu] = static_cast<u8>(0x11 * luSeed + 3 * lu + 1);
    for (u32 lu = 0; lu < 128; ++lu) l.macVolume[lu] = static_cast<u8>(0xA0 + 7 * luSeed + lu);
    return l;
}

// The console's stores, byte for byte (the listing above): the recipe the queued element must equal on 0x00..0xD0.
static void ConsoleImage(const Inputs& lrIn, u8 (&lacOut)[0xD1])
{
    std::memset(lacOut, 0, sizeof(lacOut));
    std::memcpy(lacOut + 0x00, lrIn.macTransform, 64);                     // stvx128 x4
    std::memcpy(lacOut + 0x40, &lrIn.muQueryId, 4);                         // stw r27
    std::memcpy(lacOut + 0x44, &lrIn.mx32Flags, 4);                         // stw r26
    std::memcpy(lacOut + 0x48, &lrIn.muExclude, 4);                         // stw r23
    const u32 luMode = static_cast<u32>(lrIn.meMode);
    std::memcpy(lacOut + 0x4C, &luMode, 4);                                 // stw r30
    std::memcpy(lacOut + 0x50, lrIn.macVolume, 128);                        // memcpy 0x80
    lacOut[0xD0] = lrIn.mxVolumeTypeFlags;                                  // stb r25
}

template <typename TInterface>
static bool Produce(const Inputs& lrIn, InEventVolumeTestDeepest& lrOut, int& riReturn, int& riAsserts)
{
    RecordingQueue lQueue;
    TInterface lInterface;
    lInterface.mpFineVolumeTestDeepestQueue = &lQueue;
    const int liBefore = giAsserts;
    riReturn = lInterface.VolumeTestDeepest(lrIn.muQueryId, lrIn.mx32Flags, lrIn.mxVolumeTypeFlags, lrIn.macVolume,
                                            lrIn.macTransform, lrIn.muExclude, lrIn.meMode);
    riAsserts = giAsserts - liBefore;
    if (lQueue.maEvents.size() != 1) return false;
    lrOut = lQueue.maEvents[0];
    return true;
}

int main()
{
    // ---- N. the named members sit at the producer's store offsets ------------------------------------------------
    Check(offsetof(InEventVolumeTestDeepest, mTransform) == 0x00,
          "N1 mTransform (DWARF :103, Matrix44Affine's four rows) at +0x00 -- stvx128 @0x82217150..0x8221718C");
    Check(offsetof(InEventVolumeTestDeepest, mQueryId) == 0x40, "N2 mQueryId (:104) at +0x40 -- stw @0x82217154");
    Check(offsetof(InEventVolumeTestDeepest, mx32EntityTypeFlags) == 0x44,
          "N3 mx32EntityTypeFlags (:105) at +0x44 -- stw @0x82217160");
    Check(offsetof(InEventVolumeTestDeepest, mExcludeEntityId) == 0x48,
          "N4 mExcludeEntityId (:106, the EntityId word) at +0x48 -- stw @0x82217188");
    Check(offsetof(InEventVolumeTestDeepest, meExclusionMode) == 0x4C,
          "N5 meExclusionMode (:107, the EExclusionMode word) at +0x4C -- stw @0x82217190");
    Check(offsetof(InEventVolumeTestDeepest, mVolumeBuffer) == 0x50 && sizeof(InEventVolumeTestDeepest().mVolumeBuffer) == 0x80,
          "N6 mVolumeBuffer (:108, VolumeSlot) at +0x50, 0x80 bytes -- memcpy @0x82217194");
    Check(offsetof(InEventVolumeTestDeepest, mxVolumeTypeFlags) == 0xD0,
          "N7 mxVolumeTypeFlags (:109) at +0xD0 -- stb @0x82217178");
    Check(offsetof(InEventVolumeTestDeepest, macOpaquePayload) == 0x00 && sizeof(InEventVolumeTestDeepest().macOpaquePayload) == 224
          && sizeof(InEventVolumeTestDeepest) == 0xE0 && alignof(InEventVolumeTestDeepest) == 16,
          "N8 the opaque payload still spans the element: +0x00, 224 bytes; sizeof 0xE0 (AddEvent `mulli 0xE0`), alignof 16");

    // ---- P / F. the revision's producer: bytes unchanged, every byte in its named member --------------------------
    const Inputs laIn[] = {
        MakeInputs(1, E_EXCLUDE_ENTITY_ONLY, 0x5A),
        MakeInputs(2, E_EXCLUDE_ALL_CHILD_PARTS, 0xFF),
        MakeInputs(3, E_EXCLUDE_ENTITY_ONLY, 0x00),
    };
    bool lbQueued = true, lbImage = true;
    bool lbTransform = true, lbQueryId = true, lbFlags = true, lbExclude = true, lbMode = true, lbVolume = true, lbTypeFlags = true;
#if FXFU_VTDE_BY_NAME
    bool lbByNameQueued = true, lbByNameImage = true;
#endif
    for (const Inputs& lrIn : laIn)
    {
        InEventVolumeTestDeepest lEvent;
        int liReturn = 0, liAsserts = 0;
        const bool lbOne = Produce<SceneQueryInterface>(lrIn, lEvent, liReturn, liAsserts);
        lbQueued = lbQueued && lbOne && liReturn == 1 && liAsserts == 0;
        if (!lbOne) { lbImage = lbTransform = lbQueryId = lbFlags = lbExclude = lbMode = lbVolume = lbTypeFlags = false; continue; }

        u8 lacConsole[0xD1];
        ConsoleImage(lrIn, lacConsole);
        const bool lbSame = std::memcmp(lEvent.macOpaquePayload, lacConsole, sizeof(lacConsole)) == 0;
        if (!lbSame)
        {
            for (u32 lu = 0; lu < sizeof(lacConsole); ++lu)
            {
                if (lEvent.macOpaquePayload[lu] != lacConsole[lu])
                {
                    std::printf("  image differs first at +0x%02X: 0x%02X, the console stores 0x%02X\n", lu,
                                lEvent.macOpaquePayload[lu], lacConsole[lu]);
                    break;
                }
            }
        }
        lbImage = lbImage && lbSame;

        lbTransform = lbTransform && std::memcmp(lEvent.mTransform, lrIn.macTransform, 64) == 0;
        lbQueryId   = lbQueryId && lEvent.mQueryId.mId == lrIn.muQueryId;
        lbFlags     = lbFlags && lEvent.mx32EntityTypeFlags == lrIn.mx32Flags;
        lbExclude   = lbExclude && lEvent.mExcludeEntityId == lrIn.muExclude;
        lbMode      = lbMode && lEvent.meExclusionMode == static_cast<u32>(lrIn.meMode);
        lbVolume    = lbVolume && std::memcmp(&lEvent.mVolumeBuffer, lrIn.macVolume, 128) == 0;
        lbTypeFlags = lbTypeFlags && lEvent.mxVolumeTypeFlags == lrIn.mxVolumeTypeFlags;

#if FXFU_VTDE_BY_NAME
        InEventVolumeTestDeepest lByName;
        int liByNameReturn = 0, liByNameAsserts = 0;
        const bool lbByNameOne = Produce<ByNameSceneQueryInterface>(lrIn, lByName, liByNameReturn, liByNameAsserts);
        lbByNameQueued = lbByNameQueued && lbByNameOne && liByNameReturn == liReturn && liByNameAsserts == liAsserts;
        lbByNameImage = lbByNameImage && lbByNameOne
                        && std::memcmp(lByName.macOpaquePayload, lEvent.macOpaquePayload, 0xD1) == 0;
#endif
    }
    Check(lbQueued, "P1 the producer returns 1 and queues exactly one element per call, no assert, for three input sets");
    Check(lbImage, "P2 the queued element's bytes +0x00..+0xD0 are the console's stores @0x82217150..0x82217194, for all "
                   "three sets: the layout the named members describe is the bytes the producer writes");
    Check(lbTransform, "F1 mTransform holds a6's 64 bytes (the four rows)");
    Check(lbQueryId, "F2 mQueryId holds a2");
    Check(lbFlags, "F3 mx32EntityTypeFlags holds a3");
    Check(lbExclude, "F4 mExcludeEntityId holds a7");
    Check(lbMode, "F5 meExclusionMode holds a8 (0 and 1)");
    Check(lbVolume, "F6 mVolumeBuffer holds a5's 0x80 bytes");
    Check(lbTypeFlags, "F7 mxVolumeTypeFlags holds a4 (0x5A, 0xFF, 0x00)");
#if FXFU_VTDE_BY_NAME
    Check(lbByNameQueued, "H1 the by-name producer (--hunk) returns, queues and asserts as the revision's producer");
    Check(lbByNameImage, "H2 the by-name producer (--hunk) writes the SAME bytes +0x00..+0xD0 as the revision's producer, "
                         "for all three sets");
#endif

    std::printf("FxFollowupsVolumeTestDeepestEvent: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
