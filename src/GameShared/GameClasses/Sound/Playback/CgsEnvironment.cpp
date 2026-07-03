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
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacFactory.h"

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

// @ 0x82680EF8. Return the owning rw allocator (asserted present).
rw::IResourceAllocator* Environment::GetAllocator()
{
    CGS_ASSERT(mpAllocator, "mpAllocator");
    return mpAllocator;
}

// @ 0x82680EA0. Return the owned entity registry (asserted present).
Registry* Environment::GetRegistry()
{
    CGS_ASSERT(mpRegistry, "mpRegistry");
    return mpRegistry;
}

// @ 0x826BFAF0. Look a voice up by ident (the u32 word at voice +0x0C). Returns an
// acquired handle to the match, or an empty handle.
Handle<Voice> Environment::GetV(u32 au32Id)
{
    Handle<Voice> lhResult(static_cast<Voice*>(0));
    if (mu32VoiceCount)
    {
        Object** lppVoices = reinterpret_cast<Object**>(mphVoice);
        for (u32 lu32I = 0; lu32I < mu32VoiceCount; ++lu32I)
        {
            Object* lpVoice = lppVoices[lu32I];
            if (lpVoice)
            {
                CGS_ASSERT(lpVoice, "mpObject");
                if (au32Id == *reinterpret_cast<u32*>(reinterpret_cast<u8*>(lpVoice) + 0x0C))
                {
                    Object* lpFound = lppVoices[lu32I];
                    lhResult.SetObject(reinterpret_cast<Voice*>(lpFound));
                    if (lpFound) { lpFound->Acquire(); }
                    return lhResult;
                }
            }
        }
    }
    lhResult.SetObject(0);
    return lhResult;
}

// @ 0x826BFE50. Walk the voice table for a voice of the tagged type whose named-slot
// spec resolves and whose plug-in table contains au32Plugin. Returns an acquired
// handle to the first match, or an empty handle.
Handle<Content> Environment::GetR(u32 au32Plugin)
{
    Handle<Content> lhResult(static_cast<Content*>(0));
    if (mu32VoiceCount == 0)
    {
        lhResult.SetObject(0);
        return lhResult;
    }

    Object** lppVoices = reinterpret_cast<Object**>(mphVoice);
    for (u32 lu32I = 0; lu32I < mu32VoiceCount; ++lu32I)
    {
        Object* lpVoice = lppVoices[lu32I];
        if (!lpVoice) { continue; }
        u8* lpu8Voice = reinterpret_cast<u8*>(lpVoice);
        void* lpTypeObj = *reinterpret_cast<void**>(lpu8Voice + 8);
        if (*reinterpret_cast<u32*>(reinterpret_cast<u8*>(lpTypeObj) + 8) != gu32VoiceTypeTag) { continue; }

        u32 lau32NamedSlot[40];
        lau32NamedSlot[0] = gu32NamedSlotSentinel;
        CGS_ASSERT(lppVoices[lu32I], "mpObject");
        if (!Voice::FindNamedSlot(reinterpret_cast<Voice*>(lppVoices[lu32I]), lau32NamedSlot)) { continue; }

        Object* lpNamed = lppVoices[lu32I];
        CGS_ASSERT(lpNamed, "mpObject");
        lpNamed->Acquire();

        u8* lpu8Named = reinterpret_cast<u8*>(lpNamed);
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
            Object* lpFound = lppVoices[lu32I];
            lhResult.SetObject(reinterpret_cast<Content*>(lpFound));
            if (lpFound) { lpFound->Acquire(); }
            lpNamed->Release();
            return lhResult;
        }
        lpNamed->Release();
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

// @ 0x826BFD60. Allocator-keyed placement delete: build the free request and dispatch
// it through the allocator vtable (slot 5).
void Environment::operator delete(void* lpMemory, rw::IResourceAllocator* lpAllocator)
{
    CGS_ASSERT(lpAllocator, "lpAllocator");

    void* lapRequest[6];
    lapRequest[0] = lpMemory;
    std::memset(&lapRequest[1], 0, 16);

    void** lppVtbl = *reinterpret_cast<void***>(lpAllocator);
    reinterpret_cast<int (*)(void*, void*)>(lppVtbl[5])(lpAllocator, lapRequest);
}

// @ 0x826ACF98. Placement new from an EnvironmentSpec: size the carve (handle tables
// + registry data + string table) and allocate it through the spec allocator vtable
// (slot 4). spec[4]/[5]/[6] are the RegistrySpec size fields read positionally.
void* Environment::operator new(size_t luSize, const EnvironmentSpec& lrSpec)
{
    CGS_ASSERT(luSize == 112, "sizeof(Environment) == luSize");
    CGS_ASSERT(lrSpec.mpAllocator, "lEnvironmentSpec.mpAllocator");

    const u32* lpu32Spec = reinterpret_cast<const u32*>(&lrSpec);
    rw::IResourceAllocator* lpAllocator = lrSpec.mpAllocator;

    u32 lu32Handles = lrSpec.mu32FactoryCount + lrSpec.mu32VoiceCount + lrSpec.mu32ContentCount;
    u32 lu32Total = 4u * (lu32Handles + lpu32Spec[4] + 35u) + lpu32Spec[6] + lpu32Spec[5];

    u64 lu64Request = (static_cast<u64>(lu32Total) << 32) | 4u;

    u8 lau8RetBuf[32];
    void** lppVtbl = *reinterpret_cast<void***>(lpAllocator);
    void* lpResult = reinterpret_cast<void* (*)(void*, void*, void*, const char*)>(lppVtbl[4])(
        lau8RetBuf, lpAllocator, &lu64Request, "Environment");
    return *reinterpret_cast<void**>(lpResult);
}

} // namespace Playback
} // namespace CgsSound
