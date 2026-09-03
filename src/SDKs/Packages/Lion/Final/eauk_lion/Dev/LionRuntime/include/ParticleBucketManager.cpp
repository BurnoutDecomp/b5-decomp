// ============================================================================
// ParticleBucketManager.cpp -- cParticleBucketManager runtime bodies.
//
// Vendor code (eauk_lion), reconstructed store-for-store from the X360 ARTIST asm:
//   0x82912E00 cParticleBucketManager::AppInit
//   0x82909600 cParticleBucketManager::AppDeInit
//   0x829145A0 cParticleBucketManager::AllocateBucket
//   0x82913050 cParticleBucketManager::BucketAlloc
//   0x8290F378 cParticleBucketManager::Free
//   0x8290CDD8 cParticleBucketManager::VectorBucketAlloc
// against the DecFIGS DWARF layout in ParticleBucketManager.h. Members are addressed
// by name; the free/used lists thread through each bucket's mpManagerNext.
//
// MatrixBucketAlloc (heavyweight side-pool twin of VectorBucketAlloc) and
// cParticleEmitter::BucketRemove are external to this TU -- declared in their homes and
// linked from their own TUs.
// ============================================================================

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleBucketManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace
{
// The X360 build tags every Lion allocation with a (line, file, name) TagValuePair chain.
const char* const KPC_MANAGER_FILE =
    "d:\\p4\\b5_main\\burnout\\main\\code\\sdks\\packages\\lion\\final\\eauk_lion\\dev\\lionruntime\\include/ParticleBucketManager.cpp";
const char* const KPC_TAG_NAME_BUCKETS  = "Lion::Buckets::";
const char* const KPC_TAG_NAME_MATRICES = "Lion::Matrices::";
const char* const KPC_TAG_NAME_VECBITS  = "Lion::VectorBucketActiveBits::";

// Tag ids used by the chain (matching the X360 TagValuePair construction order).
const u32 KU_TAG_NAME = 1;
const u32 KU_TAG_FILE = 5;
const u32 KU_TAG_LINE = 6;

// Source lines the X360 build stamps onto each allocation (the tag-6 values 0x25/0x26/0x3C).
const s32 KI_LINE_BUCKETS  = 37;
const s32 KI_LINE_MATRICES = 38;
const s32 KI_LINE_VECBITS  = 60;

// Per-particle side-array element count and the derived byte strides of the shared pool.
// cVector / cMatrix are pointer-free float records, so these equal the X360 strides.
const u32 KU_VECTOR_BUCKET_STRIDE = 16u * sizeof(cVector);   // 256 bytes
const u32 KU_MATRIX_BUCKET_STRIDE = 16u * sizeof(cMatrix);   // 1024 bytes

// Byte-stride pins: the shift/mask arithmetic below bakes 256 (<<8) and 1024 (>>10).
static void _AssertStrides()
{
    static_assert(KU_VECTOR_BUCKET_STRIDE == 256, "vector bucket stride == 256 (16 * cVector)");
    static_assert(KU_MATRIX_BUCKET_STRIDE == 1024, "matrix bucket stride == 1024 (16 * cMatrix)");
}
}  // namespace

