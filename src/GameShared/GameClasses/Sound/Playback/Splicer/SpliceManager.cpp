#include "GameShared/GameClasses/Sound/Playback/Splicer/SpliceManager.h"

#include "rw/audio/core/Voice.h"   // rw::audio::core::Voice::Release
#include "rw/audio/core/PlugIn.h"  // System::GetPlugInRegistry / PlugInRegistry::GetPlugInHandle
#include "rw/rwcore_structs.h"     // rw::IResourceAllocator / BaseResourceDescriptors (Allocate)
#include "GameShared/GameClasses/Sound/Playback/CgsEnvironment.h"           // Environment::GetAllocator
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacFactory.h" // GetDefaultRwacSystem + RwacLock

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
    // REWORKED (AEMS-cascade slice 3): the descriptor the asm builds -- {size,
    // alignment 4} + four (0,1) pairs -- IS the standard rw five-pair request
    // (the identical stores every AllocateMemoryResource site makes), and the
    // "+0x30 hop off the +0x6C4 object" is Environment::GetAllocator. The former
    // Heap-view call carried the console's result-first ABI and could never
    // dispatch a host allocator vtable; this is the same real DoAllocate path
    // the rest of the playback engine uses.
    rw::BaseResourceDescriptors<5> lDescriptor;
    for ( u32 luEntry = 0u; luEntry < 5u; ++luEntry )
    {
        lDescriptor.m_baseResourceDescriptors[luEntry].m_size      = 0u;
        lDescriptor.m_baseResourceDescriptors[luEntry].m_alignment = 1u;
    }
    lDescriptor.m_baseResourceDescriptors[0].m_size      = luSize;
    lDescriptor.m_baseResourceDescriptors[0].m_alignment = 4u;

    rw::Resource lResource =
        const_cast<CgsSound::Playback::Environment*>( mpEnvironment )->GetAllocator()->DoAllocate(
            reinterpret_cast<const rw::ResourceDescriptor&>( lDescriptor ), lpcTag );
    return lResource.m_baseResources[0];
}

// ============================================================================
// SpliceManager ctor @ 0x826C30C8  (AEMS-cascade slice 3; the full store-order
// decode is progress/scratch_dossiers/aems_factory_cascade_codex.md).
// Console order: the eight inlined SpliceContainer zero-ctors (+0x614..+0x6B3;
// the member array's default construction below), the assert sink cleared
// (+0x610), the environment saved (+0x6C4), the global published (off_82FFB9F0),
// then -- with the global system locked -- the four plug-in handle lookups
// ('SnP1' +0x6B4, 'Rsp0' +0x6B8, 'Sen0' +0x6C0 stored BEFORE 'Pn21' +0x6BC,
// exactly the asm's non-monotonic order), the two pool count words zeroed, the
// voice staging, and the two pool Prepares.
//
// FLAG [the voice staging is DEFERRED]: the console builds auMonoVoiceCount
// four-stage pairs via CreateMonoVoice (stage configs {&1.0f (flt_82001C98),
// 'SnP1', 1ch} {0,'Rsp0',1} {0,'Pn21',6} {0,'Sen0',6}) and auStereoVoiceCount
// three-stage pairs via CreateStereoVoice ({&2.0f (flt_82001D9C), 'SnP1', 2}
// {0,'Rsp0',2} {0,'Sen0',2}), then VoicePool::Prepare's them (@0x8268AC40,
// bodied below). On this build the 'SnP1'/'Rsp0'/'Pn21' handles are still null
// (SndPlayer1 has no PC home; Resample::Process / Pan2D1::CreateInstance decode
// in flight) and Voice::CreateInstance dereferences the descriptor a handle IS
// -- staging would crash rather than fail guarded. The pools stay EMPTY with
// their free stacks parked at -1, so every Allocate*VoicePluginPair pops null
// through its guarded path. Light the staging when the three plug-ins register.
// ============================================================================

