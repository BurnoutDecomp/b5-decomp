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
// The X360 ctor installs an intermediate vtable set (off_820CE500 / off_820CE350 /
// off_820CE358) -- the base-subobject ctors -- then overwrites the primary and the
// two secondary vtable slots with the final derived set (off_820CFA30 /
// off_820CFA24 / off_820CFA20). The RWMutex / Mutex sub-objects are default-
// constructed (RWMutex(NULL, true) / Mutex(NULL, true)); the asm passes (r4=0,
// r5=1) which is (params=NULL, bDefaultParameters=true). The environment + factory
// handles, the reserved handle and the reserved flag are all zeroed.
//
// FLAG (unrecovered rodata): the three vtable symbol addresses (off_820CFA30 /
// off_820CFA24 / off_820CFA20 and the intermediate off_820CE5xx) are this object's
// vtables; their concrete contents are not recovered by this TU, so the raw vtable
// slots are left null here rather than fabricating a table. The mutex construction,
// handle zeroing and flag zeroing -- the observable ctor side effects -- are exact.
// ---------------------------------------------------------------------------
Module::Module()
    : mpVtbl(0)              // X360: off_820CE500 then off_820CFA30 (see FLAG)
    , mInputLock(0, true)    // X360 +0x10: RWMutex(NULL, true)
    , mOutputLock(0, true)   // X360 +0x118: RWMutex(NULL, true)
    , mpSecondaryVtbl(0)     // X360: off_820CE350 then off_820CFA24 (see FLAG)
    , mpTertiaryVtbl(0)      // X360: off_820CE358 then off_820CFA20 (see FLAG)
    , mhEnvironment()        // X360 +0x2258 = 0
    , mhRwacFactory()        // X360 +0x225C = 0
    , mhAemsFactory()        // X360 +0x2260 = 0
    , mhSplicerFactory()     // X360 +0x2264 = 0 (DWARF h:374)
    , mStreamMutex(0, true)  // X360 +0x2268: Mutex(NULL, true) (DWARF h:379)
    , mbReserved(false)      // X360 +0x2298 = 0
    , mpStringTable(0)       // X360 +0x26FC (DWARF h:489)
{
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
EPrepareStage operator++(EPrepareStage& leEnumIndex, int)
{
    const EPrepareStage leOldEnumIndex = leEnumIndex;
    leEnumIndex = static_cast<EPrepareStage>(static_cast<s32>(leEnumIndex) + 1);

    CGS_ASSERT(leEnumIndex <= E_PREPARESTAGE_DONE,
               "leEnumIndex <= Module::E_PREPARESTAGE_DONE");

    return leOldEnumIndex;
}

} // namespace Module
} // namespace Playback
} // namespace CgsSound
