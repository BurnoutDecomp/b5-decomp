#ifndef SPLICE_MANAGER_H
#define SPLICE_MANAGER_H

#include "types.hpp"

// ============================================================================
// GameShared/GameClasses/Sound/Playback/Splicer/SpliceManager.h
//
// SpliceManager -- the sound-playback splice subsystem's singleton. It owns:
//   * two fixed pools of pre-created RenderWare-audio voices (mono / stereo), each
//     handed out to playing splice samples and recycled through a free stack;
//   * eight per-source splice banks (m_Splices), each a SpliceContainer describing a
//     loaded splice-data blob; and
//   * a heap the splice objects (Splice, SpliceBankStatistics) route allocations
//     through (SpliceManager::Allocate on the global instance gpSpliceManager).
//
// Shape is DWARF ground truth (DecFIGS SpliceManager.h) gated on the X360 ledger; the
// member layout is reproduced BY NAME and in DWARF declaration ORDER -- the 32-bit
// guest offsets (shown in the +0xNN comments) do NOT and need not reproduce byte-for-
// byte on the 64-bit host, so pointers widen and only order is load-bearing. The X360
// ARTIST.XEX bodies that ground the offsets/behaviour:
//     SpliceManager::Allocate            @ 0x826AD630
//     SpliceManager::FindSplice          @ 0x826A3238
//     SpliceManager::DestroyVoice        @ 0x826A3488
//     SpliceManager::FreeMonoVoicePlugInPair   @ 0x826A35E8
//     SpliceManager::FreeStereoVoicePlugInPair @ 0x826A3640
//     VoicePool::AllocateVoicePluginPairToSpliceSample @ 0x8268AD60
//     VoicePool::FreeVoicePluginPair     @ 0x8268ADF8
//
// The last member is DWARF `const CgsSound::Playback::Environment& mEnvironment`
// (guest +0x6C4). It is modelled here -- as in the original Allocate reconstruction --
// as the polymorphic heap the manager forwards block allocations through: the
// Environment carries the block allocator at its +0x30, which Allocate and
// ~SpliceBankStatistics reach. Kept named `mpHeap` so the committed Allocate body
// compiles unchanged; the DWARF name is recorded in the comment.
// ============================================================================

// Forward-declared engine collaborators (vendor homes rw/audio/core/Voice.h, PlugIn.h).
namespace rw { namespace audio { namespace core { class Voice; class PlugIn; } } }

// Forward-declared splice-runtime collaborator (home internal/SpliceObjects.cpp). The
// pool allocator takes it by pointer only and its body ignores it, so a fwd decl suffices.
struct SpliceSample;

// SPLICE_TYPE (DWARF, global scope) -- selects one of the eight per-source splice banks.
// Homed here (the low-level splice header) so both SpliceManager and SplicerContent see
// it through the include chain; SplicerContent's meType resolves to this declaration.
enum SPLICE_TYPE
{
    E_SPLICE_TYPE_INVALID = 0
};

// SPLICE_Data (DWARF) -- one loaded splice record. The 24-byte stride is grounded in
// FindSplice (returns mpHeadSplice + 24*index). The two fields the splice-object bodies
// read (SpliceObjects.cpp: Splice::Play/Update/IsPlaying/GetCpuTicks) are recovered by
// name from those bodies' X360 asm; the remainder of the 24-byte record is external
// loaded-blob data preserved as opaque padding (external-serialised-data exception).
struct SPLICE_Data
{
    u8  mau8Header[7];     // +0x00..0x06  (unrecovered blob header)
    u8  mucNumSampleRefs;  // +0x07  splice sample-slot count (Play/IsPlaying/GetCpuTicks loop bound)
    f32 mfVolumeScale;     // +0x08  master volume scale applied to Play/Update input volume
    u8  mau8Tail[12];      // +0x0C..0x17  (unrecovered) -> total 24-byte stride
};
static_assert( sizeof( SPLICE_Data ) == 24, "SPLICE_Data must keep its 24-byte FindSplice stride" );

// (The former SpliceManagerDetail::Heap/AllocationRequest console-ABI view is
// RETIRED -- AEMS-cascade slice 3: the +0x6C4 member IS the DWARF
// `const Environment&` (the ctor @0x826C30C8 saves its r4), Allocate's +0x30 hop
// is Environment::GetAllocator, and its stack descriptor is the standard rw
// five-pair {(size,4),(0,1)x4} the whole playback engine uses. The view's
// result-first call shape was also the CONSOLE ABI -- it could never dispatch a
// host allocator vtable correctly, the divergence the cascade decode flagged.)
namespace CgsSound { namespace Playback { struct Environment; } }

