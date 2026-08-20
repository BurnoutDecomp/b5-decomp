// X360 0x827DB6B0. Compiler-synthesized default constructor (the Feb-2007 partial source has no hand-written
// body; the DecFIGS DWARF emits an empty body at BrnStuntManagerDebugComponent.h:50). It exists
// only because members need non-trivial construction: the CgsDev::DebugComponent base (installs its
// vtable, off_820CDE8C as the most-derived) and the three CgsDev::SimpleStrStream subobjects (each
// runs StrStreamBase() -> installs base vtable off_82000D00 + mePrintMode=0, then SimpleStrStream()
// -> installs derived vtable off_82014B00 + resets the inline buffer). The X360 pseudocode is
// exactly that, unrolled (stride 0x108 = 264B per element, 3 elements). mpStuntManager is left
// uninitialised (assigned later by Construct()); the three Vector3 maLastPositions are trivially
// default-constructed. Reusing the real committed base + member types lands the natural layout on
// the X360 offsets (DebugComponent base +0x00..+0x0F, mpStuntManager +0x0C, maStrStreams[0] +0x10
// == r26+0x10), so an explicitly-defaulted ctor regenerates the observed code with no raw-offset
// access and no manual padding.
#include "GameSource/GameState/Offences/BrnStuntManagerDebugComponent.h"

namespace BrnGameState
{
    StuntManagerDebugComponent::StuntManagerDebugComponent() = default;

    // ------------------------------------------------------------------------
    // [gateui] Construct @ X360 0x82358C80 -- BODIED 2026-08-20.
    //
    // Why it had to land: StuntManager::Construct calls it (BrnStuntManager.cpp :: Construct,
    // `mStuntManagerDebugComponent.Construct(this)`), and the component is held BY VALUE at the
    // manager's +0x000 -- so mounting BrnStuntManager.cpp without this body is an LNK2019. It was
    // on the round-1 verify pass's measured UNDEF list.
    //
    // The console body is three things and nothing else:
    //   0x82358C9C  bl CgsSceneManager::CgsCollision::BaseCollisionGenerator::Destruct
    //               -- an ICF FOLD, not a collision call: 0x8284CB38 is a bare `blr`, so the
    //                  linker folded every empty function in the image onto it. The one that
    //                  belongs here is the DebugComponent base's own Construct.
    //   0x82358CA4  stw r30, 0xC(r31)                 -- mpStuntManager = lpStuntManager
    //   0x82358CA8..0x82358CE4  a 3-iteration `stvx128` loop from r31+0x330, stride 0x10, storing
    //               a stack quad built from flt_82001CC0 (== 0.0f) x3 + a zeroed high lane
    //               -- i.e. ZERO THE THREE maLastPositions.
    // ⓘ 0x330 == 816, not the 808 that `maStrStreams` (+0x10, 3 x 264B) ends at: Vector3 is
    // 16-byte aligned (rw::math::vpu), so the compiler pads 808 -> 816. Reproduced by NAME here,
    // which lands the same padding on the host without baking either literal.
    // ------------------------------------------------------------------------
    void StuntManagerDebugComponent::Construct(StuntManager* lpStuntManager)
    {
        DebugComponent::Construct();

        mpStuntManager = lpStuntManager;

        for (s32 liIndex = 0; liIndex < 3; ++liIndex)
        {
            maLastPositions[liIndex].SetZero();   // the console's zeroed stack quad, stvx128'd
        }
    }
}
