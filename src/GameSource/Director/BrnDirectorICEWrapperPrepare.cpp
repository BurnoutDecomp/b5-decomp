// ============================================================================
// GameSource/Director/BrnDirectorICEWrapperPrepare.cpp
//
// BrnDirector::ICEWrapper::Prepare @0x8253DD90 -- the ICE wrapper's staged bring-up.
//
// ⭐⭐ WHY THIS IS A FILE OF ITS OWN. The function's declared home is
// GameSource/Director/BrnDirectorICEWrapper.cpp, but that TU is NOT on the exe source list
// and mounting it costs the link two unresolved externals it does not have today
// (ICEManager::GetCameraTake and ICECameraMover::Construct -- measured, and recorded in the
// build script's mount ledger). Prepare itself needs NEITHER. This is the same file-split
// pattern the camera wave used for BrnCameraTweakerConstruct.cpp: give the one function that
// can land today its own TU, and leave the rest of the class's home file where it is.
// DELETE-WHEN: BrnDirectorICEWrapper.cpp joins the link -- then move this body into it.
//
// ⭐⭐ WHAT IT UNBLOCKS. Until 2026-08-01 this was `return true;` in DirectorLinkStubs.cpp.
// Prepare's stage 0 is the ONLY caller of ICE::InitICEDescriptions() in the whole image, and
// that function builds the per-channel element schedules (gaICEElementChannels) that
// ICETake::SetParameter walks. Without it every schedule held miNumKeyElements == 0, the take
// evaluator's element loops ran zero times, and mValues[] was never written -- so every ICE
// camera element read 0 forever while the take itself loaded, bound, seeked and played
// normally. See the retired stub's note in DirectorLinkStubs.cpp for the full symptom.
// ============================================================================

#include "GameSource/Director/BrnDirectorICEWrapper.h"
#include "SDKs/Packages/ICE/ICEData.hpp"        // ICE::InitICEDescriptions
#include "SDKs/Packages/ICE/ICEDataEnums.hpp"   // ICE::ICEElementDescriptions / eICE_NUM_ELEMENTS

namespace BrnDirector
{
    // ------------------------------------------------------------------------
    // ICEWrapper::Prepare @0x8253DD90
    //
    // The console's shape, from the asm (0x8253DD90..0x8253DF20):
    //
    //     s32& lrStage = *(this + 73960);              // == miICELoadStateB
    //     if (lrStage >  1) return true;               // already past bring-up
    //     if (lrStage == 1) goto STAGE_1;
    //     lrStage = 0;
    //     *(this + 0x11B24) = <arg 3>;
    //     ICEMemory::Construct(this, *AllocatorList::GetRawResource(list, 34),
    //                                *AllocatorList::GetRawResourceDescriptor(list, 34));
    //     CgsMemory::HeapMalloc::Prepare(this);
    //     ICE::spICEMemory = this;                     // dword_82FB62C0
    //     ICECameraMover::Construct(...);
    //     ICEManager::Construct(...);
    //     for (d = &ICEElementDescriptions[0]; d < end; ++d)  d->Prepare();   // 0x58 stride
    //     ICE::InitICEDescriptions();
    //     ICE::InitICEDescriptions();                  // the console really does call it twice
    //   STAGE_1:
    //     lrStage = 1;
    //     ICECameraMover::Construct(...);              // again, with the same arguments
    //     return true;
    //
    // The two InitICEDescriptions calls are not a decompiler artefact -- there are two
    // distinct `bl` at 0x8253DED4 and 0x8253DED8. They are harmless because loop 1 of that
    // function re-zeroes every schedule's counts before loop 2 refills them, so it is
    // idempotent; both are reproduced rather than "cleaned up".
    //
    // ⚠️ WHAT IS GATED, AND WHAT IT COSTS. Four sub-object constructions are held back, each
    // because its callee is un-homed or un-mounted, NOT because it is thought unnecessary:
    //   * ICE::ICEMemory::Construct + CgsMemory::HeapMalloc::Prepare + the `spICEMemory = this`
    //     store. spICEMemory is the ICE EDIT heap singleton; its only consumers in this tree
    //     are the ICE AUTHORING paths (ICEAuthor*, ICETake's undo/PushUndo, NewEditBuffer).
    //     CONSEQUENCE: the in-game ICE editor cannot allocate. Take PLAYBACK -- which is what
    //     the junkyard and game-intro cameras use -- allocates nothing and is unaffected.
    //   * ICE::ICECameraMover::Construct (both call sites) and ICE::ICEManager::Construct.
    //     Neither TU is in the link, and both take argument sets pointing into un-homed
    //     wrapper interior regions. CONSEQUENCE: the ICE editor's camera mover and its
    //     manager are inert -- again an authoring-side surface. MainDirector::UpdateICE, the
    //     one consumer of ICEManager::GetCameraTake, is itself a documented gate.
    //   DELETE-WHEN: ICEMemory.cpp / ICECameraMover.cpp / ICEManager.cpp are in the link.
    //
    // ⚠️ THE STAGE WORD IS THE REAL ONE. miICELoadStateB is the wrapper's own named member at
    // console +0x120E8 == 73960, which is exactly the word the asm loads and stores. It is
    // zeroed by ICEWrapper::Construct, so a fresh wrapper enters at stage 0 as the console's
    // does. The three parameters keep their committed spelling; the console's uses of arguments
    // 2 and 3 (the allocator list and the pointer stored at +0x11B24) belong to the gated
    // ICEMemory leg, so they are untouched here.
    // ------------------------------------------------------------------------
    bool ICEWrapper::Prepare(DirectorIO::OutputBuffer* lpOutputBuffer, s32 liPrepareArg,
                             const DirectorResourceManager* lpResourceManager)
    {
        (void)lpOutputBuffer;
        (void)liPrepareArg;
        (void)lpResourceManager;

        if (miICELoadStateB > 1)
        {
            return true;
        }

        if (miICELoadStateB == 0)
        {
            miICELoadStateB = 0;

            // ⚠️ GATE: the ICEMemory / HeapMalloc / spICEMemory leg (see the banner).
            // ⚠️ GATE: ICECameraMover::Construct + ICEManager::Construct (see the banner).

            // ⭐ The element-description system's runtime bring-up. The console runs the
            // per-element Prepare sweep explicitly and then calls InitICEDescriptions, which
            // runs the very same sweep again as its loop 2; both are reproduced.
            for (s32 liElement = 0; liElement < ICE::eICE_NUM_ELEMENTS; ++liElement)
            {
                ICE::ICEElementDescriptions[liElement].Prepare();
            }

            ICE::InitICEDescriptions();
            ICE::InitICEDescriptions();
        }

        miICELoadStateB = 1;

        // ⚠️ GATE: the trailing ICECameraMover::Construct (see the banner).

        return true;
    }
}
