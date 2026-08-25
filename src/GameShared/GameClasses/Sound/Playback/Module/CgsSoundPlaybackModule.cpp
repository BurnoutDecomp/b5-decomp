// =============================================================================
// CgsSound::Playback::Module::Module -- out-of-line members.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Module::Module            @ 0x827DFA98  (ctor -- EXECUTED in boot trace)
//   Module::GetEnvironment    @ 0x82694130
//   Module::DumpRegistries    @ 0x82694188
//   Module::ImportStringTable @ 0x826AD6B0
//   Module::AttachVoice       @ 0x826D7D80
//   Module::ConnectVoice      @ 0x826C14E0
//   Module::C  (CreateVoice)  @ 0x826D7B00
//
// Each method bodies store-for-store against the X360 asm. The asserts use the
// project CGS_ASSERT macro (the X360 BeginAssert/FireAssert/EndAssert triple);
// CGS_ASSERT supplies __FILE__/__LINE__ itself, so the original file/line numbers
// are dropped per AGENTS.md.
// =============================================================================

#include "GameShared/GameClasses/Sound/Playback/Module/CgsSoundPlaybackModule.h"

#include "rw/rwcore_structs.h"   // rw::Resource / rw::IResourceAllocator (the Release-machine DoFree wraps)

#include <cstring>  // memcpy