// cParticleBucketManager::AppInit @ 0x82912E00
void cParticleBucketManager::AppInit(EA::Allocator::ITaggedAllocator* apAllocator,
                                     u32 auBucketCount, u32 auMatrixBucketCount)
{
    mpAllocator        = apAllocator;
    mMatrixBucketCount = auMatrixBucketCount;
    mBucketCount       = auBucketCount;

    // The bucket array (stride == sizeof(cParticleBucket); X360 mulli by 0xE70).
    EA::TagValuePair lBucketsLine(KU_TAG_LINE, KI_LINE_BUCKETS);
    EA::TagValuePair lBucketsFile(KU_TAG_FILE, static_cast<const void*>(KPC_MANAGER_FILE));
    EA::TagValuePair lBucketsName(KU_TAG_NAME, static_cast<const void*>(KPC_TAG_NAME_BUCKETS));
    mpBuckets = static_cast<cParticleBucket*>(mpAllocator->Alloc(
        static_cast<size_t>(mBucketCount) * sizeof(cParticleBucket),
        lBucketsLine + lBucketsFile + lBucketsName));

    // The shared side-data pool (sized for mMatrixBucketCount matrix buckets == 1024B each;
    // VectorBucketAlloc re-carves it as 4x that many 256B vector buckets).
    EA::TagValuePair lMatricesLine(KU_TAG_LINE, KI_LINE_MATRICES);
    EA::TagValuePair lMatricesFile(KU_TAG_FILE, static_cast<const void*>(KPC_MANAGER_FILE));
    EA::TagValuePair lMatricesName(KU_TAG_NAME, static_cast<const void*>(KPC_TAG_NAME_MATRICES));
    mpMatrices = static_cast<cMatrix*>(mpAllocator->Alloc(
        static_cast<size_t>(mMatrixBucketCount) * KU_MATRIX_BUCKET_STRIDE,
        lMatricesLine + lMatricesFile + lMatricesName));

    mpFree = nullptr;
    mpUsed = nullptr;

    // Clear every bucket's manager/emitter links, side-array pointers, live count and
    // active mask, and seed its RNG.
    for (u32 lIndex = 0; lIndex < mBucketCount; ++lIndex)
    {
        cParticleBucket& lrBucket = mpBuckets[lIndex];
        lrBucket.mpManagerNext                = nullptr;
        lrBucket.mpEmitter                    = nullptr;
        lrBucket.mpEmitterNext                = nullptr;
        lrBucket.mpMatrices                   = nullptr;
        lrBucket.mpVectors                    = nullptr;
        lrBucket.mnNextParticlePositionToFill = 0;
        lrBucket.mActiveBits                  = 0;
        lrBucket.mRandomSeed.Init();
    }

    // Thread the free list: each bucket points at the next; the last one stays null.
    for (u32 lIndex = 0; lIndex + 1 < mBucketCount; ++lIndex)
    {
        mpBuckets[lIndex].mpManagerNext = &mpBuckets[lIndex + 1];
    }

    // Occupancy bitmap: one bit per vector bucket (mVectorBucketCount == 4x matrices).
    mVectorBucketCount = 4u * mMatrixBucketCount;
    mVectorChunkCount  = (mVectorBucketCount + 31u) >> 5;

    EA::TagValuePair lBitsLine(KU_TAG_LINE, KI_LINE_VECBITS);
    EA::TagValuePair lBitsFile(KU_TAG_FILE, static_cast<const void*>(KPC_MANAGER_FILE));
    EA::TagValuePair lBitsName(KU_TAG_NAME, static_cast<const void*>(KPC_TAG_NAME_VECBITS));
    mpVectorBucketActiveBits = static_cast<u32*>(mpAllocator->Alloc(
        static_cast<size_t>(mVectorChunkCount) * sizeof(u32),
        lBitsLine + lBitsFile + lBitsName));

    for (u32 lIndex = 0; lIndex < mVectorChunkCount; ++lIndex)
    {
        mpVectorBucketActiveBits[lIndex] = 0;
    }

    mpFree = mpBuckets;
}

// cParticleBucketManager::AppDeInit @ 0x82909600
void cParticleBucketManager::AppDeInit()
{
    mpAllocator->Free(mpVectorBucketActiveBits, 0);
    mpVectorBucketActiveBits = nullptr;

    mpAllocator->Free(mpBuckets, 0);
    mpBuckets = nullptr;

    mpAllocator->Free(mpMatrices, 0);
    mpMatrices = nullptr;
}

