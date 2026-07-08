#include "GameShared/GameClasses/Sound/Playback/Splicer/SpliceManager.h"

#include "rw/audio/core/Voice.h"   // rw::audio::core::Voice::Release

// The RWAC engine's 3-arg plug-in event form (X360 rw::audio::core::PlugIn::Event with
// (self, event, arg)). Declared BY NAME here exactly as in CgsEnvironment.h, mirroring
// the committed RwacSystemLock / RwacSystemUnlock house entries; bodied in its own TU.
namespace rw { namespace audio { namespace core {
    void RwacPlugInEvent( PlugIn* apPlugIn, int aiEvent, int aiArg );
} } }

// The global SpliceManager singleton (X360 off_82FFB9F0). Its home is this TU; the ctor
// installs `this` into it. The bank/statistics code reads it through the extern in
// CgsSpliceBankStatistics.h / SpliceManager.h.
SpliceManager* gpSpliceManager = 0;

// ============================================================================
// SpliceManager::Allocate @ 0x826AD630
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX. The X360 forwards
// a splice block allocation through the owned heap's polymorphic allocator:
//
//   r11 = this->mpHeap;                     (lwz r11, 0x6C4(r3))
//   r4  = r11->vtable... no -- r4 = *(r11 + 0x30)  -- the allocator object
//   build descriptor on the stack:
//       HIDWORD(v5) = size;  LODWORD(v5) = 4;      (size, alignment 4)
//       v8=v10=v12=v14 = size;  v9=v11=v13=v15 = 1;  (four (size,1) pool hints)
//       v7 = v5;
//   r3  = &result-buffer (var_50);
//   call (*(*allocator + 0x10))(&result, allocator, &v7, tag);
//   return *(&result);                      (lwz r3, 0(r3))
//
// The heap object the descriptor is handed to is loaded from this->mpHeap and
// then dereferenced one level (the X360 `lwz r4, 0x30(r11)` selects the embedded
// allocator). That embedded-allocator selection is an internal heap detail not
// resolvable from this TU; it is modelled here as the heap forwarding to its own
// polymorphic Allocate slot. Semantic parity (build this descriptor, forward it
// to the heap's allocate vtable entry, return the block) is preserved; the exact
// guest sub-object hop is documented rather than reproduced via a raw +0x30 cast.
// ============================================================================

void* SpliceManager::Allocate( u32 luSize, const char* lpcTag )
{
    SpliceManagerDetail::AllocationRequest lRequest;

    // HIDWORD(v5) = a2 (size) ; LODWORD(v5) = 4 (alignment).
    lRequest.miAlignment = 4;
    lRequest.miSize      = static_cast<s32>( luSize );

    // asm @0x826AD660: `li r10,0` then four `stw r10` into the even slots, and
    // `li r11,1` then four `stw r11` into the odd slots -- so the four pool-size
    // hints are 0 and the four flags are 1 (the Hex-Rays "= size" was a HIDWORD
    // alias of the {size,align} qword, NOT a real store of the size here).
    for ( s32 li = 0; li < 4; ++li )
    {
        lRequest.maiPoolSize[li] = 0;
        lRequest.maiPoolFlag[li] = 1;
    }

    // Forward to the heap's polymorphic allocator (vtable slot 0x10 / index 4)
    // with the result buffer, the heap, the descriptor, and the tag; return the
    // block address the call writes into the result buffer (0 on failure).
    void* lpResult = 0;
    SpliceManagerDetail::Heap* lpHeap = mpHeap;
    lpResult = lpHeap->mpVTable->mpfnAllocate( &lpResult, lpHeap, &lRequest, lpcTag );

    return lpResult;
}

