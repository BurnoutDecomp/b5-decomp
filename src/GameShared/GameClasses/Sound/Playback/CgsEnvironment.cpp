// ============================================================================
// CgsEnvironment.cpp -- CgsSound::Playback::Environment runtime bodies.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (wave 11):
//   Environment::AddFactory   @ 0x826AD130   Environment::AddVoice   @ 0x826AD1E0
//   Environment::AddContent   @ 0x826AD290   Environment::GetAllocator @ 0x82680EF8
//   Environment::GetRegistry  @ 0x82680EA0   Environment::GetFactory @ DecFIGS 0x441058
//   Environment::GetVoice     @ 0x826BFAF0  (was GetV)
//   Environment::GetR         @ 0x826BFE50   Environment::StartDac   @ 0x82680F50
//   Environment::StopDac      @ 0x82680FE8   Environment::~Environment @ 0x826E9020
//   Environment::operator new @ 0x826ACF98   Environment::operator delete @ 0x826BFD60
//
// Add*/GetR/GetV read the handle tables (mphFactory/mphVoice/mphContent, each a
// Handle<T>* -- Handle<T> == { T* }, layout-identical to an Object* array) and
// open-code the X360's Handle<T>::operator= as Object::Acquire()/Release() pairs on
// the shared Playback::Object base, preserving the exact acquire/release ordering.
//
// This TU does NOT include CgsFactory.h: that header carries a competing minimal
// `class Environment { static AddFactory/AddContent }` stub (the committed
// CgsFactory.cpp caller's decl); pulling it here would clash with the real
// Environment. Factory/Voice/Content are reached only as fwd decls + the Object base.
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/CgsEnvironment.h"
#include "GameShared/GameClasses/Sound/Playback/CgsObject.h"
#include "GameShared/GameClasses/Sound/Playback/CgsFactory.h"     // complete Factory (GetR's name compare)
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacFactory.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacPlayerVoice.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"  // the Update sub-pass monitors (phase B4)
#include "rw/audio/core/PlugIn.h"                                  // rw::audio::core::System (muDeferredRingHighWater)
#include "rw/rwcore_structs.h"                                     // rw::Resource / ResourceDescriptor / IResourceAllocator

#include <new>                                                     // placement new (the co-located carve)

#include <cstring>