// Remove apBucket from the singly-linked (mpManagerNext) list headed by arpHead.
void cParticleBucketManager::ListRemove(cParticleBucket*& arpHead, cParticleBucket* apBucket)
{
    if (!arpHead)
    {
        return;
    }

    cParticleBucket** lppLink = &arpHead;
    cParticleBucket* lpNode = arpHead;
    while (lpNode != apBucket)
    {
        lppLink = &lpNode->mpManagerNext;
        lpNode = lpNode->mpManagerNext;
        if (!lpNode)
        {
            return;  // not on this list
        }
    }

    *lppLink = lpNode->mpManagerNext;
    lpNode->mpManagerNext = nullptr;
}

// cParticleBucketManager::BucketAlloc @ 0x82913050
cParticleBucket* cParticleBucketManager::BucketAlloc(u32 /*auCount*/, const cTime& arTime)
{
    // Fast path: hand back the head of the free list.
    cParticleBucket* lpBucket = mpFree;
    if (lpBucket)
    {
        ListRemove(mpFree, lpBucket);
        lpBucket->mpManagerNext = mpUsed;
        mpUsed = lpBucket;
        return lpBucket;
    }

    // Free list empty: evict a used bucket. Reclaim the first idle one (mActiveBits == 0),
    // else the one whose birth time is furthest from arTime.
    cParticleBucket* lpUsed = mpUsed;
    if (!lpUsed)
    {
        return nullptr;
    }

    cParticleBucket* lpVictim = nullptr;
    s32 lBestDelta = 0;
    for (;;)
    {
        if (lpUsed->mActiveBits == 0)
        {
            lpVictim = lpUsed;
            break;
        }

        s32 lDelta = arTime.GetTimeDiff(lpUsed->mLatestBirthTime);
        if (lDelta < 0)
        {
            lDelta = -lDelta;
        }
        if (lDelta > lBestDelta)
        {
            lpVictim = lpUsed;
            lBestDelta = lDelta;
        }

        lpUsed = lpUsed->mpManagerNext;
        if (!lpUsed)
        {
            break;
        }
    }

    if (!lpVictim)
    {
        return nullptr;
    }

    // Free() recycles the victim onto the free list; pull it straight back onto the used list.
    Free(lpVictim);
    ListRemove(mpFree, lpVictim);
    lpVictim->mpManagerNext = mpUsed;
    mpUsed = lpVictim;
    return lpVictim;
}

// cParticleBucketManager::AllocateBucket @ 0x829145A0
cParticleBucket* cParticleBucketManager::AllocateBucket(u32 auCount, const cTime& arTime,
                                                        cParticleDescriptor::BucketType aeType)
{
    cParticleBucket* lpBucket = BucketAlloc(auCount, arTime);
    if (!lpBucket)
    {
        return nullptr;
    }

    switch (aeType)
    {
        case cParticleDescriptor::E_BUCKET_LIGHT:
            lpBucket->mpMatrices = nullptr;
            lpBucket->mpVectors  = VectorBucketAlloc();
            break;

        case cParticleDescriptor::E_BUCKET_HEAVY:
            lpBucket->mpMatrices = MatrixBucketAlloc();
            lpBucket->mpVectors  = nullptr;
            break;

        default:
            lpBucket->mpMatrices = nullptr;
            lpBucket->mpVectors  = nullptr;
            break;
    }

    lpBucket->mnNextParticlePositionToFill = 0;
    lpBucket->mActiveBits                  = 0;
    return lpBucket;
}

