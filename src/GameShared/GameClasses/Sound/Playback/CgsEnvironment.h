#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Sound/Playback/CgsObject.h"     // Playback::Object (base, with Acquire/Release)
#include "GameShared/GameClasses/Sound/Playback/CgsHandle.h"     // Handle<T>
#include "GameShared/GameClasses/Sound/Playback/CgsRegistry.h"   // RegistrySpec / Registry
#include "GameShared/GameClasses/Sound/Playback/CgsVoice.h"      // the REAL Voice (+ Content via CgsContent.h)

// CgsSound::Playback::Environment - the sound-playback world: it owns the
// factory/voice/content handle tables, the registry and the DAC plug-in, and is
// carved (operator new/delete) out of a caller-supplied rw allocator. Class shape /
// member names / method set verbatim from the DecFIGS DWARF (CgsEnvironment.h);
// gated on the X360 ledger. This header TU bodies the one ledger function
// attributed to it -- the header-inline DoDispose @0x826BFDF8 -- and declares the
// rest of the DWARF surface (their own ledger functions).
//
// X360 layout note (DoDispose @0x826BFDF8): Object base {vptr, mu32RefCount} = 8
// bytes, mCpuMonitors 40 bytes -> mpAllocator at +48, exactly the word DoDispose
// snapshots before destroying itself.
//
// NOTE (Object base): Environment derives from the CgsObject.h `Object` (vptr +
// mu32RefCount, WITH Acquire()/Release()), NOT the CgsContent.h copy -- the wave-11
// Add*/GetR/GetV bodies open-code the handle-slot ref-count ops as Object::Acquire()
// / Object::Release() (CgsObject.h:45/49). Content is only referenced as a pointer
// (static AddContent / GetR's Handle<Content>), so it is forward-declared here to
// avoid pulling CgsContent.h's competing Object definition into this TU.

namespace rw { struct IResourceAllocator; }
namespace rw { namespace audio { namespace core { class PlugIn; } } }

// The RWAC engine entries StartDac/StopDac drive (declared BY NAME; resolved in the
// RWAC System / PlugIn TUs). RwacSystemLock + GetDefaultRwacSystem already live in
// RWAC/CgsGenericRwacFactory.h; the .cpp reaches them through that header and adds
// the Unlock + 3-arg PlugIn event forms below.
namespace rw { namespace audio { namespace core {
    class System;
    void RwacSystemUnlock(System* apSystem);                              // X360 System::Unlock
    void RwacPlugInEvent(PlugIn* apPlugIn, int aiEvent, int aiArg);       // 3-arg engine event
} } }

namespace CgsSound
{
namespace Playback
{
    class Factory;

    // (2026-08-25, audio-faithfulness wave 3): the former minimal decl-only
    // `class Voice { static FindNamedSlot }` slice is RETIRED -- the CgsContent.h
    // Object fold removed the clash that forced it, so the REAL Voice home is
    // included (CgsVoice.h also completes Content, which the handle tables and the
    // static AddContent use).
    //
    // GetR interned-name globals (X360 dword_83008650 / dword_830080A8): the words
    // GetR compares/passes are interned Name hashes -- dword_83008650 is the
    // TARGET FACTORY's name (the asm reads voice->mFactory.mName and compares) and
    // dword_830080A8 is the named-slot Name handed to Voice::FindNamedSlot.
    // Bodied in their owning data TUs.
    extern const u32 gu32VoiceTypeTag;        // X360 dword_83008650 (a factory Name hash)
    extern const u32 gu32NamedSlotSentinel;   // X360 dword_830080A8 (a slot Name hash)

    // DWARF CgsEnvironment.h:69 -- the per-stage CPU perf-monitor ids the
    // environment update registers. Construct (h:77) is its own ledger function.
    struct CpuMonitors
    {
        s32 miModule;               // h:83
        s32 miProcessCommands;      // h:84
        s32 miEnvironmentUpdate;    // h:85
        s32 miRwacFactoryUpdate;    // h:86
        s32 miAemsFactoryUpdate;    // h:87
        s32 miAemsFactoryUpdate2;   // h:88
        s32 miSplicerFactoryUpdate; // h:89
        s32 miContentUpdate;        // h:90
        s32 miVoiceUpdate;          // h:91
        s32 miVoiceUpdateOutput;    // h:92