SpliceManager::SpliceManager( const CgsSound::Playback::Environment& arEnvironment,
                              u32 auMonoVoiceCount, u32 auStereoVoiceCount )
{
    // (m_Splices[8] default-construct == the eight inlined zero-ctors.)
    mAssertCallbackFunc = 0;                 // +0x610
    mpEnvironment       = &arEnvironment;    // +0x6C4 (the saved r4)
    gpSpliceManager     = this;              // off_82FFB9F0

    {
        rw::audio::core::System* lpSystem = CgsSound::Playback::GetDefaultRwacSystem();
        CgsSound::Playback::RwacLock lLock( lpSystem );

        rw::audio::core::PlugInRegistry* lpRegistry =
            rw::audio::core::System::GetPlugInRegistry( lpSystem );

        sndplayerHandle = rw::audio::core::PlugInRegistry::GetPlugInHandle( lpRegistry, 0x536E5031 ); // 'SnP1' +0x6B4
        resampleHandle  = rw::audio::core::PlugInRegistry::GetPlugInHandle( lpRegistry, 0x52737030 ); // 'Rsp0' +0x6B8
        sendHandle      = rw::audio::core::PlugInRegistry::GetPlugInHandle( lpRegistry, 0x53656E30 ); // 'Sen0' +0x6C0
        pannerHandle    = rw::audio::core::PlugInRegistry::GetPlugInHandle( lpRegistry, 0x506E3231 ); // 'Pn21' +0x6BC

        mMonoVoicePool.muPooledVoiceCount   = 0;   // +0x300 (the console pre-Prepare zero)
        mStereoVoicePool.muPooledVoiceCount = 0;   // +0x608

        // FLAG [deferred voice staging -- see the banner]: the free stacks park
        // at -1 (VoicePool::Prepare seeds count-1 when the staging lands).
        mMonoVoicePool.miPooledVoiceStackFreeIndex   = -1;
        mStereoVoicePool.miPooledVoiceStackFreeIndex = -1;
        (void)auMonoVoiceCount;
        (void)auStereoVoiceCount;
    }   // ~RwacLock == the console unlock
}

// The eight-splice-bank zero ctor (SpliceManager.h:22; the manager ctor inlines
// it eight times on console -- all five members zeroed).
SpliceManager::SpliceContainer::SpliceContainer()
    : mpSampleData( 0 )
    , mpTableOfContents( 0 )
    , mNumSplices( 0 )
    , mNumSamples( 0 )
    , mpHeadSplice( 0 )
{
}

// ============================================================================
// SpliceManager::VoicePool::Prepare @ 0x8268AC40  (dossier re-exported for this
// slice; register-level). Bounds callbacks through the GLOBAL manager's sink
// (+0x610), then the pair copy + free-stack build:
//   if (count > 0x40)  callback("Too many voices for pool");
//   if (count == 0)    callback("Must have at least 1 voice");
//   if (pairs == 0)    callback("No voices passed in");
//   for each pair: null-voice callback("Null voice pointer"); copy {voice,
//     plugins} into maVoicePluginPairs[i]; free stack slot i = &pairs[i];
//   muPooledVoiceCount = count; miPooledVoiceStackFreeIndex = count - 1;
//   return true.
// (The console walks the source through a dest-relative delta and always
// returns 1; the copy is reproduced by name.)
// ============================================================================
bool SpliceManager::VoicePool::Prepare( VoicePluginPair* apaVoicePairs, u32 auVoiceCount )
{
    SpliceManager::AssertCallbackFunc lpfnAssert =
        gpSpliceManager ? gpSpliceManager->mAssertCallbackFunc : 0;

    if ( auVoiceCount > SpliceManager::KU_MAX_POOLED_VOICES && lpfnAssert )
        lpfnAssert( "Too many voices for pool" );
    if ( auVoiceCount == 0 && lpfnAssert )
        lpfnAssert( "Must have at least 1 voice" );
    if ( apaVoicePairs == 0 && lpfnAssert )
        lpfnAssert( "No voices passed in" );

    for ( u32 lu = 0; lu < auVoiceCount; ++lu )
    {
        if ( apaVoicePairs[lu].mpVoice == 0 && lpfnAssert )
            lpfnAssert( "Null voice pointer" );
        maVoicePluginPairs[lu].mpVoice   = apaVoicePairs[lu].mpVoice;
        maVoicePluginPairs[lu].mppPlugIn = apaVoicePairs[lu].mppPlugIn;
        mapVoicePluginPairsStack[lu]     = &maVoicePluginPairs[lu];
    }

    muPooledVoiceCount          = auVoiceCount;                          // +0x300
    miPooledVoiceStackFreeIndex = static_cast<s32>( auVoiceCount ) - 1;  // +0x304
    return true;
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