struct SpliceManager
{
    // The SpliceManager's assertion sink (DWARF `void (*)(char*)`). When set on the
    // global instance it is called with the failing check's message; when null the
    // check is silent. Read via gpSpliceManager->mAssertCallbackFunc.
    typedef void (*AssertCallbackFunc)( const char* lpcMessage );

    // SpliceManager.h:15 (DWARF).
    static const u32 KU_MAX_POOLED_VOICES = 64;

    // SpliceManager.h:20 (DWARF). Per-source splice bank: a loaded splice-data blob
    // (mpSampleData), its table of contents, the splice/sample counts, and the head of
    // the SPLICE_Data record array FindSplice indexes.
    struct SpliceContainer
    {
        char*        mpSampleData;       // :37 (+0x00)
        s32*         mpTableOfContents;  // :38 (+0x04)
        s32          mNumSplices;        // :39 (+0x08)
        s32          mNumSamples;        // :40 (+0x0C)
        SPLICE_Data* mpHeadSplice;       // :41 (+0x10) head of the record array

        // Own ledger funcs (declared only; their own TUs).
        SpliceContainer();                                          // :22
        void         Init( SPLICE_Data* apHeadData, int aiNumSplices ); // :31
        void         Clear();                                       // :32
        int          Size();                                        // :33
        SPLICE_Data* GetSplice( int aiIndex );                      // :34
    };

    // SpliceManager.h:86 (DWARF). A pooled (voice, plug-in-chain) pair.
    struct VoicePluginPair
    {
        rw::audio::core::Voice*   mpVoice;    // :88 (+0x00)
        rw::audio::core::PlugIn** mppPlugIn;  // :89 (+0x04)
    };

    // SpliceManager.h:179 (DWARF). A fixed pool of pre-created voices plus a free stack
    // of the pairs available to hand out. The pairs live in maVoicePluginPairs; the
    // stack (mapVoicePluginPairsStack) holds pointers into that array, with
    // miPooledVoiceStackFreeIndex the top-of-stack cursor.
    struct VoicePool
    {
        VoicePluginPair  maVoicePluginPairs[KU_MAX_POOLED_VOICES];        // :216 (+0x000)
        VoicePluginPair* mapVoicePluginPairsStack[KU_MAX_POOLED_VOICES];  // :217 (+0x200)
        u32              muPooledVoiceCount;                              // :219 (+0x300)
        s32              miPooledVoiceStackFreeIndex;                     // :220 (+0x304)

        // Own ledger funcs (declared only; their own TUs).
        void Construct();                                               // :183
        bool Prepare( VoicePluginPair* apaVoicePairs, u32 auVoiceCount );// :188
        void Update();                                                  // :191
        bool Release();                                                 // :194
        void Destruct();                                                // :197

        // @ 0x8268AD60. Pop the next free pooled pair off the stack (null when the free
        // index is negative). The SpliceSample argument mirrors the DWARF signature but
        // the body does not read it.
        VoicePluginPair* AllocateVoicePluginPairToSpliceSample( SpliceSample* apSpliceSample ); // :201

        // @ 0x8268ADF8. Push a pair back onto the free stack (asserting on overflow).
        void FreeVoicePluginPair( VoicePluginPair* apVoicePluginPair ); // :205

        bool IsVoiceInPool( rw::audio::core::Voice* apVoice );          // :210 (declared only)
    };

    // SpliceManager.h:76 (DWARF). Plug-in-chain slot indices.
    enum PlugInIndex
    {
        SNDPLAYER_INDEX = 0,
        RESAMPLE_INDEX  = 1,
        PANNER_INDEX    = 2,
        SEND_INDEX      = 3,
        MAX_INDEX       = 4
    };

    // @ 0x826A3238. Clamp `aiIndex` into the `aeType` bank (asserting when the bank is
    // empty) and return the addressed SPLICE_Data record.
    SPLICE_Data* FindSplice( SPLICE_TYPE aeType, int aiIndex );

    // @ 0x826A3488. Assert the pair/voice are non-null and are NOT one of the pooled
    // voices, then release the voice through the RWAC engine and null the pair.
    void DestroyVoice( VoicePluginPair* apVoicePluginPair );

