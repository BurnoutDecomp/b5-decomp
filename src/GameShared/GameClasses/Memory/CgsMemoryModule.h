#ifndef CGS_MEMORY_MODULE_H
#define CGS_MEMORY_MODULE_H

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"          // CgsModule::ModuleSingleBuffered base
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"                   // CgsMemory::LinearMalloc (mScratchSpace, by value)
#include "GameShared/GameClasses/Memory/CgsMemoryBank.h"                      // CgsMemory::MemoryBank / MemoryBlock (records)
#include "GameShared/GameClasses/Memory/DebugComponent/CgsDebugComponentMemory.h"  // DebugComponentMemory (by value)

// Pointer/parameter-only types -> forward declarations.
namespace rw { struct IResourceAllocator; }
namespace CgsModule { class IOBufferStack; }

// CgsMemory::MemoryModule -- the engine's memory-bank manager (one of the four sub-modules embedded by
// value in CgsResource::ResourceModule, which the GameDataModule embeds; see [[gamedatamodule-scoping]]).
// It owns a pool of memory BANKS (each a sub-allocation of a backing region) and a free-list of BLOCKS,
// addressed by bank id via an id map; CreateBank/AllocateIntoBank carve banks, the Process* handlers
// service the per-frame memory requests drained by Update.
//
// Reconstructed from the DecFIGS DWARF (GameShared/GameClasses/Memory/CgsMemoryModule.h, real def @
// line 120) + the X360 ARTIST bodies (Construct 0x8286ACA8, Prepare 0x8286BF18, Release 0x82866FF0,
// Update 0x8286DF98, GetBank 0x82866F48, InitBanks 0x828670D0, InitBlocks 0x82869398, CreateBank
// 0x8286C038, AllocateIntoBank 0x8286B3B8, CreateRootBank 0x8286AF68, ...). [x64: pointer members are
// 8 bytes -> the byte offsets differ from the X360's 32-bit layout; members are accessed by name.]
//
// [INCREMENTAL] This pass lands the type + the two pure stage-machine lifecycle bodies (Prepare/Release).
// The remaining bodies (Construct's rw-allocator backing alloc; GetBank/InitBanks/InitBlocks and the
// bank/block allocation + Process* handlers) need the MemoryBank (84B) / MemoryBlock (12B) runtime record
// layouts, which are NOT in the DWARF dump (only MemoryBank::Params is) and are being recovered from the
// X360 access patterns -- so those methods are declared here and reconstructed in the next MemoryModule
// pass. MemoryBank/MemoryBlock stay forward-declared (used only via pointer members until then).
namespace CgsMemory
{
    namespace MemoryIO { struct InputBuffer; struct OutputBuffer; struct MemoryRequest; }

    class MemoryModule : public CgsModule::ModuleSingleBuffered
    {
    public:
        // DWARF CgsMemoryModule.h:99 / :110 -- the Prepare/Release stage machines.
        enum EPrepareStage
        {
            E_PREPARE_START       = 0,
            E_PREPARE_MANAGER     = 1,
            E_PREPARE_INIT_MEMORY = 2,
            E_PREPARE_DONE        = 3,
        };
        enum EReleaseStage
        {
            E_RELEASE_START   = 0,
            E_RELEASE_MANAGER = 1,
            E_RELEASE_DONE    = 2,
        };

        // A root memory region the module sub-divides into banks (DWARF :79).
        struct RootResourceSet
        {
            void* mpDataStart;
            u32   muDataSize;
            u16   muNumBlocks;
        };

        // Construct-time configuration (DWARF :95).
        struct InitOptions
        {
            u32             muMaxBanks;
            u32             muHighestId;
            u32             muMaxBlocks;
            RootResourceSet maResourceSets[6];

            InitOptions();
        };

        u32  GetOverheadRequired(const InitOptions* lpInitOptions);

        // "New module": skip ModuleSingleBuffered's old DataStructure IO path (X360 *(this+4)=1).
        MemoryModule() { mbIsNewModule = true; }

        virtual void Construct(InitOptions* lpInitOptions, rw::IResourceAllocator* lpAllocator);
        virtual bool Prepare();
        virtual bool Release();
        virtual void Destruct();

        void Update(CgsModule::IOBufferStack* lpInStack, CgsModule::IOBufferStack* lpOutStack,
                    const MemoryIO::InputBuffer* lpInput, MemoryIO::OutputBuffer* lpOutput);

