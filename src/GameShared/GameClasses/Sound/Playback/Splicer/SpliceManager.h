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
// FindSplice (returns mpHeadSplice + 24*index); the individual fields are not recovered
// by this TU, so the record is pinned as an opaque 24-byte span (external loaded-blob
// data walked by stride, documented per the external-serialised-data exception).
struct SPLICE_Data
{
    u8 mau8Record[24];
};

namespace SpliceManagerDetail
{
// The polymorphic heap the SpliceManager forwards block allocations to. The X360
// builds a small allocation descriptor on the stack and calls through the heap
// object's vtable at +0x10 (the 5th slot). The descriptor's exact per-pool
// semantics are not recoverable from this TU, so it is modelled as a named
// AllocationRequest that reproduces the X360 stores field-for-field, and the
// heap exposes the one virtual entry point Allocate exercises.
struct AllocationRequest
{
    // var_30 / v7: the X360 packs { LODWORD = 4 (alignment), HIDWORD = size }.
    s32 miAlignment;   // LODWORD(v5) = 4
    s32 miSize;        // HIDWORD(v5) = requested size

    // var_28..var_C / v8..v15: four (size, 1) per-pool hint pairs the X360 fills
    // before the call (v8=v10=v12=v14 = size; v9=v11=v13=v15 = 1).
    s32 maiPoolSize[4];
    s32 maiPoolFlag[4];
};

struct Heap
{
    struct VTable
    {
        // Padding so Allocate lands at vtable byte offset 0x10 (index 4) -- the
        // slot the X360 loads via `lwz r11,0x10(r11); mtctr; bctrl`.
        void* mapReserved[4];
        // The polymorphic block allocator: (out-result, this, &request, tag).
        // Returns the allocated block (or null on failure) into the out result.
        void* (*mpfnAllocate)( void* lpResult, Heap* lpHeap,
                               const AllocationRequest* lprRequest,
                               const char* lpcTag );
    };

    const VTable* mpVTable;   // +0x00
};
}

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

    // :240 -- DWARF `const CgsSound::Playback::Environment& mEnvironment` (guest +0x6C4).
    // Modelled as the polymorphic heap the manager forwards allocations through (the
    // Environment's +0x30 block allocator); kept as mpHeap for the committed Allocate body.
    SpliceManagerDetail::Heap* mpHeap;                       // :240 (+0x6C4)
};

// The global SpliceManager instance (X360 off_82FFB9F0 / DWARF spSpliceManager). The
// splice objects route allocations and read the assert sink through it. Declared here
// (its home); the ctor installs `this` into it. Also re-declared in
// CgsSpliceBankStatistics.h for the bank code that predates this header.
extern SpliceManager* gpSpliceManager;

#endif // SPLICE_MANAGER_H
