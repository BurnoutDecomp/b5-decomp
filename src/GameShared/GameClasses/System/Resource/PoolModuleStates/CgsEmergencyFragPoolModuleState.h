#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsBaseDefragPoolModuleState.h"  // BaseDefragParams / BaseDefragPoolModuleState

// CgsResource::EmergencyFragPoolModuleState - the "emergency" defragmentation step state of the
// resource PoolModule. When an allocation fails for lack of contiguous room, the PoolModule drives
// this state (via PoolModule::UpdateIntelliFrag): it packs every live block down towards the first
// free node and stages the freed-up addressed allocations, executing the moves through the pool's
// memory Relocator. It derives from BaseDefragPoolModuleState (which holds the working set + the
// running alloc/relocate counts) and overrides the two strategy hooks.
//
// DECOMPILED from BURNOUT_X360_ARTIST.XEX. Member offsets read from the inlined field accesses in
// Begin (0x828DA9F8: meState@+0x4C, miCountdown@+0x50, mpRelocator@+0x54, mpRelocationParams@+0x58)
// and RunPoolDefragmentation (0x828F80E0: mpPool@+0xC via the base, mpRelocator@+0x54,
// mpRelocationParams@+0x58). The four derived members sit immediately after the base's miMaxToMove
// (+0x48), so the first one lands at +0x4C. Field widths follow the x64 PC target (pointers widen);
// order/types are faithful. We identify members by the X360 offsets but do NOT byte-match.

namespace CgsMemory { class Relocator; struct RelocationParams; }

namespace CgsResource
{
    // CgsEmergencyFragPoolModuleState.h:42 - the working set for an emergency defrag: the base
    // params plus the Relocator that performs the moves and its per-pass parameters.
    struct EmergencyFragParams : public BaseDefragParams
    {
        CgsMemory::Relocator*       mpRelocator;        // :44  (+0x30 in the params block)
        CgsMemory::RelocationParams* mpRelocationParams; // :45 (+0x34)
    };

    // CgsEmergencyFragPoolModuleState.h:49
    class EmergencyFragPoolModuleState : public BaseDefragPoolModuleState
    {
    public:
        // CgsEmergencyFragPoolModuleState.h:52 - the per-frame poll result (mirrors the IntelliFrag
        // step's result enum).
        enum EEmergencyFragResult
        {
            E_RESULT_SUCCESS   = 0,
            E_RESULT_ERROR     = 1,
            E_RESULT_PEND      = 2,
            E_RESULT_EMERGENCY = 3,
        };

        // CgsEmergencyFragPoolModuleState.h:60 - the internal step machine.
        enum EInternalState
        {
            E_STATE_IDLE               = 0,
            E_STATE_START_DEFRAGMENTING = 1,
            E_STATE_DEFRAGMENTING_HEAP  = 2,
        };

        // ---- lifecycle / step machine (Construct / Update bodied by their own passes) ----------
        void Construct(PoolModule* lpPoolModule);                  // :41 (deferred)
        void Begin(EmergencyFragParams* lpParams);                 // :60/.cpp:60 (THIS PASS @ 0x828DA9F8)
        EEmergencyFragResult Update();                             // .cpp:83 (deferred)

    private:
        // ---- the two concrete-strategy hooks (overrides) --------------------------------------
        virtual bool RunDefragAlgorithm(AllocListSet* lpAllocListSet, LinearHeapNode* lpNodes,
                                        s32 liLastNode, s32 liMemType) override;       // .cpp:183 (THIS PASS @ 0x828E40C8)
        virtual void RunPoolDefragmentation(RelocateRequest* lpRequests, RelocateSource* lpSources,
                                            u32 luNum, s32 liMemType) override;        // .cpp:257 (THIS PASS @ 0x828F80E0)

        // ---- Layout (offsets verified against the X360 asm; first derived member at +0x4C) -----
        EInternalState               meState;             // +0x4C  :78
        s32                          miCountdown;          // +0x50  :79
        CgsMemory::Relocator*        mpRelocator;          // +0x54  :80
        CgsMemory::RelocationParams* mpRelocationParams;   // +0x58  :81
    };
}