    private:
        // Internal request CreateBank builds + hands to AllocateIntoBank: the new bank's slot + its parent
        // index, plus per-type block counts/sizes (computed from the parent's block granularity). Layout
        // matches the offsets AllocateIntoBank reads on the X360 (counts@2, sizes@12, flag@22).
        struct BankAllocRequest
        {
            u8  mu8NewBankSlot;                          // +0
            u8  mu8ParentBankIndex;                      // +1
            u16 mau16Counts[MemoryBank::KI_NUM_TYPES];   // +2
            u16 mau16Sizes[MemoryBank::KI_NUM_TYPES];    // +12
            u8  mu8Flag;                                 // +22
        };

        MemoryBank* GetBank(u8 lu8BankId);
        // Map a bank id to its bank-array index via mpu8IdMap (KU_INVALID_BANK/255 = not present).
        u8 GetBankIndexFromId(u32 luBankId) const;

        // --- bank/block bring-up + allocation (next MemoryModule pass; need the record layouts) ---
        void InitBanks();
        void InitBlocks();
        void CreateRootBank();
        // Create a bank from a Params request (sub-allocated from its parent). Returns 0 on success or an
        // error code (1 bad id, 2 already exists, 3 parent missing, 7 size not a multiple of block count).
        int CreateBank(const MemoryBank::Params* lpParams);
        // Destroy a bank: (non-leaf) verify it holds no live allocations, then return its blocks to the
        // free list + restore the parent's child-markings + clear the record. Returns 0 or an error code.
        int DestroyBank(s32 liBankId, bool lbLeafMode);
        // Sub-allocate a new bank's per-type block ranges out of its parent bank. Returns 0 on success or
        // an error code (4 search failed, 5 not divisible, 6 out of blocks, 7 zero block size).
        int AllocateIntoBank(BankAllocRequest* lpRequest);
        // Pull luCount blocks off the front of the free list, tag them (parent bank/type) and unlink.
        bool AllocateBlocks(u16 lu16Count, u8 lu8BankId, u8 lu8Type, bool lbMarkUsed,
                            u32* lpuOutFirstBlock, u32* lpuOutLastBlock);
        // Scan from luStartBlock for luNumWanted contiguous free blocks (frag-locked blocks count as free
        // when lbAllowFragmented). On success outputs the run's start index/block + scan index + end block.
        bool FindContiguousFreeBlocks(u16 lu16NumWanted, u32 luStartBlock, bool lbAllowFragmented,
                                      u32* lpuOutRunStartIndex, u32* lpuOutRunStartBlock,
                                      u32* lpuOutScanIndex, u32* lpuOutRunEndBlock);
        // Mark luCount blocks from luStartBlock as allocated by a child bank.
        void MarkRangeAsUsed(u8 lu8ChildBankId, u32 luStartBlock, u32 luCount);

        // --- per-frame request dispatch (Update -> ProcessMemoryRequest -> a Process* handler) ---
        void ProcessMemoryRequest(const MemoryIO::MemoryRequest* lpRequest, MemoryIO::OutputBuffer* lpOutput);
        void ProcessCreateBankRequest(const MemoryIO::MemoryRequest* lpRequest, MemoryIO::OutputBuffer* lpOutput);
        void ProcessDestroyBankRequest(const MemoryIO::MemoryRequest* lpRequest, MemoryIO::OutputBuffer* lpOutput);
        void ProcessCreateLinearAllocatorRequest(const MemoryIO::MemoryRequest* lpRequest, MemoryIO::OutputBuffer* lpOutput);
        void ProcessCreateGeneralAllocatorRequest(const MemoryIO::MemoryRequest* lpRequest, MemoryIO::OutputBuffer* lpOutput);
        void ProcessCreateAllocatorRequest(const MemoryIO::MemoryRequest* lpRequest, MemoryIO::OutputBuffer* lpOutput);
        void ProcessDestroyAllocatorRequest(const MemoryIO::MemoryRequest* lpRequest, MemoryIO::OutputBuffer* lpOutput);
        void ProcessCreateResourceRequest(const MemoryIO::MemoryRequest* lpRequest, MemoryIO::OutputBuffer* lpOutput);

        // ---- members (DWARF order; x64-laid-out, not the X360 32-bit offsets) ----
        EPrepareStage          mePrepareStage;
        EReleaseStage          meReleaseStage;
        MemoryBank*            mpBanks;            // bank-record array [muMaxBanks]
        MemoryBlock*           mpBlocks;           // block-record array [muMaxBlocks]
        u8*                    mpu8IdMap;          // bank id -> bank index map [muHighestId]
        char**                 mppcNames;          // per-bank name buffers [muMaxBanks]
        u32                    muMaxBanks;
        u32                    muMaxBlocks;
        u32                    muHighestId;
        u32                    muFirstFreeBlock;
        u32                    muNumFreeBlocks;
        CgsMemory::LinearMalloc mScratchSpace;     // bump allocator over the backing region
        RootResourceSet        maRootSets[6];
        DebugComponentMemory   mDebugComponentMemory;
    };
}

#endif