namespace CgsSound
{
namespace Playback
{

Environment::eAudioMode Environment::GetAudioMode() const
{
    CGS_ASSERT(mpDacPlugin != 0, "mpDacPlugin");
    // FLAG PC-platform leaf: AudioOutputPC opens the engine DAC as a two-channel
    // XAudio2 source voice, the native equivalent of XAudioGetSpeakerConfig.
    return E_AUDIO_MODE_STEREO;
}

// Header-inline in the X360 callers: allocate one resource block through the
// Environment's owning rw allocator and return its primary base resource.
void* Environment::Allocate(u32 lu32Size, u32 lu32Alignment, const char* lpcName)
{
    rw::ResourceDescriptor lDescriptor;
    for (u32 lu = 0; lu < 4u; ++lu)
    {
        lDescriptor.m_baseResourceDescriptors[lu].m_size = 0;
        lDescriptor.m_baseResourceDescriptors[lu].m_alignment = 1;
    }
    lDescriptor.m_baseResourceDescriptors[0].m_size = lu32Size;
    lDescriptor.m_baseResourceDescriptors[0].m_alignment = lu32Alignment ? lu32Alignment : 1u;
    rw::Resource lResource = GetAllocator()->DoAllocate(lDescriptor, lpcName);
    return lResource.m_baseResources[0];
}

// Header-inline in the X360 callers: release a raw allocator block through the
// same Environment allocator that supplied it.
void Environment::Free(void* lpMemory)
{
    GetAllocator()->Free(lpMemory, 0);
}

// @ 0x826AD130. Find the first free factory slot and store apFactory into it,
// acquiring a reference (and releasing any prior occupant). Returns false if the
// table is empty or full.
bool Environment::AddFactory(Environment& arEnvironment, Factory* apFactory)
{
    Environment* lpThis = &arEnvironment;
    u32 lu32Index = 0;

    u32 lu32Count = lpThis->mu32FactoryCount;
    if (lu32Count == 0)
    {
        return false;
    }

    Object** lppSlot = reinterpret_cast<Object**>(lpThis->mphFactory);
    while (*lppSlot != 0)
    {
        ++lu32Index;
        ++lppSlot;
        if (lu32Index >= lu32Count)
        {
            return false;
        }
    }

    Object* lpFactory = reinterpret_cast<Object*>(apFactory);
    if (lpFactory) { lpFactory->Acquire(); }
    Object** lppSlots = reinterpret_cast<Object**>(lpThis->mphFactory);
    if (lpFactory) { lpFactory->Acquire(); }
    if (lppSlots[lu32Index]) { lppSlots[lu32Index]->Release(); }
    lppSlots[lu32Index] = lpFactory;
    if (lpFactory) { lpFactory->Release(); }
    return true;
}

// @ 0x826AD1E0. Find the first free voice slot and store apVoice into it. Returns
// the slot index, or (u32)-1 if the table is empty or full.
u32 Environment::AddVoice(Environment& arEnvironment, Voice* apVoice)
{
    Environment* lpThis = &arEnvironment;
    u32 lu32Index = 0;

    u32 lu32Count = lpThis->mu32VoiceCount;
    if (lu32Count == 0)
    {
        return static_cast<u32>(-1);
    }

    Object** lppSlot = reinterpret_cast<Object**>(lpThis->mphVoice);
    while (*lppSlot != 0)
    {
        ++lu32Index;
        ++lppSlot;
        if (lu32Index >= lu32Count)
        {
            return static_cast<u32>(-1);
        }
    }

    Object* lpVoice = reinterpret_cast<Object*>(apVoice);
    if (lpVoice) { lpVoice->Acquire(); }
    Object** lppSlots = reinterpret_cast<Object**>(lpThis->mphVoice);
    if (lpVoice) { lpVoice->Acquire(); }
    if (lppSlots[lu32Index]) { lppSlots[lu32Index]->Release(); }
    lppSlots[lu32Index] = lpVoice;
    if (lpVoice) { lpVoice->Release(); }
    return lu32Index;
}

// @ 0x826AD290. Find the first free content slot and store apContent into it.
// Returns the slot index, or (u32)-1 if the table is empty or full.
u32 Environment::AddContent(Environment& arEnvironment, Content* apContent)
{
    Environment* lpThis = &arEnvironment;
    u32 lu32Index = 0;

    u32 lu32Count = lpThis->mu32ContentCount;
    if (lu32Count == 0)
    {
        return static_cast<u32>(-1);
    }

    Object** lppSlot = reinterpret_cast<Object**>(lpThis->mphContent);
    while (*lppSlot != 0)
    {
        ++lu32Index;
        ++lppSlot;
        if (lu32Index >= lu32Count)
        {
            return static_cast<u32>(-1);
        }
    }

    Object* lpContent = reinterpret_cast<Object*>(apContent);
    if (lpContent) { lpContent->Acquire(); }
    Object** lppSlots = reinterpret_cast<Object**>(lpThis->mphContent);
    if (lpContent) { lpContent->Acquire(); }
    if (lppSlots[lu32Index]) { lppSlots[lu32Index]->Release(); }
    lppSlots[lu32Index] = lpContent;
    if (lpContent) { lpContent->Release(); }
    return lu32Index;
}

// (GetAllocator @0x82680EF8 moved header-inline 2026-08-25 wave 3 -- the mounted
//  Content::DoDispose links it.)

// @ 0x82680EA0. Return the owned entity registry (asserted present).
Registry* Environment::GetRegistry()
{
    CGS_ASSERT(mpRegistry, "mpRegistry");
    return mpRegistry;
}

// DecFIGS @ 0x441058 (the standalone X360 body was folded/not exported). Walk
// the registered factory handles in order and return an acquired handle to the
// first factory whose interned name matches. The ARTIST CreateVoice/CreateContent
// callers attest this exact contract at their Environment::Ge call sites.
Handle<Factory> Environment::GetFactory(Name aName)
{
    Handle<Factory> lhResult(static_cast<Factory*>(0));
    for (u32 lu32I = 0; lu32I < mu32FactoryCount; ++lu32I)
    {
        Factory* lpFactory = mphFactory[lu32I].GetObject();
        if (lpFactory && lpFactory->GetName() == aName)
        {
            lhResult.SetObject(lpFactory);
            lpFactory->Acquire();
            return lhResult;
        }
    }

    lhResult.SetObject(0);
    return lhResult;
}

// @ 0x826BFAF0. Look a voice up by ident. The X360 `*(voice + 0x0C)` is
// Voice::mIdent, read by name via the wave-1 GetIdent accessor; the handle table
// is walked through the real Handle<Voice> slots (one owned pointer each), not a
// reinterpreted Object** (2026-08-25 wave 3). Returns an acquired handle to the
// match, or an empty handle.
Handle<Voice> Environment::GetVoice(u32 au32Id)
{
    Handle<Voice> lhResult(static_cast<Voice*>(0));
    if (mu32VoiceCount)
    {
        for (u32 lu32I = 0; lu32I < mu32VoiceCount; ++lu32I)
        {
            Voice* lpVoice = mphVoice[lu32I].GetObject();
            if (lpVoice)
            {
                CGS_ASSERT(lpVoice, "mpObject");
                if (au32Id == lpVoice->GetIdent())
                {
                    lhResult.SetObject(lpVoice);
                    lpVoice->Acquire();
                    return lhResult;
                }
            }
        }
    }
    lhResult.SetObject(0);
    return lhResult;
}

// @ 0x826BFE50. Walk the voice table for the tagged factory's player voice whose
// named slot resolves and whose GenericRwacVoice plug-in table contains apPlugin.
// Decoded by name:
//   * the "type tag" check is `voice->mFactory.mName == dword_83008650` (the asm
//     loads voice+8 -> factory, factory+8 -> mName) -- i.e. the voice belongs to a
//     SPECIFIC factory, matched by interned name;
//   * FindNamedSlot receives the interned Name at dword_830080A8 (one word built
//     on the stack and passed by the non-trivial-class reference ABI) -- the real
//     Voice::FindNamedSlot(Name) member.
// Returns an acquired Voice handle to the first match, or an empty handle. The
// console reaches the GenericRwacVoice secondary base at Voice+0x2C; the host
// expresses that same adjustment through the concrete player wrapper so pointer
// width and multiple-inheritance layout remain native.
Handle<Voice> Environment::GetRwacVoiceByPlugin(
    const rw::audio::core::PlugIn* apPlugin)
{
    Handle<Voice> lhResult(static_cast<Voice*>(0));
    if (mu32VoiceCount == 0)
    {
        lhResult.SetObject(0);
        return lhResult;
    }

    for (u32 lu32I = 0; lu32I < mu32VoiceCount; ++lu32I)
    {
        Voice* lpVoice = mphVoice[lu32I].GetObject();
        if (!lpVoice) { continue; }

        // Factory-name match (asm: voice+8 -> factory, +8 -> mName vs dword_83008650).
        if (lpVoice->GetFactory().GetName() !=
            Name(static_cast<uintptr_t>(gu32VoiceTypeTag))) { continue; }

        CGS_ASSERT(lpVoice, "mpObject");
        if (lpVoice->FindNamedSlot(Name(static_cast<uintptr_t>(gu32NamedSlotSentinel))) == 0)
        {
            continue;
        }

        CGS_ASSERT(lpVoice, "mpObject");
        lpVoice->Acquire();

        GenericRwacPlayerVoice* lpPlayerVoice =
            static_cast<GenericRwacPlayerVoice*>(lpVoice);
        GenericRwacVoice& lrRwacVoice = *lpPlayerVoice;
        const u32 lu32PluginCount = lrRwacVoice.GetPluginCount();
        bool lbMatched = false;
        for (u32 lu32P = 0; lu32P < lu32PluginCount; ++lu32P)
        {
            if (lrRwacVoice.GetPlugin(lu32P) == apPlugin)
            {
                lbMatched = true;
                break;
            }
        }

        if (lbMatched)
        {
            lhResult.SetObject(lpVoice);
            lpVoice->Acquire();
            lpVoice->Release();
            return lhResult;
        }
        lpVoice->Release();
    }

    lhResult.SetObject(0);
    return lhResult;
}

// @ 0x82680F50. Start the DAC plug-in: lock the audio-core System, fire the start
// event on the DAC plug-in, then unlock.
void Environment::StartDac()
{
    CGS_ASSERT(mpDacPlugin, "mpDacPlugin");
    rw::audio::core::System* lpSystem = GetDefaultRwacSystem();
    CGS_ASSERT(lpSystem, "rw::audio::core::System::GetInstance()");
    rw::audio::core::RwacSystemLock(lpSystem);
    rw::audio::core::RwacPlugInEvent(mpDacPlugin, 3, 0);
    rw::audio::core::RwacSystemUnlock(GetDefaultRwacSystem());
}

// @ 0x82680FE8. Stop the DAC plug-in: lock, fire the stop event, unlock.
void Environment::StopDac()
{
    CGS_ASSERT(mpDacPlugin, "mpDacPlugin");
    rw::audio::core::System* lpSystem = GetDefaultRwacSystem();
    CGS_ASSERT(lpSystem, "rw::audio::core::System::GetInstance()");
    rw::audio::core::RwacSystemLock(lpSystem);
    rw::audio::core::RwacPlugInEvent(mpDacPlugin, 4, 0);
    rw::audio::core::RwacSystemUnlock(GetDefaultRwacSystem());
}

// @ 0x826E9020. Out-of-line destructor PLACEHOLDER. The X360 scalar-deleting thunk
// runs ~Environment() then frees on (flag & 1); MSVC re-synthesises that thunk from
// this out-of-line dtor. FLAG: real member teardown is external -- fill in when the
// full ~Environment lands so teardown is not silently lost.
Environment::~Environment()
{
}

// @ 0x826BFD60. Allocator-keyed placement delete: hand the carve back to the rw
// allocator. The X360 builds the 20-byte {memory, 0,0,0,0} block -- the CONSOLE
// rw::Resource (BaseResources<5>, first base = the carve) -- and dispatches the
// allocator's console vtable slot 5 == DoFree(const Resource&) (slot 4 right
// beside it is DoAllocate, proven by operator new below passing a
// BaseResourceDescriptor and receiving a Resource; the retail console vtable has
// no AllocDebug pair -- the x64 rwcore.pdb puts DoAllocate/DoFree at 6/7 behind
// them). By name on the host 4-slot Resource. (2026-08-25 wave 3: the raw
// vtable-index dispatch is retired.)
void Environment::operator delete(void* lpMemory, rw::IResourceAllocator* lpAllocator)
{
    CGS_ASSERT(lpAllocator, "lpAllocator");

    rw::Resource lResource;
    lResource.m_baseResources[0] = lpMemory;
    lResource.m_baseResources[1] = 0;
    lResource.m_baseResources[2] = 0;
    lResource.m_baseResources[3] = 0;
    lpAllocator->DoFree(lResource);
}

// @ 0x826ACF98. Placement new from an EnvironmentSpec: size the carve (the object +
// the factory/voice/content handle tables + the registry entity table, data blob and
// string table) and allocate it through the spec allocator.
//
// The X360 packs ONE {size, align=4} pair (a BaseResourceDescriptor) and calls the
// allocator's console vtable slot 4 == DoAllocate(descriptor, "Environment"),
// which returns a Resource by hidden-buffer; the carve base is its first word. The
// former reconstruction read the RegistrySpec fields POSITIONALLY (spec[4]/[5]/[6])
// -- wrong on the host where muDataSize/muStringTableSize are size_t -- and
// dispatched the raw vtable index; both retired 2026-08-25 (wave 3), by name below.
//
// Console size formula (@0x826ACF98): 4*(handleCount + entityCount + 35) +
// stringTableSize + dataSize -- the `35` covers the console 112-byte Environment
// (28 words) plus the registry header (7 words, CgsRegistry.h). HOST NOTE: the
// formula's word-scale is the console's 4-byte pointer width; the host carve is
// sized with the host sizeofs where the console used its own (Environment 112 ->
// sizeof(Environment); handle/entity slots 4 -> sizeof(void*)), keeping the carve
// big enough for the widened tables the constructor lays out.
void* Environment::operator new(size_t luSize, const EnvironmentSpec& lrSpec)
{
    CGS_ASSERT(luSize == sizeof(Environment), "sizeof(Environment) == luSize");
    CGS_ASSERT(lrSpec.mpAllocator, "lEnvironmentSpec.mpAllocator");

    rw::IResourceAllocator* lpAllocator = lrSpec.mpAllocator;

    const size_t luHandles = static_cast<size_t>(lrSpec.mu32FactoryCount)
                           + lrSpec.mu32VoiceCount
                           + lrSpec.mu32ContentCount;

    // Console: 4 * (handles + entityCount + 35) + stringTable + data. Host: the
    // pointer-width slots widen (sizeof(void*)), the fixed 35-word head becomes the
    // host Environment + registry-header sizes.
    const size_t luTotal = sizeof(void*) * (luHandles + lrSpec.mRegistrySpec.mu32EntityCount)
                         + luSize
                         + sizeof(Registry)                      // the registry header (host; the
                                                                 //  console's 7 words -- its trailing
                                                                 //  first slot slightly over-covers)
                         + lrSpec.mRegistrySpec.muStringTableSize
                         + lrSpec.mRegistrySpec.muDataSize;

    rw::ResourceDescriptor lDescriptor;
    lDescriptor.m_baseResourceDescriptors[0].m_size      = static_cast<u32>(luTotal);
    lDescriptor.m_baseResourceDescriptors[0].m_alignment = 4u;   // console align word
    for (u32 lu = 1; lu < 4u; ++lu)
    {
        lDescriptor.m_baseResourceDescriptors[lu].m_size      = 0u;
        lDescriptor.m_baseResourceDescriptors[lu].m_alignment = 1u;
    }

    rw::Resource lResource = lpAllocator->DoAllocate(lDescriptor, "Environment");
    return lResource.m_baseResources[0];
}

// =============================================================================
// Environment::Environment(const EnvironmentSpec&)  @ 0x826BFBC8  (private;
// bodied 2026-08-25, faithful-audio-engine phase B3)
//
// Lays the co-located carve out (console word map in brackets): adopt the
// allocator [w12] + the three counts [w13..15]; point the three handle tables at
// the carve tail (mphFactory = this+1 [w16 = this+28 words], voice after factory
// [w17], content after voice [w18]) and null every slot; placement-construct the
// Registry just past the content table [w19] over the remaining blob; zero the
// CpuMonitors [w2..11], mpDacPlugin [w20], muActiveVoices/Content [w26/27]. The
// mafVoiceTypeTickTotals [w21..25] are UNTOUCHED by the console ctor and stay so.
// The Object base ctor supplies the vptr [w0] + zero refcount [w1].
// =============================================================================
Environment::Environment(const EnvironmentSpec& lrSpec)
    : Object()
{
    mpAllocator       = lrSpec.mpAllocator;
    mu32FactoryCount  = lrSpec.mu32FactoryCount;
    mu32VoiceCount    = lrSpec.mu32VoiceCount;
    mu32ContentCount  = lrSpec.mu32ContentCount;

    u8* lpTail = reinterpret_cast<u8*>(this + 1);

    mphFactory = reinterpret_cast<Handle<Factory>*>(lpTail);
    lpTail += sizeof(Handle<Factory>) * mu32FactoryCount;
    mphVoice = reinterpret_cast<Handle<Voice>*>(lpTail);
    lpTail += sizeof(Handle<Voice>) * mu32VoiceCount;
    mphContent = reinterpret_cast<Handle<Content>*>(lpTail);
    lpTail += sizeof(Handle<Content>) * mu32ContentCount;

    for (u32 lu = 0; lu < mu32FactoryCount; ++lu)
        new (&mphFactory[lu]) Handle<Factory>();
    for (u32 lu = 0; lu < mu32VoiceCount; ++lu)
        new (&mphVoice[lu]) Handle<Voice>();
    for (u32 lu = 0; lu < mu32ContentCount; ++lu)
        new (&mphContent[lu]) Handle<Content>();

    // The in-place Registry over the rest of the blob (its own ctor carves the
    // slot array / data / string-table pointers with host-safe arithmetic).
    mpRegistry = new (lpTail) Registry(lrSpec.mRegistrySpec);

    mCpuMonitors.miModule              = 0;
    mCpuMonitors.miProcessCommands     = 0;
    mCpuMonitors.miEnvironmentUpdate   = 0;
    mCpuMonitors.miRwacFactoryUpdate   = 0;
    mCpuMonitors.miAemsFactoryUpdate   = 0;
    mCpuMonitors.miAemsFactoryUpdate2  = 0;
    mCpuMonitors.miSplicerFactoryUpdate = 0;
    mCpuMonitors.miContentUpdate       = 0;
    mCpuMonitors.miVoiceUpdate         = 0;
    mCpuMonitors.miVoiceUpdateOutput   = 0;

    mpDacPlugin     = 0;
    muActiveVoices  = 0;
    muActiveContent = 0;
}

// =============================================================================
// Environment::Create(const EnvironmentSpec&)  @ 0x826C7948  (bodied phase B3;
// the IDA-truncated "CgsSound::Playback::Envi")
//   env = new (spec) Environment(spec);   // the co-located carve above
//   assert(env, "lpNewEnvironment", CgsEnvironment.h:528)
//   handle = env (+ Acquire)
// =============================================================================
Handle<Environment> Environment::Create(const EnvironmentSpec& lrSpec)
{
    void* lpMemory = operator new(sizeof(Environment), lrSpec);
    Environment* lpEnvironment = 0;
    if (lpMemory)
        lpEnvironment = ::new (lpMemory) Environment(lrSpec);   // global placement form (the class-scope carve operator would otherwise hide it)

    CGS_ASSERT(lpEnvironment != 0, "lpNewEnvironment");

    Handle<Environment> lhResult;
    lhResult.SetObject(lpEnvironment);
    if (lpEnvironment)
        lpEnvironment->Acquire();
    return lhResult;
}

// =============================================================================
// Environment::GetAllocatedSize  (the debug-size read Module::Prepare's
// gxMessageFilterFlags print inlines @0x826E90C0: console 4*(fc+vc+cc +
// registry capacity + 35) + registry string/data sizes; host strides mirror
// operator new's carve math)
// =============================================================================
size_t Environment::GetAllocatedSize()
{
    CGS_ASSERT(mpRegistry, "mpRegistry");
    return sizeof(Environment)
         + sizeof(void*) * (static_cast<size_t>(mu32FactoryCount)
                            + mu32VoiceCount + mu32ContentCount
                            + mpRegistry->GetEntityCapacity())
         + sizeof(Registry)
         + mpRegistry->GetStringTableSize()
         + mpRegistry->GetDataSize();
}

// =============================================================================
// Environment::UpdateContent  @ 0x826C00B8  (bodied phase B4)
//
// Tick every live content slot: Content::Update(dt); clear the CHANGED high bit
// off mu8ContentState (console `if (state & ~0x7F) state &= 0x7F`); dispose
// entries whose mu8RemoveState reached E_CONTENT_REMOVE_REMOVED (Object::Release
// -- @0x82680938 on the console -- then null the slot); count muActiveContent.
// Bracketed by the miContentUpdate CPU monitor.
// =============================================================================
void Environment::UpdateContent(f32 af32TimeStep)
{
    CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miContentUpdate);

