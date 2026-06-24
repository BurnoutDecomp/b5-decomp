#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsBaseDefragPoolModuleState.h"  // BaseDefragParams / BaseDefragPoolModuleState

// CgsResource::IntelliFragPoolModuleState - the "intelligent" (linear-merge) defragmentation step
// state of the resource PoolModule. It is the normal defrag pass the PoolModule drives while
// allocating (PoolModule::UpdateAllocating): it walks the addressed-alloc requests against the live
// linear-heap layout and merges live blocks down only as far as needed to satisfy each request,
// staging the relocations through a ScratchPool. It derives from BaseDefragPoolModuleState and
// overrides the two strategy hooks.
//
// DECOMPILED from BURNOUT_X360_ARTIST.XEX. Member offsets read from the inlined field accesses in
// Begin (0x828DA908: miMaxToMove (base) set to 0x200000 @ +0x48, meState@+0x4C, mpScratchPool@+0x50)
// and RunPoolDefragmentation (0x828F80C0: mpPool@+0xC via the base, mpScratchPool@+0x50). The two
// derived members sit immediately after the base's miMaxToMove (+0x48), so meState lands at +0x4C.
// Field widths follow the x64 PC target (pointers widen); order/types are faithful. We identify
// members by the X360 offsets but do NOT byte-match.

namespace CgsResource
{
    class ScratchPool;   // defrag staging pool (pointer member only)

    // CgsIntelliFragPoolModuleState.h:41 - the working set for an intelligent defrag: the base
    // params plus the ScratchPool the relocations are staged through.
    struct IntelliFragParams : public BaseDefragParams
    {
        ScratchPool* mpScratchPool;   // :43  (+0x30 in the params block)
    };

    // CgsIntelliFragPoolModuleState.h:47
    class IntelliFragPoolModuleState : public BaseDefragPoolModuleState
    {
    public:
        // CgsIntelliFragPoolModuleState.h:50 - the per-frame poll result.
        enum EIntelliFragResult
        {
            E_RESULT_SUCCESS   = 0,
            E_RESULT_ERROR     = 1,
            E_RESULT_PEND      = 2,
            E_RESULT_EMERGENCY = 3,
        };

        // CgsIntelliFragPoolModuleState.h:58 - the internal step machine.
        enum EInternalState
        {
            E_STATE_IDLE               = 0,
            E_STATE_START_DEFRAGMENTING = 1,
            E_STATE_DEFRAGMENTING_HEAP  = 2,
        };

        // ---- lifecycle / step machine (Construct / Update bodied by their own passes) ----------
        void Construct(PoolModule* lpPoolModule);                  // .cpp:41 (deferred)
        void Begin(IntelliFragParams* lpParams);                   // .cpp:59 (THIS PASS @ 0x828DA908)
        EIntelliFragResult Update();                               // .cpp:82 (deferred)

    private:
        // ---- the two concrete-strategy hooks (overrides) --------------------------------------
        virtual bool RunDefragAlgorithm(AllocListSet* lpAllocListSet, LinearHeapNode* lpNodes,
                                        s32 liLastNode, s32 liMemType) override;       // .cpp:179 (THIS PASS @ 0x828E3EB8)
        virtual void RunPoolDefragmentation(RelocateRequest* lpRequests, RelocateSource* lpSources,
                                            u32 luNum, s32 liMemType) override;        // .cpp:279 (THIS PASS @ 0x828F80C0)

        // ---- Layout (offsets verified against the X360 asm; first derived member at +0x4C) -----
        EInternalState meState;        // +0x4C  :76
        ScratchPool*   mpScratchPool;  // +0x50  :77
    };
}