namespace CgsSound
{
namespace Playback
{
namespace Module
{

// ---------------------------------------------------------------------------
// Module::Module  @ 0x827DFA98
//
// The X360 ctor runs the ModuleSingleBuffered base ctor (the intermediate vtable
// off_820CE500 + the two RWMutex(NULL, true) builds @+0x10/+0x118 -- since phase
// B1 that IS the inherited base ctor here), installs the two interface-base
// vptrs (off_820CE350/58, overwritten with the final off_820CFA24/20 by the
// derived hand-off), zeroes the environment + factory handles, builds the
// stream mutex (Mutex(NULL, true) -- asm r4=0, r5=1), zeroes the deferred
// queue's leading count byte (+0x2298; the queue's Construct() re-establishes
// it), and nulls the string-table head.
//
// FLAG (interface bases): the +0x228/+0x22C vptr slots' concrete interfaces are
// not typed yet (the +0x22C one is the DWARF IContentLoadService), so the raw
// slots are left null rather than fabricating tables.
// ---------------------------------------------------------------------------
Module::Module()
    : CgsModule::ModuleSingleBuffered()
    , mpSecondaryVtbl(0)     // X360: off_820CE350 then off_820CFA24 (see FLAG)
    , mpTertiaryVtbl(0)      // X360: off_820CE358 then off_820CFA20 (see FLAG)
    , mhEnvironment()        // X360 +0x2258 = 0
    , mhRwacFactory()        // X360 +0x225C = 0
    , mhAemsFactory()        // X360 +0x2260 = 0
    , mhSplicerFactory()     // X360 +0x2264 = 0 (DWARF h:374)
    , mStreamMutex(0, true)  // X360 +0x2268: Mutex(NULL, true) (DWARF h:379)
    , mpStringTable(0)       // X360 +0x26FC (DWARF h:489)
{
}

// ---------------------------------------------------------------------------
// Module::Construct(int)  @ 0x826C0EC0  (vtable +0x40; RootSoundModule::Construct
// step [3] calls it with module-id 6)
//
// Store map (console offsets; every store lands on a named member):
//   +0x2700 mf32TimeStep = 0.0    +0x2708 mi32PoolId = a2   +0x2704 mf32TotalTime = 0.0
//   +0x26F0/F4/F8 sizes + flag = 0
//   ModuleSingleBuffered::Construct(this)
//   +0x230 mePrepareStage = 0     +0x234 meReleaseStage = 6 (DONE)
//   receiver queue: cap 0x2000 @+0x248, align 16 @+0x24C, storage this+0x250
//     @+0x238, then Clear  == EventReceiverQueue<0x2000,16>::Construct()
//   +0x26FC mpStringTable = 0     +0x2250/+0x2254 in/out buffers = 0
//   VariableEventQueue<1024,16>::Construct(+0x2298)
//   the 3 StreamBuffer records: {0, 4, 0, 0, 0, 0.0} each
//   +4 mbIsNewModule = 1 (base)
// ---------------------------------------------------------------------------
void Module::Construct(s32 li32PoolId)
{
    mf32TimeStep  = 0.0f;
    mi32PoolId    = li32PoolId;
    mf32TotalTime = 0.0f;

    muStreamBufferSize          = 0;
    muStreamNumBlocks           = 0;
    mbStreamsUsingMainAllocator = false;

    CgsModule::ModuleSingleBuffered::Construct();

    mePrepareStage = E_PREPARESTAGE_0;
    meReleaseStage = E_RELEASESTAGE_DONE;

    mResourceReceiverQueue.Construct();

    mpStringTable  = 0;
    mpInputBuffer  = 0;
    mpOutputBuffer = 0;

    mDeferredResourceRequestQueue.Construct();

    for (u32 lu = 0; lu < 3; ++lu)
    {
        maStreamBuffers[lu].mpBuffer       = 0;
        maStreamBuffers[lu].mu32Reserved04 = 4;
        maStreamBuffers[lu].mu32Reserved08 = 0;
        maStreamBuffers[lu].mu32Reserved0C = 0;
        maStreamBuffers[lu].mu32Reserved10 = 0;
        maStreamBuffers[lu].mf32Reserved14 = 0.0f;
    }

    mbIsNewModule = true;
}

// ---------------------------------------------------------------------------
// Module::Release  @ 0x826C0FB8  (the release COUNTDOWN machine, run forward
// from meReleaseStage; each rung bumps the cursor via the @0x82681CD0 operator)
// ---------------------------------------------------------------------------
bool Module::Release()
{
    rw::IResourceAllocator* lpMainAllocator = 0;

    switch (meReleaseStage)
    {
    case 0:
        meReleaseStage++;
        // fall through
    case 1:
        // Free the string-table chain through the environment allocator's DoFree
        // (each chunk wrapped as a {ptr, 0 x4} resource -- the wave-3 dispose idiom).
        while (mpStringTable)
        {
            StringTable* lpNext = mpStringTable->mpNext;
            Environment* lpEnvironment = GetEnvironment();
            rw::Resource lResource;
            lResource.m_baseResources[0] = mpStringTable;
            for (u32 lu = 1; lu < 4; ++lu)
                lResource.m_baseResources[lu] = 0;
            lpEnvironment->GetAllocator()->DoFree(lResource);
            mpStringTable = lpNext;
        }
        meReleaseStage++;
        // fall through
    case 2:
        // Null-assign the three factory handles (the console's Handle null-assign
        // helpers @0x826A76A8 family: drop the owned reference, then store null).
        if (mhRwacFactory.GetObject())
            mhRwacFactory.GetObject()->Release();
        mhRwacFactory.SetObject(0);
        if (mhAemsFactory.GetObject())
            mhAemsFactory.GetObject()->Release();
        mhAemsFactory.SetObject(0);
        if (mhSplicerFactory.GetObject())
            mhSplicerFactory.GetObject()->Release();
        mhSplicerFactory.SetObject(0);
        meReleaseStage++;
        // fall through
    case 3:
        // Snapshot the main allocator BEFORE dropping the environment handle (the
        // stream-buffer frees below need it once the environment is gone).
        lpMainAllocator = GetEnvironment()->GetAllocator();
        if (mhEnvironment.GetObject())
            mhEnvironment.GetObject()->Release();
        mhEnvironment.SetObject(0);
        meReleaseStage++;
        // fall through
    case 4:
        for (u32 lu = 0; lu < 3; ++lu)
        {
            void* lpBuffer = maStreamBuffers[lu].mpBuffer;
            maStreamBuffers[lu].mu32Reserved08 = 0;
            maStreamBuffers[lu].mpBuffer       = 0;
            if (mbStreamsUsingMainAllocator)
            {
                CGS_ASSERT(lpMainAllocator != 0, "lpMainAllocator");
                rw::Resource lResource;
                lResource.m_baseResources[0] = lpBuffer;
                for (u32 luI = 1; luI < 4; ++luI)
                    lResource.m_baseResources[luI] = 0;
                lpMainAllocator->DoFree(lResource);
            }
        }
        meReleaseStage++;   // the inlined @0x82681CD0 bump (bound assert inside)
        // fall through
    case 5:
        if (!mDeferredResourceRequestQueue.Release())
            return false;
        mResourceReceiverQueue.Clear();
        if (!CgsModule::ModuleSingleBuffered::Release())
            return false;
        meReleaseStage++;
        // fall through
    case E_RELEASESTAGE_DONE:
        mePrepareStage = E_PREPARESTAGE_0;
        return true;
    default:
        CGS_ASSERT(false, "Invalid Release Stage");
        return false;
    }
}

// ---------------------------------------------------------------------------
// Module::Destruct  @ 0x826C1268
// ---------------------------------------------------------------------------
void Module::Destruct()
{
    mDeferredResourceRequestQueue.Destruct();
    mResourceReceiverQueue.Clear();
    CgsModule::ModuleSingleBuffered::Destruct();
}

// ---------------------------------------------------------------------------
// Module::GetEnvironment  @ 0x82694130
// Asserts the environment handle is non-null (CgsHandle.h:305 -> "mpObject") then
// returns the owned Environment pointer.
// ---------------------------------------------------------------------------
Environment* Module::GetEnvironment()
{
    CGS_ASSERT(mhEnvironment.GetObject(), "mpObject");
    return mhEnvironment.GetObject();
}

// ---------------------------------------------------------------------------
// Module::DumpRegistries  @ 0x82694188
// Dumps the registries owned by the environment, the RWAC factory and the AEMS
// factory in turn. Each owning object is asserted non-null ("mpObject") and each
// registry pointer is asserted non-null ("mpRegistry") before the dump.
//
// X360 member reads: a1[2198] = mhEnvironment (env, registry @ env+0x4C),
// a1[2199] = mhRwacFactory (factory, registry @ +0x401C),
// a1[2200] = mhAemsFactory (factory, registry @ +0x60).
// ---------------------------------------------------------------------------
void Module::DumpRegistries()
{
    Environment* lpEnvironment = mhEnvironment.GetObject();
    CGS_ASSERT(lpEnvironment, "mpObject");
    Registry* lpRegistry = lpEnvironment->GetRegistry();
    CGS_ASSERT(lpRegistry, "mpRegistry");
    lpRegistry->Dump();

    Factory* lpRwacFactory = mhRwacFactory.GetObject();
    CGS_ASSERT(lpRwacFactory, "mpObject");
    GetRwacFactoryRegistry(lpRwacFactory)->Dump();

    Factory* lpAemsFactory = mhAemsFactory.GetObject();
    CGS_ASSERT(lpAemsFactory, "mpObject");
    GetAemsFactoryRegistry(lpAemsFactory)->Dump();
}

// ---------------------------------------------------------------------------
// Module::ImportStringTable  @ 0x826AD6B0
//
// Imports a CSIS string-table resource: if the resource has a populated string
// region, allocate an environment-owned chunk of (size + 4) bytes, push it onto the
// mpStringTableHead list, copy the string bytes in, then walk every NUL-terminated
// string in the copied region and intern it via Name::MakeHash.
//
// The resource layout the asm reads (a2 = lpStringTableResource):
//   a2[4]            (+0x10) -- "has string region" flag (the outer `if`)
//   a2[5]            (+0x14) -- end pointer of the string region (v4)
//   a2[1], a2[2]     (+0x04, +0x08) -- count and byte-offset used to compute the
//                      start pointer:  start = &a2[a2[1] + 7] + a2[2]
//                      i.e. base = (const char*)a2 + (a2[1] + 7) * 4 + a2[2]
//   size = end - start  (v6, the byte length to copy)
// ---------------------------------------------------------------------------
void Module::ImportStringTable(const u32* lpStringTableResource)
{
    if (!lpStringTableResource[4])
        return;

    const u8* lkpu8Base = reinterpret_cast<const u8*>(lpStringTableResource);
    const char* lkpcEnd = reinterpret_cast<const char*>(
        static_cast<uintptr_t>(lpStringTableResource[5]));
    const char* lkpcStart = reinterpret_cast<const char*>(
        lkpu8Base + (lpStringTableResource[1] + 7) * 4 + lpStringTableResource[2]);

    u32 lu32StringTableSize =
        static_cast<u32>(lkpcEnd - lkpcStart);
    if (lkpcEnd == lkpcStart)
        return;

    Environment* lpEnvironment = mhEnvironment.GetObject();
    CGS_ASSERT(lpEnvironment, "mpObject");

    Module::StringTable* lpChunk = static_cast<Module::StringTable*>(
        lpEnvironment->Allocate(lu32StringTableSize + 4, 4, "StringTable"));
    if (!lpChunk)
        return;

    // Push the new chunk onto the head of the module's string-table list.
    lpChunk->mpNext = mpStringTable;
    mpStringTable   = lpChunk;

    // Re-derive the source start (the X360 recomputes it under the same a2[4] guard).
    const char* lkpcSource = reinterpret_cast<const char*>(
        lkpu8Base + (lpStringTableResource[1] + 7) * 4 + lpStringTableResource[2]);
    std::memcpy(lpChunk->macData, lkpcSource, lu32StringTableSize);

    // Intern every NUL-terminated string in the copied region.
    char* lpcCursor = lpChunk->macData;
    do
    {
        char* lpcScan = lpcCursor;
        while (*lpcScan++)
            ;
        u32 lu32StringLen = static_cast<u32>(lpcScan - lpcCursor);
        CGS_ASSERT(lu32StringTableSize >= lu32StringLen,
                   "luStringTableSize >= luStringLen");
        Name::MakeHash(lpcCursor);
        lu32StringTableSize -= lu32StringLen;
        lpcCursor           += lu32StringLen;
    } while (lu32StringTableSize);
}

// ---------------------------------------------------------------------------
// Module::AttachVoice  @ 0x826D7D80  (DWARF h:345)
// Attaches lphContent to the named slot lu32SlotName on lphVoice (the real
// Voice::Attach(Name, Handle<Content>&) -- the u32 the asm builds on the stack and
// passes by reference IS the interned slot Name), then releases the transient
// references held by both handles (the X360 drops a ref on each owned object).
// ---------------------------------------------------------------------------
void Module::AttachVoice(Handle<Voice>* lphVoice, Handle<Content>* lphContent,
                         u32 lu32SlotName)
{
    CGS_ASSERT(lphVoice->GetObject(), "lhVoice");
    CGS_ASSERT(lphContent->GetObject(), "lhContent");

    CGS_ASSERT(lphVoice->GetObject(), "mpObject");

    bool lbAttached =
        lphVoice->GetObject()->Attach(Name(static_cast<uintptr_t>(lu32SlotName)),
                                      *lphContent);
    CGS_ASSERT(lbAttached, "lhVoice->Attach(lSlotName, lhContent)");

    if (lphVoice->GetObject())
        lphVoice->GetObject()->Release();
    if (lphContent->GetObject())
        lphContent->GetObject()->Release();
}

// ---------------------------------------------------------------------------
// Module::ConnectVoice  @ 0x826C14E0
// Resolves the submix voice for lu32SubmixId out of the environment, takes a
// reference on it, connects lphVoice's send lu32SendName to it, then releases the
// transient references.
// ---------------------------------------------------------------------------
void Module::ConnectVoice(Handle<Voice>* lphVoice, u32 lu32SendName, u32 lu32SubmixId)
{
    Environment* lpEnvironment = mhEnvironment.GetObject();
    CGS_ASSERT(lpEnvironment, "mpObject");

    // GetVoice (the IDA-truncated `GetV`, DWARF h:289) fills a transient handle
    // (var_80); the X360 reads the owned Voice*, takes a ref on it, then releases
    // the transient handle's reference.
    Handle<Voice> lhSubmixLookup = lpEnvironment->GetVoice(lu32SubmixId);
    Voice* lpSubmixVoice = lhSubmixLookup.GetObject();
    if (lpSubmixVoice)
        lpSubmixVoice->Acquire();
    if (lhSubmixLookup.GetObject())
        lhSubmixLookup.GetObject()->Release();

    CGS_ASSERT(lphVoice->GetObject(), "lhVoice");
    CGS_ASSERT(lpSubmixVoice, "lhVoice");  // X360 streams "Submix ID: <id>" here

    CGS_ASSERT(lphVoice->GetObject(), "mpObject");

    // The real Voice::Connect(Name, Handle<SubmixVoice>&) @0x826ACC90 -- the u32
    // built on the stack is the interned send Name, and the submix voice is the
    // SubmixVoice subclass (CgsSubmixVoice.h).
    Handle<SubmixVoice> lhSubmix(static_cast<SubmixVoice*>(lpSubmixVoice));
    bool lbConnected =
        lphVoice->GetObject()->Connect(Name(static_cast<uintptr_t>(lu32SendName)),
                                       lhSubmix);
    CGS_ASSERT(lbConnected, "lhVoice->Connect(lSendName, lhSubmixVoice)");

    if (lpSubmixVoice)
        lpSubmixVoice->Release();
    if (lphVoice->GetObject())
        lphVoice->GetObject()->Release();
}

// ---------------------------------------------------------------------------
// Module::CreateVoice  @ 0x826D7B00  (the IDA-truncated "Module::C"; DWARF h:318
// `Handle<Voice> CreateVoice(u32,u32,u32)` -- the by-value Handle return is the
// sret out-pointer this signature spells)
//
// Resolves the owning factory (by lFactoryName, the real Environment::GetFactory
// -- IDA's `Environment::Ge`) and the voice spec (by lu32SubmixName, looked up in
// the environment registry via the real Registry::GetEntity<VoiceSpec>) out of the
// environment, asks the factory to CreateVoice for the requested ident, runs the
// init-submix hack on the reserved ident, and stores the new voice into
// lphVoiceOut. On any failure path it clears lphVoiceOut and releases the
// resolved factory handle.
//
// FLAG: the X360 emits a streamed assert message
// ("E_COMMAND_VOICE_CREATE failed: VoiceSpec\n") on the failure path; reconstructed
// here as a plain CGS_ASSERT(false, ...) with the message text, matching the
// fire-assert effect (the StrStream machinery is style-only).
// ---------------------------------------------------------------------------
void Module::CreateVoice(Handle<Voice>* lphVoiceOut, u32 lu32SlotName,
                         const Name& lFactoryName, u32 lu32SubmixName)
{
    Environment* lpEnvironment = mhEnvironment.GetObject();
    CGS_ASSERT(lpEnvironment, "mpObject");

    // Resolve the owning factory handle by name (Environment::GetFactory, DWARF h:280).
    Handle<Factory> lhFactory = lpEnvironment->GetFactory(lFactoryName);
    Factory* lpFactory = lhFactory.GetObject();

    const VoiceSpec* lpSpec = 0;
    if (lpFactory)
    {
        Environment* lpEnv2 = mhEnvironment.GetObject();
        CGS_ASSERT(lpEnv2, "mpObject");
        Registry* lpRegistry = lpEnv2->GetRegistry();
        CGS_ASSERT(lpRegistry, "mpRegistry");
        Name lSpecName(static_cast<uintptr_t>(lu32SubmixName));
        lpSpec = lpRegistry->GetEntity<VoiceSpec>(lSpecName);
    }

    if (!lpFactory || !lpSpec)
    {
        CGS_ASSERT(false, "E_COMMAND_VOICE_CREATE failed: VoiceSpec");
        lphVoiceOut->SetObject(0);
        if (lpFactory)
            lpFactory->Release();
        return;
    }

    Handle<Voice> lhNewVoice;
    if (lpFactory->CreateVoice<Voice>(*lpSpec, lhNewVoice, lu32SlotName) ==
        static_cast<u32>(-1))
    {
        if (lhNewVoice.GetObject())
            lhNewVoice.GetObject()->Release();
        CGS_ASSERT(false, "E_COMMAND_VOICE_CREATE failed: VoiceSpec");
        lphVoiceOut->SetObject(0);
        if (lpFactory)
            lpFactory->Release();
        return;
    }

    // The `*(voice+12) = ident` store: the real Voice::SetIdent (DWARF CgsVoice.h:567
    // -- the old rival named it SetSlotName; the +0xC word is Voice::mIdent).
    lhNewVoice.GetObject()->SetIdent(lu32SlotName);

    if (lu32SlotName == KU_INIT_SND9_SUBMIX_IDENT)
    {
        CGS_ASSERT(lFactoryName == GenericRwacFactorySkName(),
                   "GenericRwacFactory::SK_NAME == lFactoryName");
        Handle<Voice> lhInitVoice(lhNewVoice.GetObject());
        if (lhInitVoice.GetObject())
            lhInitVoice.GetObject()->Acquire();
        HACK_SetSnd9InitSubmix(&lhInitVoice);
    }

    Voice* lpNewVoice = lhNewVoice.GetObject();
    lphVoiceOut->SetObject(lpNewVoice);
    if (!lpNewVoice)
    {
        if (lpFactory)
            lpFactory->Release();
        return;
    }

    lpNewVoice->Acquire();
    lpNewVoice->Release();
    lpFactory->Release();
}

// ---------------------------------------------------------------------------
// Module::CreateContent  @ 0x826C12A8
//
// Mirror of the sibling Module::C (CreateVoice @0x826D7B00). Resolves the owning
// Factory (by lContentClassName) and the ContentSpec (by lContentSpecName) out of the
// environment, asks the factory to CreateContent, wires the created Content's ident
// (+0x10 = mIdent) and owner iface (+0x14 = mpLoadService, = this+0x22C when non-null),
// stores it into *lppContentOut, refs it, and releases the transient factory handle.
// On failure it clears *lppContentOut and releases the factory handle.
//
// FLAG: the X360 streams an assert message on the failure path; reconstructed as a
// plain CGS_ASSERT(false, <leading literal>) exactly as Module::C reduced its own
// streamed voice-create failure -- the StrStream interpolation + trailing \n dropped.
// ---------------------------------------------------------------------------
Content** Module::CreateContent(Content** lppContentOut, u32 lu32Ident,
                                const Name& lContentClassName,
                                const Name& lContentSpecName)
{
    Environment* lpEnvironment = mhEnvironment.GetObject();
    CGS_ASSERT(lpEnvironment, "mpObject");

    // Resolve the owning content factory handle by class name (Environment::GetFactory).
    Handle<Factory> lhFactory = lpEnvironment->GetFactory(lContentClassName);
    Factory* lpFactory = lhFactory.GetObject();

    const ContentSpec* lpContentSpec = 0;
    if (lpFactory)
    {
        Environment* lpEnv2 = mhEnvironment.GetObject();
        CGS_ASSERT(lpEnv2, "mpObject");
        Registry* lpRegistry = lpEnv2->GetRegistry();
        CGS_ASSERT(lpRegistry, "mpRegistry");
        Name lSpecName(lContentSpecName);
        lpContentSpec = lpRegistry->GetEntity<ContentSpec>(lSpecName);
    }

    if (!lpFactory || !lpContentSpec)
    {
        CGS_ASSERT(false, "E_COMMAND_CONTENT_CREATE failed with content spec ");
        *lppContentOut = 0;
        if (lpFactory)
            lpFactory->Release();
        return lppContentOut;
    }

    Handle<Content> lhContent;
    if (lpFactory->CreateContent(*lpContentSpec, lhContent, lu32Ident) ==
        static_cast<u32>(-1))
    {
        if (lhContent.GetObject())
            lhContent.GetObject()->Release();
        CGS_ASSERT(false, "E_COMMAND_CONTENT_CREATE failed with content spec ");
        *lppContentOut = 0;
        if (lpFactory)
            lpFactory->Release();
        return lppContentOut;
    }

    // Wire the freshly created content's owner fields BY NAME (the module's own
    // `stw +0x10 / stw +0x14` stores after the factory call): mIdent, and the
    // IContentLoadService base sub-object pointer (this+0x22C -- the DWARF-declared
    // `: protected IContentLoadService` base this class models as the tertiary
    // vptr slot until the MI shape is recovered).
    (*lhContent).mIdent = lu32Ident;                                    // Content +0x10
    void* lpOwner = this ? reinterpret_cast<char*>(this) + 0x22C : 0;
    (*lhContent).mpLoadService = lpOwner;                               // Content +0x14

    Content* lpContent = lhContent.GetObject();
    *lppContentOut = lpContent;
    if (!lpContent)
    {
        if (lpFactory)
            lpFactory->Release();
        return lppContentOut;
    }

    lpContent->Acquire();   // X360: ++*(content+4) inline
    lpContent->Release();
    lpFactory->Release();
    return lppContentOut;
}

// ---------------------------------------------------------------------------
// operator++(Module::EPrepareStage&, int)  @ 0x82681C70
//   old=*a1; new=old+1; *a1=new (stored UNCONDITIONALLY, before the guard);
//   assert(new <= E_PREPARESTAGE_DONE); return old. The increment applies even on the
//   assert path; the return is always the saved OLD stage.
// ---------------------------------------------------------------------------
Module::EPrepareStage operator++(Module::EPrepareStage& leEnumIndex, int)
{
    const Module::EPrepareStage leOldEnumIndex = leEnumIndex;
    leEnumIndex = static_cast<Module::EPrepareStage>(static_cast<s32>(leEnumIndex) + 1);

    CGS_ASSERT(leEnumIndex <= Module::E_PREPARESTAGE_DONE,
               "leEnumIndex <= Module::E_PREPARESTAGE_DONE");

    return leOldEnumIndex;
}

// ---------------------------------------------------------------------------
// operator++(Module::EReleaseStage&, int)  @ 0x82681CD0
//   Same shape as the prepare sibling: store the incremented cursor
//   UNCONDITIONALLY, assert the bound (CgsSoundPlaybackModule.h:501,
//   "leEnumIndex <= Module::E_RELEASESTAGE_DONE" == 6), return the saved OLD
//   value. Release's case-4 rung inlines this exact sequence.
// ---------------------------------------------------------------------------
Module::EReleaseStage operator++(Module::EReleaseStage& leEnumIndex, int)
{
    const Module::EReleaseStage leOldEnumIndex = leEnumIndex;
    leEnumIndex = static_cast<Module::EReleaseStage>(static_cast<s32>(leEnumIndex) + 1);

    CGS_ASSERT(leEnumIndex <= Module::E_RELEASESTAGE_DONE,
               "leEnumIndex <= Module::E_RELEASESTAGE_DONE");

    return leOldEnumIndex;
}

} // namespace Module
} // namespace Playback
} // namespace CgsSound