    muActiveContent = 0;
    for (u32 lu = 0; lu < mu32ContentCount; ++lu)
    {
        Content* lpContent = mphContent[lu].GetObject();
        if (!lpContent)
            continue;

        lpContent->Update(af32TimeStep);

        if (lpContent->HasContentStateChanged())
            lpContent->AcknowledgeContentStateChange();   // drop E_CONTENT_STATE_CHANGED

        if (lpContent->mu8RemoveState == E_CONTENT_REMOVE_REMOVED)
        {
            if (mphContent[lu].GetObject())
                mphContent[lu].GetObject()->Release();
            mphContent[lu].SetObject(0);
        }

        ++muActiveContent;
    }

    CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miContentUpdate);
}

// =============================================================================
// Environment::UpdateVoices  @ 0x826C01B8  (bodied phase B4)
//
// Zero muActiveVoices + the five mafVoiceTypeTickTotals, then tick every live
// voice: Voice::Update(system, dt); accumulate its GetCpuTicks() into the
// per-GetProfileVoiceType() tick total (console vtbl +8 / +16 -- Object's
// [dtor, DoDispose] head precedes Voice's own virtuals, so those slots ARE
// GetCpuTicks / GetProfileVoiceType); clear the CHANGED high bit off
// mu8PlaybackState; release voices whose mu8RemoveState reached the REMOVED(3)
// rung (the console inlines Object::Release -- refcount assert CgsObject.h:117,
// decrement, DoDispose at zero) and null the slot. Bracketed by the
// miVoiceUpdate CPU monitor.
// =============================================================================
void Environment::UpdateVoices(rw::audio::core::System* apSystem, f32 af32TimeStep)
{
    CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miVoiceUpdate);

    muActiveVoices = 0;
    for (u32 lu = 0; lu < 5; ++lu)
        mafVoiceTypeTickTotals[lu] = 0.0f;

    for (u32 lu = 0; lu < mu32VoiceCount; ++lu)
    {
        Voice* lpVoice = mphVoice[lu].GetObject();
        if (!lpVoice)
            continue;

        lpVoice->Update(apSystem, af32TimeStep);
        ++muActiveVoices;

        mafVoiceTypeTickTotals[lpVoice->GetProfileVoiceType()] += lpVoice->GetCpuTicks();

        if (lpVoice->HasPlaybackStateChanged())
            lpVoice->AcknowledgePlaybackStateChange();   // drop the CHANGED bit

        if (lpVoice->GetRemoveState() == E_VOICE_REMOVE_REMOVED)
        {
            Voice* lpDeadVoice = mphVoice[lu].GetObject();
            if (lpDeadVoice)
                lpDeadVoice->Release();
            mphVoice[lu].SetObject(0);
        }
    }

    CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miVoiceUpdate);
}