// cParticleBucketManager::Free @ 0x8290F378
void cParticleBucketManager::Free(cParticleBucket* apBucket)
{
    if (!apBucket)
    {
        return;
    }

    // Release the matrix side array: clear its nibble (four vector-bit slots) in the map.
    if (apBucket->mpMatrices)
    {
        u32 lMatIndex = static_cast<u32>(
            reinterpret_cast<u8*>(apBucket->mpMatrices) - reinterpret_cast<u8*>(mpMatrices))
            / KU_MATRIX_BUCKET_STRIDE;
        u32 lMask = 0xF0000000u >> ((lMatIndex & 7u) * 4u);
        mpVectorBucketActiveBits[lMatIndex >> 3] &= ~lMask;
        apBucket->mpMatrices = nullptr;
    }

    // Release the vector side array: clear its single bit in the map.
    if (apBucket->mpVectors)
    {
        u32 lVecIndex = static_cast<u32>(
            reinterpret_cast<u8*>(apBucket->mpVectors) - reinterpret_cast<u8*>(mpMatrices))
            / KU_VECTOR_BUCKET_STRIDE;
        u32 lBit = 1u << (31u - (lVecIndex & 0x1Fu));
        mpVectorBucketActiveBits[lVecIndex >> 5] &= ~lBit;
        apBucket->mpVectors = nullptr;
    }

    // Tell the owning emitter to drop this bucket.
    if (apBucket->mpEmitter)
    {
        apBucket->mpEmitter->BucketRemove(apBucket);
    }

    // Unlink from the used list, then push onto the head of the free list.
    ListRemove(mpUsed, apBucket);
    apBucket->mpManagerNext = mpFree;
    mpFree = apBucket;
}

// cParticleBucketManager::VectorBucketAlloc @ 0x8290CDD8
cVector* cParticleBucketManager::VectorBucketAlloc()
{
    u32 lIndex = mVectorBucketCount;
    if (lIndex == 0)
    {
        return nullptr;
    }

    cVector* lpResult = nullptr;
    s32 lByteOffset = static_cast<s32>(lIndex * KU_VECTOR_BUCKET_STRIDE);
    do
    {
        if (lpResult)
        {
            break;
        }

        --lIndex;
        lByteOffset -= static_cast<s32>(KU_VECTOR_BUCKET_STRIDE);

        u32 lBit = 1u << (31u - (lIndex & 0x1Fu));
        if ((mpVectorBucketActiveBits[lIndex >> 5] & lBit) == 0)
        {
            lpResult = reinterpret_cast<cVector*>(
                reinterpret_cast<u8*>(mpMatrices) + lByteOffset);
            mpVectorBucketActiveBits[lIndex >> 5] |= lBit;
        }
    }
    while (lIndex != 0);

    return lpResult;
}

// ------------------------------------------------------------------------------------------------
// cParticleBucketManager::Instance (DWARF ParticleBucketManager.h:36)
//
// NO STANDALONE X360 BODY -- inlined at every call site, which is why cParticleSystem::AppInit
// @0x82913810 and AppDeInit @0x82911DF0 both pass the literal `&unk_831238C0` as `this`.
//
// A file-scope object, NOT a function-local static: the console has no magic-static guard word
// beside it (the neighbouring .bss is the emitter-manager singleton at 0x831238E8, 0x2C bytes
// later), and adding one would give first use an ordering the console does not have.
// ------------------------------------------------------------------------------------------------
namespace
{
    cParticleBucketManager gBucketManagerSingleton;   // X360 unk_831238C0
}

cParticleBucketManager& cParticleBucketManager::Instance()
{
    return gBucketManagerSingleton;
}

// ------------------------------------------------------------------------------------------------
// cParticleBucketManager::MatrixBucketAlloc @0x8290CD60 -- LOUD TRAP, not a body.
//
// An EXPORT-SET HOLE: IDA names it in AllocateBucket's xrefs and emits no JSON for it, so it has
// no ledger row and no pseudocode. It is the 1024-byte twin of VectorBucketAlloc -- claim a free
// matrix-bucket slot from the shared side pool and mark its bit -- and it is reached only when a
// descriptor asks for the heavyweight bucket type, i.e. only once emitters actually run. Defined
// here, in its own class's TU, because the Lion install path put AllocateBucket on the link.
// ------------------------------------------------------------------------------------------------
cMatrix* cParticleBucketManager::MatrixBucketAlloc()
{
    CGS_ASSERT(false, "cParticleBucketManager::MatrixBucketAlloc @0x8290CD60 -- NOT RECONSTRUCTED (export-set hole)");
    return 0;
}