        void Construct();
    };

    // DWARF CgsEnvironment.h:104 -- the creation spec Create/operator new consume.
    struct EnvironmentSpec
    {
        rw::IResourceAllocator* mpAllocator;      // h:108
        u32                     mu32FactoryCount; // h:109
        u32                     mu32VoiceCount;   // h:110
        u32                     mu32ContentCount; // h:111
        RegistrySpec            mRegistrySpec;    // h:112
    };

    struct Environment : public Object
    {
        // DWARF CgsEnvironment.h:334.
        enum eAudioMode
        {
            E_AUDIO_MODE_STEREO   = 0,
            E_AUDIO_MODE_SURROUND = 1,
        };

        static const u32 KU32_INVALID_INDEX = 0xFFFFFFFFu;   // DWARF h:365

        // ---- DWARF surface ----
        virtual ~Environment();
        static size_t GetAllocationSize(const EnvironmentSpec& lrSpec);

        // @ (the debug-size read Prepare's gxMessageFilterFlags print inlines:
        // 4*(fc+vc+cc + registry capacity + 35) + registry string/data sizes on
        // the console -- the 35 == the 28-word Environment + the 7-word Registry
        // header). Bodied in CgsEnvironment.cpp (phase B3) with host strides.
        size_t GetAllocatedSize();

        // @ 0x826ACF98 (bodied phase B3). Asserts sizeof + the spec allocator,
        // then carves ONE block holding the Environment + the three handle
        // tables + the in-place Registry (header/slots/data/strings), through
        // the spec allocator's DoAllocate ("Environment", align 4).
        void* operator new(size_t luSize, const EnvironmentSpec& lrSpec);
        void operator delete(void* lpMemory, rw::IResourceAllocator* lpAllocator);   // h:495
        void operator delete(void* lpMemory, const EnvironmentSpec& lrSpec);         // h:163
        void operator delete(void* lpMemory);                                        // h:167
        void* Allocate(u32 lu32Size, u32 lu32Alignment, const char* lpcName);
        void Free(void* lpMemory);
        static Handle<Environment> Create(const EnvironmentSpec& lrSpec);

        // @0x826AD130 / 1E0 / 290. Insert into the factory/voice/content handle tables.
        // STATIC 2-arg form (X360 __fastcall a1=Environment&, a2=object*): the asm
        // NULL-checks the object arg, so it is a pointer. This is the form the committed
        // CgsFactory.cpp caller uses (Environment::AddFactory(mEnvironment, this) /
        // AddContent(mEnvironment, arHandleOut.GetObject())) and CgsFactory.h declares.
        static bool AddFactory(Environment& arEnvironment, Factory* apFactory);   // @0x826AD130
        static u32  AddVoice  (Environment& arEnvironment, Voice*   apVoice);     // @0x826AD1E0
        static u32  AddContent(Environment& arEnvironment, Content* apContent);   // @0x826AD290

        bool RemoveFactory(Factory& lrFactory);

        // @0x82680EF8 / EA0. Accessors (assert the member is present). GetAllocator
        // is header-inline (2026-08-25 wave 3): the mounted Content::DoDispose
        // (CgsObject.cpp) links it while the rest of this TU stays gate-only.
        rw::IResourceAllocator* GetAllocator()    // @0x82680EF8
        {
            CGS_ASSERT(mpAllocator, "mpAllocator");
            return mpAllocator;
        }
        Registry*               GetRegistry();    // @0x82680EA0

        // Host accessor for the CpuMonitors sub-object (console env+0x08): the
        // Playback::Module::Prepare @0x826E90C0 stage-3 monitor registrations
        // store STRAIGHT into these words on the X360 (the class is a struct
        // there); the host keeps the member private and exposes it by name.
        CpuMonitors& GetCpuMonitors() { return mCpuMonitors; }

        // DWARF h:280. Look the owning factory handle up by interned name (the X360
        // symbol IDA truncates to `Environment::Ge`; callers: Module::CreateVoice
        // @0x826D7B00 / Module::CreateContent @0x826C12A8). FLAG (DEFER):
        // declared-only -- bodied with the Environment slices.
        Handle<Factory> GetFactory(Name aName);

