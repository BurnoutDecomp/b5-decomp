#pragma once

#include <cstddef>   // size_t
#include <cstdint>   // uint32_t / int8_t / wchar_t

// EA::Allocator::GeneralAllocator - EA's PPMalloc general-purpose heap allocator (the same EA
// middleware allocator Burnout's resource system allocates module memory through). It is a
// dlmalloc-style allocator: small/fast bins (mpFastBinArray), regular bins (mpBinArray) keyed
// by a bitmap (mBinBitmap), a top chunk, and a linked list of OS core blocks (mHeadCoreBlock).
//
// Layout (member names/types/order) recovered from Spore PC (EA::Allocator::GeneralAllocator,
// sizeof=1316 on x86), dumped to progress/scratch_dossiers/ea_generalallocator_spore.txt, and
// cross-checked against the X360 DWARF surface (references/DecFIGS/.../PPMalloc/EAGeneralAllocator.h)
// + the X360 ctor 0x82B4FF58 / Init 0x82B4FA30 store offsets. Spore + Burnout share this EA
// middleware. The PDB is x86 (4-byte pointers); this header uses the real pointer types so the
// PC x64 build lays it out naturally (semantic parity - field set/order match; pointer-sized
// members widen 4B->8B). Method BODIES (Malloc/MallocAligned/MallocAlignedInternal/Init/SetOption/
// bin management) are reconstructed from the X360 pseudocode on top of this layout.
//
// This is EA vendor code: members/types follow EA's PPMalloc convention (mpX/mnX/mbX fields,
// kEnumValue enumerators), NOT the Brn/Cgs K..._ convention. Assertions route through the
// allocator's own mpAssertionFailureFunction / AssertionFailure sink (PPMalloc is self-contained;
// it does not pull in the game's CGS_ASSERT, which would invert the vendor<-game layering).
namespace EA
{
namespace Allocator
{
    class GeneralAllocator
    {
    public:
        // DWARF EAGeneralAllocator.h:357 - the ICoreAllocator interface type id.
        static const int kTypeId = 883630667;

        enum HeapValidationLevel
        {
            kHeapValidationLevelNone   = 0,
            kHeapValidationLevelBasic  = 1,
            kHeapValidationLevelDetail = 2,
            kHeapValidationLevelFull   = 3,
        };

        // DWARF EAGeneralAllocator.h:595 / 605 - hook callback classification.
        enum HookType
        {
            kHookTypeMalloc   = 0,
            kHookTypeFree     = 1,
            kHookTypeCoreAlloc = 2,
            kHookTypeCoreFree = 3,
        };
        enum HookSubType
        {
            kHookSubTypeNone           = 0,
            kHookSubTypeMalloc         = 1,
            kHookSubTypeCalloc         = 2,
            kHookSubTypeRealloc        = 3,
            kHookSubTypeMallocAligned  = 4,
            kHookSubTypeMallocMultiple1 = 5,
            kHookSubTypeMallocMultiple2 = 6,
        };

        // DWARF EAGeneralAllocator.h:1102 / 1108 - DescribeData preview kind / bin classification.
        enum DataType
        {
            kDataTypeBinary = 0,
            kDataTypeChar   = 1,
            kDataTypeWChar  = 2,
        };
        enum BinType
        {
            kBinTypeNone   = 0,
            kBinTypeFast   = 1,
            kBinTypeNormal = 2,
            kBinTypeTop    = 3,
        };

        // DWARF EAGeneralAllocator.h:700 - SetOption option ids (the X360 SetOption 0x82B4DF60
        // jump-table indexes; the table covers options 1..13).
        enum Option
        {
            kOptionNone                     = 0,
            kOptionEnableThreadSafety       = 1,
            kOptionEnableHighAllocation     = 2,
            kOptionEnableSystemAlloc        = 3,
            kOptionEnableMallocFailureAssert = 4,
            kOptionMaxMallocFailureCount    = 5,
            kOptionMaxFastBinRequestSize    = 6,
            kOptionTrimThreshold            = 7,
            kOptionTopPad                   = 8,
            kOptionMMapThreshold            = 9,
            kOptionMMapMaxAllowed           = 10,
            kOptionNewCoreSize              = 11,
            kOptionCoreIncrementSize        = 12,
            kOptionMMapTopDown              = 13,
        };