// ============================================================================
// SpliceManager::FindSplice @ 0x826A3238
//
// Look a splice record up in the `aeType` bank. Reconstructed from the X360:
//   container = m_Splices[aeType];               (r30 = 20*type + this)
//   if (container.mNumSplices <= 0) assert("No splices to Find, Empty Splice!");
//   clamp aiIndex into [0, mNumSplices-1];
//   return container.mpHeadSplice + aiIndex;      (mpHeadSplice + 24*index)
// The assert sink is read from the GLOBAL instance (off_82FFB9F0->mAssertCallbackFunc),
// exactly as the asm does, not from `this`.
// ============================================================================

SPLICE_Data* SpliceManager::FindSplice( SPLICE_TYPE aeType, int aiIndex )
{
    SpliceContainer& lrContainer = m_Splices[aeType];

    if ( lrContainer.mNumSplices <= 0 )
    {
        AssertCallbackFunc lpfnAssert = gpSpliceManager->mAssertCallbackFunc;
        if ( lpfnAssert )
            lpfnAssert( "No splices to Find, Empty Splice!" );
    }

    const int liCount = lrContainer.mNumSplices;
    if ( aiIndex >= liCount )
        aiIndex = liCount - 1;
    if ( aiIndex < 0 )
        aiIndex = 0;

    return lrContainer.mpHeadSplice + aiIndex;
}

// ============================================================================
// SpliceManager::DestroyVoice @ 0x826A3488
//
// Release a voice pair created for a splice sample. Reconstructed from the X360:
//   * assert the pair is non-null ("NULL voice plugin pair");
//   * assert its voice is non-null ("Trying to destroy NULL voice");
//   * assert the voice is NOT one of the pooled mono voices ("... mono pooled voice")
//     nor one of the pooled stereo voices ("... stereo pooled voice") -- a pooled voice
//     must be recycled, not destroyed;
//   * release the voice through the RWAC engine and null the pair.
// The pooled-voice scans compare each pool pair's voice against the pair's voice, over
// the first muPooledVoiceCount entries. The assert sink is the global instance's.
// ============================================================================

void SpliceManager::DestroyVoice( VoicePluginPair* apVoicePluginPair )
{
    if ( !apVoicePluginPair )
    {
        AssertCallbackFunc lpfnAssert = gpSpliceManager->mAssertCallbackFunc;
        if ( lpfnAssert )
            lpfnAssert( "NULL voice plugin pair" );
    }

    if ( !apVoicePluginPair->mpVoice )
    {
        AssertCallbackFunc lpfnAssert = gpSpliceManager->mAssertCallbackFunc;
        if ( lpfnAssert )
            lpfnAssert( "Trying to destroy NULL voice" );
    }

    // Scan the mono pool for a pair whose voice matches the one being destroyed.
    bool lbInMonoPool = false;
    for ( u32 lu = 0; lu < mMonoVoicePool.muPooledVoiceCount; ++lu )
    {
        if ( mMonoVoicePool.maVoicePluginPairs[lu].mpVoice == apVoicePluginPair->mpVoice )
        {
            lbInMonoPool = true;
            break;
        }
    }
    if ( lbInMonoPool )
    {
        AssertCallbackFunc lpfnAssert = gpSpliceManager->mAssertCallbackFunc;
        if ( lpfnAssert )
            lpfnAssert( "Trying to destroy a mono pooled voice" );
    }

    // Scan the stereo pool likewise.
    bool lbInStereoPool = false;
    for ( u32 lu = 0; lu < mStereoVoicePool.muPooledVoiceCount; ++lu )
    {
        if ( mStereoVoicePool.maVoicePluginPairs[lu].mpVoice == apVoicePluginPair->mpVoice )
        {
            lbInStereoPool = true;
            break;
        }
    }
    if ( lbInStereoPool )
    {
        AssertCallbackFunc lpfnAssert = gpSpliceManager->mAssertCallbackFunc;
        if ( lpfnAssert )
            lpfnAssert( "Trying to destroy a stereo pooled voice" );
    }

    rw::audio::core::Voice::Release( apVoicePluginPair->mpVoice );
    apVoicePluginPair->mpVoice   = 0;
    apVoicePluginPair->mppPlugIn = 0;
}