        // @0x826BFAF0 / FE50. Look a voice up by ident / a content up by plugin key.
        // (2026-08-25 wave 6: GetV renamed to its DWARF name GetVoice, h:289 --
        // the param is the StreamBuffer::Ident u32. GetR keeps its truncated name
        // for now: the DWARF candidate is GetRwacVoiceByPlugin (h:362) which
        // returns Handle<Voice>, but the committed body builds a Handle<Content>
        // from the matched voice's tail -- reconcile with the Environment slice.)
        Handle<Voice>   GetVoice(u32 au32Id);     // @0x826BFAF0 (DWARF h:289)
        Handle<Content> GetR(u32 au32Plugin);     // @0x826BFE50 (see the note above)

        // @0x82680F50 / FE8. Start/stop the DAC plug-in through the RWAC engine.
        void StartDac();                          // @0x82680F50
        void StopDac();                           // @0x82680FE8

        // FLAG PC seam (phase D 2026-08-28): the console writer of mpDacPlugin is
        // not located (no exported body stores env+0x50 -- the DAC voice is
        // DATA-created, its spec arriving with the phase-F registry content, and
        // the store is inside that unexported create path). Until it lands, the
        // playback Module's phase-D bring-up stores the created Dac through this
        // setter; retire it when the real writer is decoded.
        void SetDacPlugin(rw::audio::core::PlugIn* apPlugIn) { mpDacPlugin = apPlugIn; }

        // @0x826D7500 (bodied phase B4). The per-frame engine pump the playback
        // Module::Update drives: UpdateContent, then (under the RWAC system
        // lock) UpdateVoices + UpdateFactories + the command-ring high-water
        // assert (muDeferredRingHighWater >= 157286, CgsEnvironment.cpp:192).
        void Update(f32 af32TimeStep);

        // @0x826BFDF8 (this TU, DWARF h:535) -- the ref-count-zero disposer:
        // snapshot the owning allocator, run the (non-deleting) destructor, then
        // release the carve through the allocator-keyed operator delete. The X360
        // dispatches the dtor virtually (vtbl slot 0, deleting-flag 0).
        virtual void DoDispose()
        {
            rw::IResourceAllocator* lpAllocator = mpAllocator;
            this->~Environment();
            operator delete(this, lpAllocator);
        }

    private:
        // @ 0x826BFBC8 (DWARF h:378, private; bodied phase B3 -- Create's only
        // path). Adopts the spec, wires the three handle tables + the Registry
        // into the co-located carve tail, zeroes the tables/monitors/counters.
        explicit Environment(const EnvironmentSpec& lrSpec);

        // The Update sub-passes (private; bodied phase B4):
        // @0x826C00B8 -- tick every live content (Content::Update), clear the
        //   CHANGED high bit, dispose REMOVED entries, count muActiveContent.
        void UpdateContent(f32 af32TimeStep);
        // @0x826C01B8 -- zero + rebuild mafVoiceTypeTickTotals (per
        //   GetProfileVoiceType, by GetCpuTicks), tick every live voice
        //   (Voice::Update), clear its CHANGED bit, release REMOVED voices,
        //   count muActiveVoices.
        void UpdateVoices(rw::audio::core::System* apSystem, f32 af32TimeStep);
        // @0x826A2200 -- per-factory virtual DoUpdate(dt).
        void UpdateFactories(f32 af32TimeStep);

        CpuMonitors             mCpuMonitors;             // h:404 (X360 +0x08)
        rw::IResourceAllocator* mpAllocator;              // h:405 (X360 +0x30 -- the DoDispose snapshot)
        u32                     mu32FactoryCount;         // h:406
        u32                     mu32VoiceCount;           // h:407
        u32                     mu32ContentCount;         // h:408
        Handle<Factory>*        mphFactory;               // h:409
        Handle<Voice>*          mphVoice;                 // h:410
        Handle<Content>*        mphContent;               // h:411
        Registry*               mpRegistry;               // h:412
        rw::audio::core::PlugIn* mpDacPlugin;             // h:413
        f32                     mafVoiceTypeTickTotals[5];// h:437
        u32                     muActiveVoices;           // h:439
        u32                     muActiveContent;          // h:440
    };
}
}
