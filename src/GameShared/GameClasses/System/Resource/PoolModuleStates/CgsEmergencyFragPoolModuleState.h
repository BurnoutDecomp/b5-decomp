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
// Member offsets are read from the inlined field accesses in Begin and RunPoolDefragmentation. The
// four derived members sit immediately after the base's miMaxToMove, so the first lands at +0x4C.
// Field widths follow the x64 PC target (pointers widen); order/types are faithful. Members are
// identified by offset but the layout is NOT byte-matched.

namespace CgsMemory { class Relocator; struct RelocationParams; }

namespace CgsResource
{
    // The working set for an emergency defrag: the base
    // params plus the Relocator that performs the moves and its per-pass parameters.
    struct EmergencyFragParams : public BaseDefragParams
    {
        CgsMemory::Relocator*       mpRelocator;        // +0x30 in the params block
        CgsMemory::RelocationParams* mpRelocationParams; // +0x34
    };

    class EmergencyFragPoolModuleState : public BaseDefragPoolModuleState
    {
    public:
        // The per-frame poll result (mirrors the IntelliFrag
        // step's result enum).
        enum EEmergencyFragResult
        {
            E_RESULT_SUCCESS   = 0,
            E_RESULT_ERROR     = 1,
            E_RESULT_PEND      = 2,
            E_RESULT_EMERGENCY = 3,
        };

        // The internal step machine.
        enum EInternalState
        {
            E_STATE_IDLE               = 0,
            E_STATE_START_DEFRAGMENTING = 1,
            E_STATE_DEFRAGMENTING_HEAP  = 2,
        };

        // ---- lifecycle / step machine (Construct / Update bodied by their own passes) ----------
        void Construct(PoolModule* lpPoolModule);                  // deferred
        void Begin(EmergencyFragParams* lpParams);
        EEmergencyFragResult Update();

    private:
        // ---- the two concrete-strategy hooks (overrides) --------------------------------------
        virtual bool RunDefragAlgorithm(AllocListSet* lpAllocListSet, LinearHeapNode* lpNodes,
                                        s32 liLastNode, s32 liMemType) override;
        virtual void RunPoolDefragmentation(RelocateRequest* lpRequests, RelocateSource* lpSources,
                                            u32 luNum, s32 liMemType) override;

        // ---- Layout (first derived member at +0x4C) -------------------------------------------
        EInternalState               meState;             // +0x4C
        s32                          miCountdown;          // +0x50
        CgsMemory::Relocator*        mpRelocator;          // +0x54
        CgsMemory::RelocationParams* mpRelocationParams;   // +0x58
    };
}