// =============================================================================
// Environment::UpdateFactories  @ 0x826A2200  (bodied phase B4)
//   per live factory: Factory::Update(dt) -> virtual DoUpdate (console vtbl +16 --
//   Factory slot [4]).
// =============================================================================
void Environment::UpdateFactories(f32 af32TimeStep)
{
    for (u32 lu = 0; lu < mu32FactoryCount; ++lu)
    {
        Factory* lpFactory = mphFactory[lu].GetObject();
        if (lpFactory)
            lpFactory->Update(af32TimeStep);   // the public front (DWARF h:310) -> the console vtbl+16 DoUpdate dispatch
    }
}

// =============================================================================
// Environment::Update  @ 0x826D7500  (bodied phase B4)
//
// The per-frame engine pump: UpdateContent, then -- under the RWAC system lock
// -- UpdateVoices + UpdateFactories, the command-ring high-water assert (the
// console builds "Command buffer high water mark. <n>" through a StrStream;
// kept as a static CGS_ASSERT string), unlock.
// =============================================================================
void Environment::Update(f32 af32TimeStep)
{
    UpdateContent(af32TimeStep);

    rw::audio::core::System* lpSystem = GetDefaultRwacSystem();
    CGS_ASSERT(lpSystem != 0, "mpSystem");
    rw::audio::core::RwacSystemLock(lpSystem);

    UpdateVoices(lpSystem, af32TimeStep);
    UpdateFactories(af32TimeStep);

    CGS_ASSERT(lpSystem->muDeferredRingHighWater < 157286u,
               "Command buffer high water mark.");

    rw::audio::core::RwacSystemUnlock(lpSystem);
}

} // namespace Playback
} // namespace CgsSound
