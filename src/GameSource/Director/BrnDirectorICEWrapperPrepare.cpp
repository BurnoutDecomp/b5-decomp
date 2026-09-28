// ============================================================================
// GameSource/Director/BrnDirectorICEWrapperPrepare.cpp
//
// BrnDirector::ICEWrapper::Prepare @0x8253DD90 -- the ICE wrapper's staged bring-up.
//
// ⭐⭐ WHY THIS IS A FILE OF ITS OWN. The function's declared home is
// GameSource/Director/BrnDirectorICEWrapper.cpp, but that TU is NOT on the exe source list
// (its EditorOn / EditorOff reach the un-homed DebugInterface console toggles). This is the same file-split
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
#include "GameSource/Director/BrnDirectorResourceManager.h"               // DirectorResourceManager::GetIceResourceManager
#include "GameSource/Resource/SharedIO/BrnGameDataAllocatorList.h"        // AllocatorList::GetRawResource / ...Descriptor
#include "SDKs/Packages/ICE/ICEData.hpp"        // ICE::InitICEDescriptions
#include "SDKs/Packages/ICE/ICEDataEnums.hpp"   // ICE::ICEElementDescriptions / eICE_NUM_ELEMENTS
#include "SDKs/Packages/ICE/ICEMemory.hpp"      // ICE::spICEMemory, ICE::ICEPointers

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
    // ⭐⭐ UN-GATED 2026-09-27 (OWNERLIST lane L5 -- the pause camera). Stage 0 used to skip the
    // console's first seven stores and calls (the resource-manager store, the ICE heap, spICEMemory,
    // ICECameraMover::Construct and ICEManager::Construct) under a banner that called them editor-only.
    // They are not: ICEWrapper::PlayMovie resolves every movie through mpResourceManager
    // (DirectorResourceManager::GetKeyAnim), ICEManager::Update advances playback by *mpTimer (a pointer
    // only ICEManager::Construct stores), and the camera mover writes through the anchor / camera
    // pointers only ICECameraMover::Construct stores. The first movie ever played -- the pause menu's
    // playlist, ArbStateCrashNav::Prepare @0x822660A8 -> ICEMoviePlayer::Loop -> PlayMovie -- died on
    // the null resource manager (an AV in DirectorResourceManager::GetKeyAnim). All seven are the
    // console's, in its order (0x8253DDF8..0x8253DEAC); the only PC reading is how the arguments are
    // named:
    //   *(this + 0x11B24) = arg 3                 mpResourceManager
    //   *GetRawResource(list, 34) / ...Descriptor  the "Ice" raw bank (BrnMemoryMapData.h: bank 34,
    //                                              0x40000 bytes) -- its first base pointer and size
    //   ICEMemory::Construct(this, ...)           mICEMemory, the wrapper's first member (offset 0)
    //   HeapMalloc::Prepare(this)                 mICEMemory's HeapMalloc base
    //   dword_82FB62C0 = this                     ICE::spICEMemory = &mICEMemory
    //   ICECameraMover::Construct(this+0x11BD0, 1, this+0x11ED0, this+0x11D60, <camera take>, 0, rm+0x228)
    //                                             the take is ICEManager::GetCameraTake inlined
    //                                             (manager +0x1CE0 ? +0x15A8 : +0x1D38); rm+0x228 is the
    //                                             resource manager's ICEResourceMgr (GetIceResourceManager)
    //   ICEManager::Construct(this+0xA40, &{this, this+0x9B24, this+0x11B28, this+0x11ED0, this+0x9B20,
    //                                        rm+0x228})     the inlined ICEPointers::Construct
    //
    // ⚠️ THE STAGE WORD IS THE REAL ONE. miICELoadStateB is the wrapper's own named member at
    // console +0x120E8 == 73960, which is exactly the word the asm loads and stores. It is
    // zeroed by ICEWrapper::Construct, so a fresh wrapper enters at stage 0 as the console's
    // does. The compare is UNSIGNED (`cmplwi cr6, r11, 1` @0x8253DDCC): 0 -> stage 0, 1 -> stage 1,
    // anything else -> done.
    // ------------------------------------------------------------------------
    bool ICEWrapper::Prepare(DirectorIO::OutputBuffer* lpOutputBuffer,
                             const BrnResource::GameDataIO::AllocatorList* lpAllocatorList,
                             const DirectorResourceManager* lpResourceManager)
    {
        (void)lpOutputBuffer;

        // The Ice raw bank (BrnMemoryMapData.h KAC_MEMORY_MAP_RAW_ALLOCATORS: bank 34, "Ice").
        const s32 KI_ICE_RAW_BANK = 34;   // li r4, 0x22 @0x8253DDF0 / 0x8253DE08

        const u32 luStage = static_cast<u32>(miICELoadStateB);
        if (luStage > 1u)
        {
            return true;
        }

        if (luStage == 0u)
        {
            miICELoadStateB = 0;
            mpResourceManager = const_cast<DirectorResourceManager*>(lpResourceManager);   // stwx @0x8253DDFC

            rw::Resource*           lpResource = lpAllocatorList->GetRawResource(KI_ICE_RAW_BANK);
            rw::ResourceDescriptor* lpDesc     = lpAllocatorList->GetRawResourceDescriptor(KI_ICE_RAW_BANK);
            mICEMemory.Construct(lpResource->m_baseResources[0],
                                 static_cast<s32>(lpDesc->m_baseResourceDescriptors[0].m_size));   // @0x8253DE24
            mICEMemory.Prepare();                                                                  // @0x8253DE2C
            ICE::spICEMemory = &mICEMemory;                                                        // @0x8253DE3C

            mCameraMover.Construct(1, &mICECameraAnchor, &mICECamera, mICEManager.GetCameraTake(), 0,
                                   lpResourceManager->GetIceResourceManager());                   // @0x8253DE70

            ICE::ICEPointers lICEPointers;
            lICEPointers.Construct(&mICEFileHandler, &mActionQueue, &mICEMemory, &mICECameraAnchor, &mICETimer,
                                   lpResourceManager->GetIceResourceManager());
            mICEManager.Construct(&lICEPointers);                                                  // @0x8253DEAC

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

        // The stage-1 leg re-Constructs the mover, every call, with the same arguments (@0x8253DF14).
        mCameraMover.Construct(1, &mICECameraAnchor, &mICECamera, mICEManager.GetCameraTake(), 0,
                               lpResourceManager->GetIceResourceManager());

        return true;
    }
}