    // @ 0x826A35E8 / 0x826A3640. Fire the plug-in's stop event and return the pair to
    // the mono / stereo pool's free stack.
    void FreeMonoVoicePlugInPair( VoicePluginPair* apVoicePluginPair );
    void FreeStereoVoicePlugInPair( VoicePluginPair* apVoicePluginPair );

    // @ 0x826AD630. Allocate a `luSize`-byte splice block tagged `lpcTag` through the
    // owned heap. Builds the X360 allocation descriptor (alignment 4, four (size,1) pool
    // hints) and forwards to the heap's polymorphic Allocate slot (0 on failure).
    void* Allocate( u32 luSize, const char* lpcTag );

    // Free a block previously handed out by Allocate, through the same heap (declared
    // only; own ledger func -- de-inlined call site in ~SpliceBankStatistics).
    void  Free( void* lpMemory );

    // Fill a stereo (voice, plug-in-chain) pair for a splice sample that could not be
    // served from the stereo pool -- the dynamic-voice fallback path. @ own ledger func;
    // called by StereoSpliceSample::StartVoice (r3=this, r4=&sample->mDynamicVoicePluginPair).
    void  CreateStereoVoice( VoicePluginPair* apOutPair );

    // The SpliceManager's assertion sink getter (DWARF SpliceManager::GetAssertCallbackFunction).
    // Inlined at every X360 call site (a direct load of the global instance's +0x610 field), so
    // it is a header inline here; the splice objects read the sink through it. Grounded in
    // Splice::operator new @0x826C33A0 and StereoSpliceSample::StartVoice @0x826A3F98.
    AssertCallbackFunc GetAssertCallbackFunction() { return mAssertCallbackFunc; }

    // De-inlined stereo-pool voice acquisition: the X360 inlines
    // `mStereoVoicePool.AllocateVoicePluginPairToSpliceSample(sample)` (a direct call on
    // this+0x308) into StereoSpliceSample::StartVoice. Exposed here so the pool stays
    // private while the call site reproduces the exact behaviour.
    VoicePluginPair* AllocateStereoVoicePluginPair( SpliceSample* apSpliceSample )
    {
        return mStereoVoicePool.AllocateVoicePluginPairToSpliceSample( apSpliceSample );
    }

private:
    VoicePool          mMonoVoicePool;                       // :224 (+0x000)
    VoicePool          mStereoVoicePool;                     // :225 (+0x308)
    AssertCallbackFunc mAssertCallbackFunc;                  // :227 (+0x610)
    SpliceContainer    m_Splices[8];                         // :231 (+0x614)

    // :233..236 -- the four plug-in registry handles (DWARF PlugInRegistry::PlugInHandle,
    // which the RWAC PlugInRegistry::GetPlugInHandle returns as an opaque pointer).
    void* sndplayerHandle;                                   // :233 (+0x6B4)
    void* resampleHandle;                                    // :234 (+0x6B8)
    void* pannerHandle;                                      // :235 (+0x6BC)
    void* sendHandle;                                        // :236 (+0x6C0)

    // :240 -- DWARF `const CgsSound::Playback::Environment& mEnvironment` (guest
    // +0x6C4; the ctor @0x826C30C8 stores its incoming r4). Pointer-modelled so
    // the struct stays default-constructible-shaped; Allocate reaches the block
    // allocator through it (the console's +0x30 hop == Environment::GetAllocator).
    const CgsSound::Playback::Environment* mpEnvironment;    // :240 (+0x6C4)

public:
    // @ 0x826C30C8 (AEMS-cascade slice 3; full decode progress/scratch_dossiers/
    // aems_factory_cascade_codex.md). Constructed in place at the tail of the
    // SplicerFactory carve. Body in SpliceManager.cpp.
    SpliceManager( const CgsSound::Playback::Environment& arEnvironment,
                   u32 auMonoVoiceCount, u32 auStereoVoiceCount );

    // The assert-sink installer (the SplicerFactory ctor's manager+0x610 store).
    void SetAssertCallbackFunction( AssertCallbackFunc apfnAssert )
    {
        mAssertCallbackFunc = apfnAssert;
    }
};

// The global SpliceManager instance (X360 off_82FFB9F0 / DWARF spSpliceManager). The
// splice objects route allocations and read the assert sink through it. Declared here
// (its home); the ctor installs `this` into it. Also re-declared in
// CgsSpliceBankStatistics.h for the bank code that predates this header.
extern SpliceManager* gpSpliceManager;

#endif // SPLICE_MANAGER_H
