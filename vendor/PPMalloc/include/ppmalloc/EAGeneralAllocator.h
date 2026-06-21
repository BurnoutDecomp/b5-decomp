#pragma once

#include <cstddef>   // size_t

// EA::Allocator::GeneralAllocator - EA's PPMalloc general-purpose heap allocator (the same EA
// middleware allocator Burnout's resource system allocates module memory through). It is a
// dlmalloc-style allocator: small/fast bins (mpFastBinArray), regular bins (mpBinArray) keyed
// by a bitmap (mBinBitmap), a top chunk, and a linked list of OS core blocks (mHeadCoreBlock).
//
// Layout (member names/types/order) recovered from Spore PC (EA::Allocator::GeneralAllocator, 
// sizeof=1316 on x86), dumped to progress/scratch_dossiers/ea_generalallocator_spore.txt. 
// Spore + Burnout share this EA middleware. The PDB is x86 (4-byte pointers); this header uses
// the real pointer types so the PC x64 build lays it out naturally (semantic parity - 
// field set/order match; pointer-sized members widen 4B->8B). The method BODIES 
// (Malloc/MallocAligned/MallocAlignedInternal/bin management) are reconstructed from the X360 
// pseudocode on top of this layout.
namespace EA
{
namespace Allocator
{
    class GeneralAllocator
    {
    public:
        enum HeapValidationLevel   // values TBD (not in the PDB layout dump)
        {
            kHeapValidationLevelNone  = 0,
            kHeapValidationLevelBasic = 1,
            kHeapValidationLevelDetail = 2,
            kHeapValidationLevelFull  = 3,
        };
        enum HookType    { kHookTypeMalloc = 0, kHookTypeFree = 1 };       // values TBD
        enum HookSubType { kHookSubTypeNone = 0 };                          // values TBD

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

        // --- API (bodies reconstructed from the X360 on top of this layout) -------------
        GeneralAllocator();
        virtual ~GeneralAllocator();

        bool  Init();
        void  Shutdown();
        // Adopt [pCore, pCore+nSize) as the heap's backing memory. @ 0x82B4F800.
        bool  AddCore(void* pCore, size_t nSize, bool bShouldFree = false, bool bShouldFreeOnShutdown = false);
        void* Malloc(size_t nSize, int nAllocationFlags = 0);
        void* MallocAligned(size_t nSize, size_t nAlignment, size_t nAlignmentOffset = 0, int nAllocationFlags = 0);  // @ 0x82B515B0
        void* Calloc(size_t nElementCount, size_t nElementSize, int nAllocationFlags = 0);
        void* Realloc(void* pData, size_t nNewSize, int nAllocationFlags = 0);
        void  Free(void* pData);
        // True if pData lies within this allocator's adopted core (used to route Free between
        // sibling allocators, e.g. rw::core::GeneralResourceAllocator's main vs physical heaps).
        bool  Owns(const void* pData) const;
        void  SetName(const char* pName);
        const char* GetName() const;

    protected:
        void* MallocInternal(size_t nSize, int nAllocationFlags);
        void* MallocAlignedInternal(size_t nSize, size_t nAlignment, size_t nAlignmentOffset, int nAllocationFlags);

        // Default assertion/trace sinks the ctor installs (X360 AssertionFailureFunctionDefault
        // 0x82B4EDC8 / TraceFunctionDefault 0x82B4DC98).
        static void AssertionFailureFunctionDefault(const char* pMessage, void* pContext);
        static void TraceFunctionDefault(const char* pMessage, void* pContext);

        // bin_at(i): the regular bins overlap successive Chunk fd/bk fields (the classic dlmalloc
        // trick). On x64 a Chunk's fd (mpPrevChunk) is at +8, so bin i (1-based) aliases the pair
        // mpBinArray[2*(i-1)] / mpBinArray[2*(i-1)+1]. Recovered behaviourally from the X360 bin
        // self-link loop (Init 0x82B4FA30) rather than by byte offset (the X86/PPC/x64 layouts differ).
        Chunk* GetBin(int i);

    private:
        // --- members (real order; see the PDB dump) -------------------------------------
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
    };
}
}
