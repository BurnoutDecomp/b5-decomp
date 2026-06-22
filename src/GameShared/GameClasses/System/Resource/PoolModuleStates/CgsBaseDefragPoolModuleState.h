#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/System/Resource/CgsResourceHeap.h"   // AllocRequestAddressed / RelocateRequest / RelocateSource / LinearHeapNode / EBatchAllocResult

// CgsResource::BaseDefragPoolModuleState - the shared base for the resource pool's
// defragmentation module-states (IntelliFrag / EmergencyFrag derive from it). A pool-module
// "state" is a small step machine that the PoolModule pushes onto its defrag pipeline; the
// base holds the working set handed in via BaseDefragParams (the pool, its alloc/relocate/
// distribution scratch arrays and their capacities) plus the running counts the defrag
// algorithm fills (addressed-alloc count, relocation count, the flattened linear-heap node
// list and its length). The two RunXxx hooks are pure-virtual overridden by the concrete
// frag strategies.
//
// DECOMPILED from BURNOUT_X360_ARTIST.XEX. Member offsets verified against the inlined field
// accesses in Begin (0x828DA090), FindNextFreeNode (0x828DA248) and BuildFinalRelocationData
// (0x828DA2E8): vptr@+0, mpPoolModule@+4, miCurrentMemType@+8, then the BaseDefragParams block
// mpPool@+0xC..muMaxRelocateSources@+0x38, then muAddressedAllocCount@+0x3C, muRelocationCount@
// +0x40, muNumLinearHeapNodes(u16)@+0x44, miMaxToMove@+0x48. Field widths follow the x64 PC
// target (pointers widen); field order/types are faithful. We identify members by the X360
// offsets but do NOT byte-match. Method bodies that this pass owns are in the matching .cpp;
// the remaining members are declared for the class shape and bodied by their own passes.

namespace CgsResource
{
    class Pool;
    class PoolModule;
    struct AllocListSet;
    struct DistributionEntry;

    // CgsBaseDefragPoolModuleState.h:48 - the working set handed to Begin(): the pool to
    // defragment plus the caller-owned scratch arrays and their capacities.
    struct BaseDefragParams
    {
        Pool*                  mpPool;                       // :50
        AllocListSet*          mpAllocListSet;               // :51
        AllocRequestAddressed* mpAddressedAllocRequests;     // :52
        RelocateRequest*       mpRelocateRequests;           // :53
        DistributionEntry*     mpDistributionEntries;        // :54
        LinearHeapNode*        mpLinearHeapNodes;            // :55
        RelocateSource*        mpRelocateSources;            // :56
        u32                    muMaxAddressedAllocRequests;  // :57
        u32                    muMaxRelocateRequests;        // :58
        u32                    muMaxDistributionRequests;    // :59
        u32                    muMaxLinearHeapNodes;         // :60
        u32                    muMaxRelocateSources;         // :61
    };

    // CgsBasePoolModuleState.h:33 - empty base (no members of its own in the X360 DWARF).
    struct BasePoolModuleState
    {
    };

    // CgsBaseDefragPoolModuleState.h:74
    class BaseDefragPoolModuleState : public BasePoolModuleState
    {
    public:
        // ---- lifecycle / step machine ------------------------------------------------
        void Construct(PoolModule* lpPoolModule);                         // :80 (deferred)
        void Begin(BaseDefragParams* lpParams);                           // :83 (THIS PASS @ 0x828DA090)
        bool BeginDefragment(s32 liMemType);                              // :86 (deferred)
        bool DoFinalAllocations();                                        // :89 (deferred)
        s32  FindFirstFreeNode();                                         // :92 (deferred)
        s32  FindNextFreeNode(s32 liStartNode);                           // :95 (THIS PASS @ 0x828DA248)
        s32  BuildFinalRelocationData();                                  // :98 (THIS PASS @ 0x828DA2E8)

        // ---- request builders / accessors --------------------------------------------
        u32  AddAddressedAllocRequest(u32 luSize, u32 luOffset, void* lpOwner);  // :181 (deferred)
        u32  AddRelocateRequest(u16 luNode, u32 luDestOffset);                   // :203 (deferred)
        Pool*           GetPool();                                               // :217 (deferred)
        PoolModule*     GetPoolModule();                                         // :223 (deferred)
        EBatchAllocResult GetAllocationResult(s32 liIndex);                      // :229 (deferred)
        void            SetMaxToMove(s32 liMaxToMove);                           // :235 (deferred)

    private:
        // ---- the two concrete-strategy hooks -----------------------------------------
        virtual bool RunDefragAlgorithm(AllocListSet* lpAllocListSet, LinearHeapNode* lpNodes,
                                        s32 liA, s32 liB);                       // :155
        virtual void RunPoolDefragmentation(RelocateRequest* lpRequests, RelocateSource* lpSources,
                                            u32 luNum, s32 liMemType);           // :162

        // ---- Layout (offsets verified against the X360 asm) --------------------------
        PoolModule*            mpPoolModule;                 // +0x04  :125
        s32                    miCurrentMemType;             // +0x08  :127
        Pool*                  mpPool;                       // +0x0C  :129
        AllocListSet*          mpAllocListSet;               // +0x10  :130
        AllocRequestAddressed* mpAddressedAllocRequests;     // +0x14  :131
        RelocateRequest*       mpRelocateRequests;           // +0x18  :132
        DistributionEntry*     mpDistributionEntries;        // +0x1C  :133
        LinearHeapNode*        mpLinearHeapNodes;            // +0x20  :134
        RelocateSource*        mpRelocateSources;            // +0x24  :135
        u32                    muMaxAddressedAllocRequests;  // +0x28  :137
        u32                    muMaxRelocateRequests;        // +0x2C  :138
        u32                    muMaxDistributionRequests;    // +0x30  :139
        u32                    muMaxLinearHeapNodes;         // +0x34  :140
        u32                    muMaxRelocateSources;         // +0x38  :141
        u32                    muAddressedAllocCount;        // +0x3C  :143
        u32                    muRelocationCount;            // +0x40  :144
        u16                    muNumLinearHeapNodes;         // +0x44  :146
        s32                    miMaxToMove;                  // +0x48  :148
    };
}