        // --- nested types (GeneralAllocator::*) -----------------------------------------
        struct Chunk
        {
            unsigned int mnPriorSize;
            unsigned int mnSize;
            Chunk*       mpPrevChunk;
            Chunk*       mpNextChunk;
        };

        struct CoreBlock
        {
            char*        mpCore;
            unsigned int mnSize;
            unsigned int mnReservedSize;
            bool         mbMMappedMemory;
            bool         mbShouldFree;
            bool         mbShouldFreeOnShutdown;
            bool         mbShouldTrim;
            unsigned int (*mpCoreFreeFunction)(GeneralAllocator*, void*, unsigned int, void*);
            void*        mpCoreFreeFunctionContext;
            CoreBlock*   mpPrevCoreBlock;
            CoreBlock*   mpNextCoreBlock;
        };

        struct BlockInfo
        {
            const void*  mpCore;
            const void*  mpBlock;
            unsigned int mnBlockSize;
            void*        mpData;
            unsigned int mnDataSize;
            char         mBlockType;
            bool         mbMemoryMapped;
        };

        struct HookInfo
        {
            GeneralAllocator*   mpGeneralAllocator;
            bool                mbEntry;
            HookType            mHookType;
            HookSubType         mHookSubType;
            unsigned int        mnSizeInputTotal;
            const void*         mpDataInput;
            unsigned int        mnCountInput;
            unsigned int        mnSizeInput;
            const unsigned int* mpSizeInputArray;
            unsigned int        mnAlignmentInput;
            int                 mnAllocationFlags;
            void*               mpDataOutput;
            void**              mpArrayOutput;
            unsigned int        mnSizeOutput;
        };

        struct InitCallbackNode
        {
            void              (*mpInitCallbackFunction)(GeneralAllocator*, bool, void*);
            void*             mpContext;
            InitCallbackNode* mpNext;
        };

        // RAII lock over the allocator's mutex (a CRITICAL_SECTION). ctor enters, dtor leaves;
        // a null mutex is a no-op (matches the X360 `if (mutex)` guard).
        struct PPMAutoMutex
        {
            void* mpMutex;
            PPMAutoMutex(void* pMutex);   // @ 0x82B4DF08
            ~PPMAutoMutex();
        };

        struct SnapshotImage
        {
            unsigned long long mData[32];
        };

        struct Snapshot
        {
            int          mnMagicNumber;
            unsigned int mnSizeOfThis;
            int          mnBlockTypeFlags;
            bool         mbUserAllocated;
            bool         mbReport;
            bool         mbDynamic;
            CoreBlock*   mpCurrentCoreBlock;
            Chunk*       mpCurrentChunk;
            Chunk*       mpCurrentMChunk;
            unsigned int mnBlockInfoCount;
            unsigned int mnBlockInfoIndex;
            BlockInfo    mBlockInfo[1];
        };

        // ================================================================================
        // PUBLIC API. The existing leaf-engine bodies (Init()/AddCore/Malloc.../Free/Owns)
        // are PRESERVED; the full-DWARF surface (the 6-arg ctor/Init core-init overload, the
        // validation/trace/report/option/callback methods) is declared on top of them.
        // ================================================================================

        // DWARF EAGeneralAllocator.h:405 - core-adopting ctor:
        // GeneralAllocator(pInitialCore, nSize, bShouldFreeInitialCore, bShouldTrace, pCoreAddFn, pContext).
        GeneralAllocator(void* pInitialCore, size_t nInitialCoreSize,
                         bool bShouldFreeInitialCore = true, bool bShouldTrace = false,
                         unsigned int (*pCoreAddFunction)(GeneralAllocator*, void*, unsigned int, void*) = 0,
                         void* pCoreAddFunctionContext = 0);
        // Default-construct an empty allocator (core adopted later via AddCore). Preserved.
        GeneralAllocator();
        virtual ~GeneralAllocator();

        // Empty-heap bring-up (preserved 0-arg form used by the resource allocators).
        bool  Init();
        // DWARF EAGeneralAllocator.h:420 - full core-init overload (same arg set as the ctor).
        bool  Init(void* pInitialCore, size_t nInitialCoreSize,
                   bool bShouldFreeInitialCore, bool bShouldTrace,
                   unsigned int (*pCoreAddFunction)(GeneralAllocator*, void*, unsigned int, void*),
                   void* pCoreAddFunctionContext);
        bool  Shutdown();

