// ============================================================================
// CgsEnvironment.cpp -- CgsSound::Playback::Environment runtime bodies.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (wave 11):
//   Environment::AddFactory   @ 0x826AD130   Environment::AddVoice   @ 0x826AD1E0
//   Environment::AddContent   @ 0x826AD290   Environment::GetAllocator @ 0x82680EF8
//   Environment::GetRegistry  @ 0x82680EA0   Environment::GetV       @ 0x826BFAF0
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
#include "rw/rwcore_structs.h"                                     // rw::Resource / ResourceDescriptor / IResourceAllocator

#include <cstring>

namespace CgsSound
{
namespace Playback
{

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

// @ 0x826BFAF0. Look a voice up by ident. The X360 `*(voice + 0x0C)` is
// Voice::mIdent, read by name via the wave-1 GetIdent accessor; the handle table
// is walked through the real Handle<Voice> slots (one owned pointer each), not a
// reinterpreted Object** (2026-08-25 wave 3). Returns an acquired handle to the
// match, or an empty handle.
Handle<Voice> Environment::GetV(u32 au32Id)
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

// @ 0x826BFE50. Walk the voice table for the tagged factory's voice whose named
// slot resolves and whose plug-in table contains au32Plugin. Decoded by name
// (2026-08-25 wave 3):
//   * the "type tag" check is `voice->mFactory.mName == dword_83008650` (the asm
//     loads voice+8 -> factory, factory+8 -> mName) -- i.e. the voice belongs to a
//     SPECIFIC factory, matched by interned name;
//   * FindNamedSlot receives the interned Name at dword_830080A8 (one word built
//     on the stack and passed by the non-trivial-class reference ABI) -- the real
//     Voice::FindNamedSlot(Name) member.
// Returns an acquired handle to the first match, or an empty handle.
Handle<Content> Environment::GetR(u32 au32Plugin)
{
    Handle<Content> lhResult(static_cast<Content*>(0));
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

        // FLAG (offset reach into the voice's un-homed tail region): the matched
        // voice's plug-in table -- a sub-object at console voice+0x2C with the
        // plug-in array at +0x08 and its u16 count at +0x0C -- lies past the homed
        // Voice members (mOffsets ends the named layout at +0x2C; DWARF SubmixVoice
        // adds only mpSubmix there). Which concrete voice subclass owns this table
        // is not yet attested, so the walk stays byte-addressed until that home
        // lands. The assert strings are verbatim.
        u8* lpu8Named = reinterpret_cast<u8*>(lpVoice);
        u16 lu16PluginCount = *reinterpret_cast<u16*>(lpu8Named + 0x38);
        u8* lpPluginTable = lpu8Named + 0x2C;
        bool lbMatched = false;
        for (u32 lu32P = 0; lu32P < lu16PluginCount; ++lu32P)
        {
            CGS_ASSERT(lu32P < *reinterpret_cast<u16*>(lpPluginTable + 0x0C), "lu32I < mu16PluginCount");
            void** lppPlugins = *reinterpret_cast<void***>(lpPluginTable + 0x08);
            CGS_ASSERT(lppPlugins[lu32P], "mppPlugin[lu32I]");
            if (reinterpret_cast<uintptr_t>(lppPlugins[lu32P]) == au32Plugin) { lbMatched = true; break; }
        }

        if (lbMatched)
        {
            lhResult.SetObject(reinterpret_cast<Content*>(lpVoice));
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
                         + 7u * sizeof(void*)                    // registry header words
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

} // namespace Playback
} // namespace CgsSound