// ============================================================================
// SpliceManager::FreeMonoVoicePlugInPair  @ 0x826A35E8
// SpliceManager::FreeStereoVoicePlugInPair @ 0x826A3640
//
// Return a voice pair to its pool. Reconstructed from the X360: fire the plug-in chain's
// stop event (RWAC PlugIn::Event(chainHead, 1, 0), where the chain head is *mppPlugIn)
// and push the pair back onto the corresponding pool's free stack.
// ============================================================================

void SpliceManager::FreeMonoVoicePlugInPair( VoicePluginPair* apVoicePluginPair )
{
    rw::audio::core::RwacPlugInEvent( *apVoicePluginPair->mppPlugIn, 1, 0 );
    mMonoVoicePool.FreeVoicePluginPair( apVoicePluginPair );
}

void SpliceManager::FreeStereoVoicePlugInPair( VoicePluginPair* apVoicePluginPair )
{
    rw::audio::core::RwacPlugInEvent( *apVoicePluginPair->mppPlugIn, 1, 0 );
    mStereoVoicePool.FreeVoicePluginPair( apVoicePluginPair );
}

// ============================================================================
// SpliceManager::VoicePool::AllocateVoicePluginPairToSpliceSample @ 0x8268AD60
//
// Pop the next free pooled pair off the stack. Reconstructed from the X360:
//   idx = miPooledVoiceStackFreeIndex;
//   if (idx < 0) return 0;                              // stack exhausted
//   if (idx >= muPooledVoiceCount) assert("Pooled voice stack index too big");
//   result = mapVoicePluginPairsStack[idx];
//   miPooledVoiceStackFreeIndex = idx - 1;
//   return result;
// The SpliceSample argument is part of the DWARF signature but the body does not use it.
// ============================================================================

SpliceManager::VoicePluginPair*
SpliceManager::VoicePool::AllocateVoicePluginPairToSpliceSample( SpliceSample* /*apSpliceSample*/ )
{
    const s32 liFreeIndex = miPooledVoiceStackFreeIndex;
    if ( liFreeIndex < 0 )
        return 0;

    if ( liFreeIndex >= static_cast<s32>( muPooledVoiceCount ) )
    {
        SpliceManager::AssertCallbackFunc lpfnAssert = gpSpliceManager->mAssertCallbackFunc;
        if ( lpfnAssert )
            lpfnAssert( "Pooled voice stack index too big" );
    }

    VoicePluginPair* lpResult = mapVoicePluginPairsStack[miPooledVoiceStackFreeIndex];
    miPooledVoiceStackFreeIndex = liFreeIndex - 1;
    return lpResult;
}

// ============================================================================
// SpliceManager::VoicePool::FreeVoicePluginPair @ 0x8268ADF8
//
// Push a pair back onto the free stack. Reconstructed from the X360:
//   miPooledVoiceStackFreeIndex += 1;
//   if (miPooledVoiceStackFreeIndex >= muPooledVoiceCount)
//       assert("Pooled voice stack was already full");
//   mapVoicePluginPairsStack[miPooledVoiceStackFreeIndex] = apVoicePluginPair;
// ============================================================================

void SpliceManager::VoicePool::FreeVoicePluginPair( VoicePluginPair* apVoicePluginPair )
{
    miPooledVoiceStackFreeIndex = miPooledVoiceStackFreeIndex + 1;

    if ( miPooledVoiceStackFreeIndex >= static_cast<s32>( muPooledVoiceCount ) )
    {
        SpliceManager::AssertCallbackFunc lpfnAssert = gpSpliceManager->mAssertCallbackFunc;
        if ( lpfnAssert )
            lpfnAssert( "Pooled voice stack was already full" );
    }

    mapVoicePluginPairsStack[miPooledVoiceStackFreeIndex] = apVoicePluginPair;
}