        // DWARF EAGeneralAllocator.h:437 - ICoreAllocator interface query.
        void* AsInterface(int id);

        // Adopt [pCore, pCore+nSize) as the heap's backing memory. @ 0x82B4F800.
        bool  AddCore(void* pCore, size_t nSize, bool bShouldFree = false, bool bShouldFreeOnShutdown = false);
        void* Malloc(size_t nSize, int nAllocationFlags = 0);
        void* MallocAligned(size_t nSize, size_t nAlignment, size_t nAlignmentOffset = 0, int nAllocationFlags = 0);  // @ 0x82B515B0
        void* Calloc(size_t nElementCount, size_t nElementSize, int nAllocationFlags = 0);
        void* Realloc(void* pData, size_t nNewSize, int nAllocationFlags = 0);
        void  Free(void* pData);

        // DWARF EAGeneralAllocator.h:521/526 - batch allocation (uniform / per-element sizes).
        void** MallocMultiple(size_t nElementCount, size_t nElementSize, void** pResultArray, int nAllocationFlags = 0);
        void** MallocMultiple(size_t nElementCount, const size_t* pElementSizeArray, void** pResultArray, int nAllocationFlags = 0);

        // DWARF EAGeneralAllocator.h:552/563 - return free core to the OS / drop cached fast bins.
        size_t TrimCore(size_t nPadding);
        void   ClearCache();

        // True if pData lies within this allocator's adopted core (used to route Free between
        // sibling allocators, e.g. rw::core::GeneralResourceAllocator's main vs physical heaps).
        bool  Owns(const void* pData) const;
        void  SetName(const char* pName);
        const char* GetName() const;

        // DWARF EAGeneralAllocator.h:577/583/590 - failure / assertion sinks.
        void SetMallocFailureFunction(bool (*pMallocFailureFunction)(GeneralAllocator*, size_t, size_t, void*), void* pContext);
        void SetAssertionFailureFunction(void (*pAssertionFailureFunction)(const char*, void*), void* pContext);
        void AssertionFailure(const char* pExpression) const;

        // DWARF EAGeneralAllocator.h:646/655 - alloc hook + manual lock.
        void SetHookFunction(void (*pHookFunction)(const HookInfo*, void*), void* pContext);
        void Lock(bool bEnable);

        // DWARF EAGeneralAllocator.h:700 - runtime tunables (option ids in enum Option). @ 0x82B4DF60.
        void SetOption(int option, int nValue);

        // DWARF EAGeneralAllocator.h:749/757/770/777 - init-callback + size queries.
        void   SetInitCallback(InitCallbackNode* pInitCallbackNode, bool bRegister);
        size_t GetUsableSize(const void* pData) const;
        size_t GetBlockSize(const void* pData, bool bNetSize) const;
        size_t GetLargestFreeBlock(bool bClearCache);

        // DWARF EAGeneralAllocator.h:785/794 - trace output sink + field/record delimiters.
        void SetTraceFunction(void (*pTraceFunction)(const char*, void*), void* pContext);
        void SetTraceFieldDelimiters(unsigned char fieldDelimiter, unsigned char recordDelimiter);

        // DWARF EAGeneralAllocator.h:819/836 - heap validation.
        bool ValidateHeap(HeapValidationLevel heapValidationLevel);
        void SetAutoHeapValidation(HeapValidationLevel heapValidationLevel, size_t nFrequency);

        // DWARF EAGeneralAllocator.h:853 - dump every live allocation through a callback. @ 0x82B516F0.
        void   TraceAllocatedMemory(void (*pTraceCallback)(const char*, void*), void* pTraceCallbackContext,
                                    void* pStorage, size_t nStorageSize);
        // DWARF EAGeneralAllocator.h:859 - describe a block's data into a text buffer.
        size_t DescribeData(const void* pData, char* pBuffer, size_t nBufferLength);

        // DWARF EAGeneralAllocator.h:894/903 - address queries.
        const void* ValidateAddress(const void* pAddress, int addressType) const;
        bool        IsAddressHigh(const void* pAddress);

        // DWARF EAGeneralAllocator.h:958/959 - heap snapshot lifecycle.
        void* TakeSnapshot(int nBlockTypeFlags, bool bMakeCopy, void* pStorage, size_t nStorageSize);
        void  FreeSnapshot(void* pSnapshot);

