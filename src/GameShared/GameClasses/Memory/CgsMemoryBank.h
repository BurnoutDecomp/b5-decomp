#ifndef CGS_MEMORY_BANK_H
#define CGS_MEMORY_BANK_H

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsFlagSet.h"   // CgsContainers::FlagSet

// CgsMemory::MemoryBank / MemoryBankResourceSet / MemoryBlock -- the runtime records the
// CgsMemory::MemoryModule manages (see [[gamedatamodule-scoping]]). Reconstructed from the DecFIGS
// DWARF (GameShared/GameClasses/Memory/CgsMemoryBank.h) with the bank record size + free-list shape
// verified against the X360 ARTIST bodies (GetBank 0x82866F48 -> 84-byte bank stride; InitBanks
// 0x828670D0 / InitBlocks 0x82869398).
namespace CgsMemory
{
    // One memory-type sub-allocation within a bank: a block-size class + its block-index range + data
    // pointer (DWARF CgsMemoryBank.h:35). [X360: 16 bytes; on x64 the pointer makes it larger --
    // members are accessed by name, not by offset.]
    struct MemoryBankResourceSet
    {
        u16   mu16BlockSize;
        u16   mu16NumBlocks;
        u32   muFirstBlock;
        u32   muLastBlock;
        void* mpData;
    };

    // A memory bank: a named region the manager carves into typed block ranges (DWARF CgsMemoryBank.h:54;
    // record fields :205-208, layout order = resource sets first). The X360 ARTIST bank record is 84 bytes
    // = maResourceSets[5] (5x16) + mId(2) + mu8Parent(1) + mu8Flags(1). [BUILD DRIFT] the PS3 DecFIGS
    // build declares KI_NUM_TYPES = 6 (a 6th resource set); the X360 (our target) has 5 -- confirmed by
    // the 84-byte GetBank stride AND InitBanks looping 5x. We follow the X360.
    struct MemoryBank
    {
        enum
        {
            KI_NAME_SIZE                = 32,
            KI_NUM_TYPES                = 5,     // X360 (PS3 DWARF declares 6 -- see the build-drift note)
            KU_INVALID_BANK             = 255,
            KU_FLAG_ALLOW_FRAGMENTATION = 1,
            KU_FLAG_IS_LEAF             = 2,
        };

        // CreateBank configuration (DWARF :172). Carries 6 size/block entries regardless of KI_NUM_TYPES.
        struct Params
        {
            char macName[KI_NAME_SIZE];
            s32  mnParentBankId;
            s32  mnBankId;
            u32  mauBankSize[6];
            u32  mauBankBlocks[6];
            bool mbIsLeaf;
            bool mbAllowFragmentation;
        };

        // Reset to "unused": empty every resource set + mark the bank invalid. (X360 InitBanks inlines
        // this per bank: each set zeroed with first/last block = invalid, mId = -1, parent/flags = 0xFF.)
        void Clear()
        {
            for (s32 li = 0; li < KI_NUM_TYPES; ++li)
            {
                maResourceSets[li].mu16BlockSize = 0;
                maResourceSets[li].mu16NumBlocks = 0;
                maResourceSets[li].muFirstBlock  = 0xFFFFFFFFu;   // MemoryBlock::KU_INVALID_BLOCK
                maResourceSets[li].muLastBlock   = 0xFFFFFFFFu;
                maResourceSets[li].mpData        = 0;
            }
            mId       = -1;
            mu8Parent = KU_INVALID_BANK;   // 0xFF
            mu8Flags  = 0xFF;              // X360 sets the parent+flags half-word to 0xFFFF
        }
        bool  IsUsed() const                    { return mId != -1; }
        s16   GetID() const                     { return mId; }
        u8    GetParent() const                 { return mu8Parent; }
        bool  GetAllowFragmentation() const     { return (mu8Flags & KU_FLAG_ALLOW_FRAGMENTATION) != 0; }
        bool  GetIsLeaf() const                 { return (mu8Flags & KU_FLAG_IS_LEAF) != 0; }
        u16   GetBlockSize(s32 liType) const    { return maResourceSets[liType].mu16BlockSize; }
        u16   GetNumBlocks(s32 liType) const    { return maResourceSets[liType].mu16NumBlocks; }
        u32   GetFirstBlock(s32 liType) const   { return maResourceSets[liType].muFirstBlock; }
        u32   GetLastBlock(s32 liType) const    { return maResourceSets[liType].muLastBlock; }
        void* GetData(s32 liType)               { return maResourceSets[liType].mpData; }
        u32   GetSize(s32 liType);              // [body = allocation pass: needs the X360 0x... body]
        void  SetID(s16 lId)                    { mId = lId; }
        void  SetParent(u8 lu8Parent)           { mu8Parent = lu8Parent; }
        void  SetAllowFragmentation(bool lb)    { lb ? (mu8Flags |= KU_FLAG_ALLOW_FRAGMENTATION) : (mu8Flags &= ~KU_FLAG_ALLOW_FRAGMENTATION); }
        void  SetIsLeaf(bool lb)                { lb ? (mu8Flags |= KU_FLAG_IS_LEAF) : (mu8Flags &= ~KU_FLAG_IS_LEAF); }
        void  SetBlockSize(s32 liType, u16 lu16){ maResourceSets[liType].mu16BlockSize = lu16; }
        void  SetNumBlocks(s32 liType, u16 lu16){ maResourceSets[liType].mu16NumBlocks = lu16; }
        void  SetFirstBlock(s32 liType, u32 lu) { maResourceSets[liType].muFirstBlock = lu; }
        void  SetLastBlock(s32 liType, u32 lu)  { maResourceSets[liType].muLastBlock = lu; }
        void  SetData(s32 liType, void* lp)     { maResourceSets[liType].mpData = lp; }

        // members (DWARF layout order)
        MemoryBankResourceSet maResourceSets[KI_NUM_TYPES];
        s16                   mId;
        u8                    mu8Parent;
        u8                    mu8Flags;
    };

    // A single allocatable block; free blocks form a doubly-linked list by index. DWARF CgsMemoryBank.h:234
    // (12 bytes on X360). KU_INVALID_BLOCK (-1) terminates the free list.
    struct MemoryBlock
    {
        enum : u32 { KU_INVALID_BLOCK = 0xFFFFFFFFu };
        typedef CgsContainers::FlagSet<s8> FlagSet8;

        u32      muNextBlock;
        u32      muPrevBlock;
        u8       mu8ParentBank;
        u8       mu8ChildBank;
        u8       mu8Type;
        FlagSet8 mxStatus;
    };
}

#endif