        // DWARF EAGeneralAllocator.h:977/993/994/995 - block enumeration.
        bool              ReportHeap(bool (*pReportFunction)(const BlockInfo*, void*), void* pContext,
                                     int nBlockTypeFlags, bool bMakeCopy, void* pStorage, size_t nStorageSize);
        const void*       ReportBegin(void* pSnapshot, int nBlockTypeFlags, bool bMakeCopy, void* pStorage, size_t nStorageSize);
        const BlockInfo*  ReportNext(const void* pContext, int nBlockTypeFlags);
        void              ReportEnd(const void* pContext);

    protected:
        // --- internal allocation engine -------------------------------------------------
        void* MallocInternal(size_t nSize, int nAllocationFlags);
        void* CallocInternal(size_t nElementCount, size_t nElementSize, int nAllocationFlags);
        void* ReallocInternal(void* pData, size_t nNewSize, int nAllocationFlags);
        void  FreeInternal(void* pData);
        void* MallocAlignedInternal(size_t nSize, size_t nAlignment, size_t nAlignmentOffset, int nAllocationFlags);
        void** MallocMultipleInternal(size_t nElementCount, size_t nSizeOrSizeCount, const size_t* pSizeArray, void** pResultArray, int nAllocationFlags);

        // --- core management (DWARF EAGeneralAllocator.h:1134..1136) ---------------------
        Chunk* ExtendCoreInternal(size_t nMinSize);
        Chunk* AddCoreInternal(size_t nMinSize);
        bool   FreeCore(CoreBlock* pCoreBlock, bool bShouldFreeCoreBlock);

        void   ClearFastBins();

        // Default assertion/trace sinks the ctor installs (X360 AssertionFailureFunctionDefault
        // 0x82B4EDC8 / TraceFunctionDefault 0x82B4DC98).
        static void AssertionFailureFunctionDefault(const char* pMessage, void* pContext);
        static void TraceFunctionDefault(const char* pMessage, void* pContext);

        // --- alignment / page helpers (DWARF EAGeneralAllocator.h:1146..1153) ------------
        bool         GetIsPowerOf2(size_t value);
        bool         GetIsMinAligned(void* pData);
        bool         GetIsAligned(void* pData, size_t nAlignment);
        size_t       AlignUp(size_t nValue, size_t nAlignment);
        char*        AlignUp(void* pValue, size_t nAlignment);
        unsigned int GetPageSize();
        bool         RequestIsWithinRange(size_t nSize);
        bool         GetCoreIsContiguous() const;

        // --- top-chunk / fence management (DWARF EAGeneralAllocator.h:1154..1163) ---------
        void   AdjustHighFence();
        void   SetNewTopChunk(Chunk* pChunk, bool bChunkIsNew);
        void   AdjustTopChunk(Chunk* pChunk, unsigned int nChunkSize);
        bool   ChunkMatchesLowHigh(int nBlockType, const Chunk* pChunk, unsigned int nChunkSize) const;
        int    ChunkMatchesBlockType(const Chunk* pChunk, int nBlockTypeFlags);
        void   SetFencepostAfterChunk(const Chunk* pChunk, unsigned int nPriorSize);
        void   AddDoubleFencepost(Chunk* pChunk, int nPrevInUse);
        Chunk* MakeChunkFromCore(void* pCore, size_t nCoreSize, int nFlags);
        void   GetBlockInfoForCoreBlock(const CoreBlock* pCoreBlock, BlockInfo* pBlockInfo) const;
        void   GetBlockInfoForChunk(const Chunk* pChunk, BlockInfo* pBlockInfo, const void* pCore) const;

        // --- chunk address / header bit helpers (DWARF EAGeneralAllocator.h:1167..1205) ---
        void*  GetPostHeaderPtrFromChunkPtr(const Chunk* pChunk);
        void*  GetDataPtrFromChunkPtr(const Chunk* pChunk);
        Chunk* GetChunkPtrFromDataPtr(const void* pData);
        unsigned int GetChunkSizeFromDataSize(unsigned int nDataSize);
        unsigned int GetChunkSizeFromDataSizeChecked(unsigned int nDataSize);
        unsigned int GetMMapChunkSizeFromDataSize(unsigned int nDataSize);
        Chunk* GetMMapChunkFromMMapListChunk(const Chunk* pMMapListChunk);
        int    GetChunkIsInternal(const Chunk* pChunk);
        void   SetChunkIsInternal(Chunk* pChunk);
        void   ClearChunkIsInternal(Chunk* pChunk);
        int    GetChunkIsFastBin(const Chunk* pChunk);
        void   SetChunkIsFastBin(Chunk* pChunk);
        void   ClearChunkIsFastBin(Chunk* pChunk);
        int    GetChunkIsMMapped(const Chunk* pChunk);
        int    GetPrevChunkIsInUse(const Chunk* pChunk);
        unsigned int GetChunkSize(const Chunk* pChunk);
        unsigned int GetUsableChunkSize(const Chunk* pChunk);
        Chunk* GetNextChunk(const Chunk* pChunk);
        Chunk* GetPrevChunk(const Chunk* pChunk);
        Chunk* GetChunkAtOffset(const Chunk* pChunk, int nOffset);
        int    GetChunkIsInUse(const Chunk* pChunk);
        void   SetChunkIsInUse(Chunk* pChunk);
        void   ClearChunkInUse(Chunk* pChunk);
        int    GetChunkInUseOffset(const Chunk* pChunk, int nOffset);
        void   SetChunkInUseOffset(Chunk* pChunk, int nOffset);
        void   ClearChunkInUseOffset(Chunk* pChunk, int nOffset);
        void   SetChunkSize(Chunk* pChunk, unsigned int nChunkSize);
        void   SetChunkSizePreserveFlags(Chunk* pChunk, unsigned int nChunkSize);
        unsigned int GetNextChunkPrevSize(const Chunk* pChunk, unsigned int nChunkSize);
        void   SetNextChunkPrevSize(Chunk* pChunk, unsigned int nPrevSize);
        void   LinkChunk(Chunk* pChunk, Chunk* pChunkPrev, Chunk* pChunkNext);
        void   UnlinkChunk(const Chunk* pChunk);
        Chunk* FindPriorChunk(const Chunk* pChunk) const;
        Chunk* GetInitialTopChunk() const;
        Chunk* FindAndSetNewTopChunk();
        bool   GetChunkIsListHead(const Chunk* pChunk) const;
        bool   GetChunkIsFenceChunk(const Chunk* pChunk) const;
        Chunk* GetFenceChunk(const CoreBlock* pCoreBlock);
        BinType FindChunkBin(const Chunk* pChunk) const;

        // --- bin access (DWARF EAGeneralAllocator.h:1208..1228) --------------------------
        // bin_at(i): the regular bins overlap successive Chunk fd/bk fields (the classic dlmalloc
        // trick). On x64 a Chunk's fd (mpPrevChunk) is at +8, so bin i (1-based) aliases the pair
        // mpBinArray[2*(i-1)] / mpBinArray[2*(i-1)+1]. Recovered behaviourally from the X360 bin
        // self-link loop (Init 0x82B4FA30) rather than by byte offset (the X86/PPC/x64 layouts differ).
        Chunk* GetBin(int i);
        Chunk* GetUnsortedBin() const;
        Chunk* GetFirstChunkInBin(const Chunk* pBin);
        Chunk* GetLastChunkInBin(const Chunk* pBin);
        Chunk* GetNextBin(Chunk* pBin);
        bool   SizeIsWithinSmallBinRange(unsigned int nChunkSize);
        int    GetSmallBinIndexFromChunkSize(unsigned int nChunkSize);
        int    GetLargeBinIndexFromChunkSize(unsigned int nChunkSize);
        int    GetBinIndexFromChunkSize(unsigned int nChunkSize);
        int    GetBinBitmapWordFromBinIndex(int nIndex);
        int    GetBinBitmapBitFromBinIndex(int nIndex);
        void   MarkBinBitmapBit(int nIndex);
        void   UnmarkBinBitmapBit(int nIndex);
        int    GetBinBitmapBit(int nIndex) const;
        unsigned int GetFastBinIndexFromChunkSize(unsigned int nChunkSize);
        int    GetFastBinChunksExist() const;
        void   SetFastBinChunksExist(bool bExist);
        unsigned int GetMaxFastBinChunkSize() const;
        void   SetMaxFastBinDataSize(unsigned int nDataSize);

        // --- core-block list (DWARF EAGeneralAllocator.h:1231..1233) ---------------------
        void       LinkCoreBlock(CoreBlock* pCoreBlock, CoreBlock* pCoreBlockPrev);
        void       UnlinkCoreBlock(CoreBlock* pCoreBlock);
        CoreBlock* FindCoreBlockForAddress(const void* pAddress) const;

        // --- consistency checks (DWARF EAGeneralAllocator.h:1236..1244) ------------------
        int    CheckChunk(const Chunk* pChunk);
        int    CheckFreeChunk(const Chunk* pChunk);
        int    CheckUsedChunk(const Chunk* pChunk);
        int    CheckRemallocedChunk(const Chunk* pChunk, unsigned int nRequestedChunkSize);
        int    CheckMallocedChunk(const Chunk* pChunk, unsigned int nRequestedChunkSize, bool bIsRemalloced, bool bMMapped);
        int    CheckMMappedChunk(const Chunk* pChunk);
        int    CheckState(HeapValidationLevel heapValidationLevel);
        size_t DescribeChunk(const Chunk* pChunk, char* pBuffer, size_t nBufferLength, bool bAppendLineEnd) const;
        DataType GetDataPreview(const void* pData, size_t nDataSize, char* pBuffer, wchar_t* pBufferW, size_t nBufferLength) const;

        // NOTE on the default sinks (DWARF EAGeneralAllocator.h:1245/1246 TraceFunctionDefault /
        // AssertionFailureFunctionDefault): both are declared above as STATIC (the form the ctor
        // installs into the mpTraceFunction / mpAssertionFailureFunction slots). The DWARF lists
        // them without an explicit this in the trace, consistent with the static signature; no
        // separate non-static overload is added (it would collide on identical parameter types).

    protected:
        // --- members (real order; see the PDB dump + X360 ctor/Init store offsets) ------
        bool         mbInitialized;
        unsigned int mnMaxFastBinChunkSize;
        Chunk*       mpFastBinArray[10];
        Chunk*       mpBinArray[256];
        unsigned int mBinBitmap[4];
        Chunk*       mpTopChunk;
        Chunk*       mpLastRemainderChunk;
        CoreBlock    mHeadCoreBlock;
        void*        mpHighFence;
        bool         mbHighFenceInternallyDisabled;
        bool         mbSystemAllocEnabled;
        int          mnCheckChunkReentrancyCount;
        unsigned char mcTraceFieldDelimiter;
        unsigned char mcTraceRecordDelimiter;
        HeapValidationLevel mAutoHeapValidationLevel;
        unsigned int mnAutoHeapValidationFrequency;
        unsigned int mnAutoHeapValidationEventCount;
        bool         mbHeapValidationActive;
        int          mnMMapCount;
        unsigned int mnMMapMallocTotal;
        int          mnMMapMaxAllowed;
        unsigned int mnMMapThreshold;
        bool         mbMMapTopDown;
        Chunk        mHeadMMapChunk;
        void         (*mpHookFunction)(HookInfo*, void*);
        void*        mpHookFunctionContext;
        bool         (*mpMallocFailureFunction)(GeneralAllocator*, unsigned int, unsigned int, void*);
        void*        mpMallocFailureFunctionContext;
        unsigned int mnMaxMallocFailureCount;
        void         (*mpAssertionFailureFunction)(const char*, void*);
        void*        mpAssertionFailureFunctionContext;
        void         (*mpTraceFunction)(const char*, void*);
        void*        mpTraceFunctionContext;
        unsigned int mnTrimThreshold;
        unsigned int mnTopPad;
        char*        mpInitialTopChunk;
        unsigned int mnPageSize;
        unsigned int mnNewCoreSize;
        unsigned int mnCoreIncrementSize;
        bool         mbTraceInternalMemory;
        void*        mpMutex;
        unsigned int mpMutexData[8];
        unsigned char mnFillFree;
        unsigned char mnFillDelayedFree;
        unsigned char mnFillNew;
        unsigned char mnFillGuard;
        unsigned char mnFillUnusedCore;
        const char*  mpName;
        char         mNotifyInitState;
        // DWARF EAGeneralAllocator.h:1335 - global head of the init-callback list (static).
        static volatile InitCallbackNode* mpInitCallbackNode;
    };
}
}
