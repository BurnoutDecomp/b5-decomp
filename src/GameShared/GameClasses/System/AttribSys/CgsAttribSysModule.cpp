#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysModule.h"

#include <cstddef>                                                 // native layout assertions
#include <new>                                                     // placement new (RegisterSchema)
#include <string.h>                                                // strcmp (channel match)
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"                 // CGS_ASSERT
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysModuleIO.h" // AttribSysIO::InputBuffer + requests + Inp
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"          // LinearMalloc::Malloc (schema vault)
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Windows/CgsLogWindow.h" // the debug log window
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/attribsys.h"   // Attrib::Database
#include "SDKs/EA/GameTalk/GameTalk.h"                              // EA::GameTalk::GameTalkMessage accessors
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/attriblivelink.h" // Attrib::DecodeLiveLinkMessage

#define CGS_ARTIST_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\attribsys\\CgsAttribSysModule.cpp"

namespace CgsAttribSys
{
// ---- file-scope statics (CgsAttribSysModule.cpp:31/34) ----------------------------------
// sbSchemaLoaded enforces "only one schema loaded at a time"; spVaultArray is published by
// Construct (X360 dword_83011BB4) and handed back by GetVaultArray.
bool        AttribSysModule::sbSchemaLoaded = false;
VaultArray* AttribSysModule::spVaultArray   = 0;
// ARTIST off_82F31050 is shared with AttribSysDebugComponent::OnActivate.
CgsDev::DebugUI::LogWindow sLogWindow;

namespace
{
    // X360 schema vault allocation size (CgsAttribSysModule.cpp:151 -> LinearMalloc::Malloc(.., 88)).
    const s32 KI_SCHEMA_VAULT_BYTES = 88;
}

// EPrepareStage post-increment (CgsAttribSysModule.h:230, X360 0x828024C0): step the stage and
// assert it never advances past E_PREPARESTAGE_DONE.
AttribSysModule::EPrepareStage operator++(AttribSysModule::EPrepareStage& leStage, int)
{
    const AttribSysModule::EPrepareStage leOld = leStage;
    leStage = static_cast<AttribSysModule::EPrepareStage>(leStage + 1);
    CGS_ASSERT(leStage <= AttribSysModule::E_PREPARESTAGE_DONE,
        CGS_ASSERT_MESSAGE_AT(
            "leEnumIndex <= AttribSysModule::E_PREPARESTAGE_DONE",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\attribsys\\CgsAttribSysModule.h", 230 /* ARTIST 0x828024EC */));
    return leOld;
}

// @ 0x827DD340 -- AttribSysGarbageCollector's scalar deleting destructor (MSVC's ??_G thunk).
// Defining the virtual destructor out-of-line here makes the compiler synthesise exactly that
// thunk: it runs the derived destructor (which the X360 folds to a direct bl on the base
// Attrib::IGarbageCollector::~IGarbageCollector), then -- when the low bit of the deleting
// flag is set -- calls operator delete(this) and returns this on both paths. The class owns
// only miTotalFreedSoFar (a plain s32) and holds no resources, so the destructor body is
// empty; the observable effect is the base-vptr install and the conditional free the
// synthesised thunk supplies. Mirrors the base sibling Attrib::IGarbageCollector
// (attribgarbagecollector.cpp @ 0x827DBA70).
AttribSysGarbageCollector::~AttribSysGarbageCollector()
{
}

// @ 0x827DD330 -- account the released byte count. AssetID occupies one native
// 64-bit GPR (r5); data is r6 and size is r7, which the ARTIST add consumes.
void AttribSysGarbageCollector::ReleaseData(u8 /*luType*/, Attrib::Vault::AssetID /*lAssetId*/,
                                            void* /*lpData*/, unsigned int luSize)
{
    miTotalFreedSoFar = static_cast<s32>(static_cast<u32>(miTotalFreedSoFar) + luSize);
}

// @ 0x8280AD50 -- one-time construction. Builds the base module, the GC, vault array and debug
// log window, clears the schema-vault pointer + the "schema loaded" flag, publishes the vault
// array into the static accessor, and marks this as a "new" (lock-free) module.
void AttribSysModule::Construct()
{
    _AssertLayout();
    ModuleSingleBuffered::Construct();

    mMemoryManager.Construct();

    mGarbageCollector.Construct();
    mpSchemaVault  = 0;
    mePrepareStage = E_PREPARESTAGE_START;
    meReleaseStage = E_RELEASESTAGE_DONE;

    mVaultArray.Construct(&mGarbageCollector);

    // ARTIST @0x8280ADA0 calls the debug component's (empty, ICF-folded) Construct on this+0x22C.
    mDebugComponent.Construct();

    sLogWindow.Construct(20);

    sbSchemaLoaded = false;
    spVaultArray   = &mVaultArray;

    // AttribSysModule is a "new" module: it manages its own IO rather than the base lock path.
    mbIsNewModule = true;
}

// @ 0x828025E8 -- hand back the live vault array (the static published by Construct).
const VaultArray* AttribSysModule::GetVaultArray()
{
    return spVaultArray;
}

// @ 0x8280FF50 -- the GameTalk callback registered on the "AttribSys.xenon" channel.
// The asm defensively re-compares the message's channel string (msg+0x10 == GetChannel())
// against "AttribSys.xenon" byte-for-byte; on a match it looks up the "Update" key and, if
// present, forwards the key content to Attrib::DecodeLiveLinkMessage. The Hex-Rays int
// return is the manager's fiction -- the registered handler is a void MessageHandler.
void AttribSysModule::AttribulatorGameTalkHandler(EA::GameTalk::GameTalkMessage* lpMessage)
{
    // beq/bne cr6 on the channel-string compare: bail unless this really is the AttribSys channel.
    if (strcmp(lpMessage->GetChannel(), "AttribSys.xenon") != 0)
    {
        return;
    }

    // bl sub_82837E08(msg, "Update") == GameTalkMessage::GetKeyContent("Update"); its result
    // (NULL when the key is absent) is the r3 forwarded straight into DecodeLiveLinkMessage.
    const char* lpcUpdate = lpMessage->GetKeyContent("Update");
    if (lpcUpdate != 0)
    {
        Attrib::DecodeLiveLinkMessage(lpcUpdate);
    }
}

// @ 0x82807620 -- resumable bring-up. Each stage records itself before doing its work so a
// partial Prepare resumes at the right point; success advances mePrepareStage to DONE.
bool AttribSysModule::Prepare(CgsMemory::LinearMalloc* lpLinearAlloc,
                              CgsMemory::HeapMalloc* lpAttribSysHeap,
                              CgsMemory::HeapMalloc* lpGameTalkHeap,
                              CgsMemory::HeapMalloc* lpEastlHeap,
                              s32 liMaxNumVaults)
{
    switch (mePrepareStage)
    {
        case E_PREPARESTAGE_START:
            mePrepareStage = E_PREPARESTAGE_START;
            mDebugComponent.Register();
            // fall through
        case E_PREPARESTAGE_MANAGER:
            mePrepareStage = E_PREPARESTAGE_MANAGER;
            if (!ModuleSingleBuffered::Prepare())
                return false;
            // fall through
        case E_PREPARESTAGE_MEMORY:
            mePrepareStage = E_PREPARESTAGE_MEMORY;

            CGS_ASSERT(lpLinearAlloc   != 0,
                CGS_ASSERT_MESSAGE_AT(
                    "lpLinearAlloc != NULL", CGS_ARTIST_FILE, 133 /* ARTIST 0x828076C8 */));
            CGS_ASSERT(lpAttribSysHeap != 0,
                CGS_ASSERT_MESSAGE_AT(
                    "lpAttribSysHeap != NULL", CGS_ARTIST_FILE, 134 /* ARTIST 0x828076EC */));
            CGS_ASSERT(lpGameTalkHeap  != 0,
                CGS_ASSERT_MESSAGE_AT(
                    "lpGameTalkHeap != NULL", CGS_ARTIST_FILE, 135 /* ARTIST 0x82807710 */));
            CGS_ASSERT(lpEastlHeap     != 0,
                CGS_ASSERT_MESSAGE_AT(
                    "lpEastlHeap != NULL", CGS_ARTIST_FILE, 136 /* ARTIST 0x82807734 */));
            CGS_ASSERT(liMaxNumVaults > 0,
                CGS_ASSERT_MESSAGE_AT(
                    "liMaxNumVaults > 0", CGS_ARTIST_FILE, 137 /* ARTIST 0x82807758 */));

            mMemoryManager.Prepare(lpLinearAlloc, lpAttribSysHeap,
                                            lpGameTalkHeap, lpEastlHeap);
            mVaultArray.Prepare(liMaxNumVaults, lpLinearAlloc);

            mpSchemaVault = static_cast<Attrib::Vault*>(
                lpLinearAlloc->Malloc(KI_SCHEMA_VAULT_BYTES));
            CGS_ASSERT(mpSchemaVault != 0,
                CGS_ASSERT_MESSAGE_AT(
                    "mpSchemaVault != NULL", CGS_ARTIST_FILE, 152 /* ARTIST 0x828077B4 */));

            mePrepareStage++;
            // fall through
        case E_PREPARESTAGE_DONE:
            meReleaseStage = E_RELEASESTAGE_START;
            mePrepareStage = E_PREPARESTAGE_DONE;
            return true;
        default:
            CGS_ASSERT(false,
                CGS_ASSERT_MESSAGE_AT(
                    "Invalid prepare state.\n", CGS_ARTIST_FILE, 169 /* ARTIST 0x828077F4 */));
            return false;
    }
}

// @ 0x82802520 -- resumable tear-down: the mirror of Prepare's staging.
bool AttribSysModule::Release()
{
    switch (meReleaseStage)
    {
        case E_RELEASESTAGE_START:
            meReleaseStage = E_RELEASESTAGE_START;
            // fall through
        case E_RELEASESTAGE_MEMORY:
            meReleaseStage = E_RELEASESTAGE_MEMORY;
            // fall through
        case E_RELEASESTAGE_MANAGER:
            meReleaseStage = E_RELEASESTAGE_MANAGER;
            if (!ModuleSingleBuffered::Release())
                return false;
            // fall through
        case E_RELEASESTAGE_DONE:
            mePrepareStage = E_PREPARESTAGE_START;
            meReleaseStage = E_RELEASESTAGE_DONE;
            return true;
        default:
            CGS_ASSERT(false,
                CGS_ASSERT_MESSAGE_AT(
                    "Invalid release state.\n", CGS_ARTIST_FILE, 226 /* ARTIST 0x828025B4 */));
            return false;
    }
}

// @ 0x82804380 -- tear down the base module and debug component.
void AttribSysModule::Destruct()
{
    ModuleSingleBuffered::Destruct();
    // ARTIST @0x82804398 then calls the debug component's (empty, ICF-folded) Destruct on this+0x22C.
    mDebugComponent.Destruct();
}















































// @ 0x8280FC60 -- per-frame entry: drain the queued requests, then (if a schema is loaded)
// run an attribute-database garbage-collect pass.
void AttribSysModule::Update(const AttribSysIO::InputBuffer* lpInputBuffer)
{
    CGS_ASSERT(lpInputBuffer != 0, "lpInputBuffer != NULL");

    ProcessInputs(lpInputBuffer);

    if (sbSchemaLoaded)
    {
        Attrib::Database::Get().CollectGarbage();
    }
}















// @ 0x8280FB60 -- drain every queued request from the read-locked input buffer, dispatching on
// the event type id (0 RegisterVault, 1 RegisterSchema, 2 UnregisterVault).
void AttribSysModule::ProcessInputs(const AttribSysIO::InputBuffer* lpInputBuffer)
{
    CGS_ASSERT_AT(lpInputBuffer != 0, "lpInputBuffer != NULL", CGS_ARTIST_FILE, 299);

    lpInputBuffer->LockForRead();

    int32_t liEventId;
    int32_t liEventSize;
    const CgsModule::Event* lpEvent;
    // The queue is fetched through AttribSysIO::Inp (ARTIST 0x828042D8) before each walk step; DecFIGS
    // fetches it through InputBuffer::GetVaultRequestInterface instead, so its local list (the three
    // above, cpp:303-305) does not settle this pointer. ARTIST keeps &lpEvent / &liEventSize as
    // stack addresses at each call (addi r4/r5 @0x8280FBB0, 0x8280FC34) and r27-r31 only.
    const AttribSysIO::AttribSysEventQueue* lpQueue = AttribSysIO::Inp(lpInputBuffer);
    liEventId = lpQueue->GetFirstEvent(&lpEvent, &liEventSize);
    while (liEventId >= 0)
    {
        switch (liEventId)
        {
            case 0:
                RegisterVault(reinterpret_cast<AttribSysIO::RegisterVaultRequest*>(
                    const_cast<CgsModule::Event*>(lpEvent)));
                break;
            case 1:
                RegisterSchema(reinterpret_cast<AttribSysIO::RegisterSchemaRequest*>(
                    const_cast<CgsModule::Event*>(lpEvent)));
                break;
            case 2:
                UnregisterVault(reinterpret_cast<AttribSysIO::UnregisterVaultRequest*>(
                    const_cast<CgsModule::Event*>(lpEvent)));
                break;
            default:





                CGS_ASSERT_AT(false, "Unrecognised AttribSys event type\n", CGS_ARTIST_FILE, 334);
                break;
        }

        lpQueue   = AttribSysIO::Inp(lpInputBuffer);
        liEventId = lpQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize);
    }

    lpInputBuffer->UnlockForRead();
}










// @ 0x8280EBF0 -- register a data vault (absent from the .ida-exports; recovered via
// headless IDA over the ARTIST .i64): assert the schema is live, hand the request to the
// vault array (slot ref-count / DoLoad), then acknowledge the requester -- the type-3
// reply's 4-byte payload is the request's miEventId (GameData passes its event-slot index
// there and ProcessAttribSysRegisterVaultResponse @0x82666590 looks the slot back up with
// it; the world passes 1 and only tests queue length).
void AttribSysModule::RegisterVault(AttribSysIO::RegisterVaultRequest* lpRegisterVaultRequest)
{
    CGS_ASSERT_AT(IsSchemaLoaded(), "IsSchemaLoaded()", CGS_ARTIST_FILE, 362);
    mVaultArray.RegisterVault(lpRegisterVaultRequest);

    const s32 liEventId = lpRegisterVaultRequest->miEventId;
    lpRegisterVaultRequest->mpUserReceiverQueue->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&liEventId),
        3, static_cast<s32>(sizeof(liEventId)));
}




















// @ 0x8280E4A0 -- load the (single) schema: construct the schema vault over the request's
// .vlt image, resolve its dependency blob, initialise it into the database, then notify the
// requester. Only one schema may be loaded at a time. The DWARF locals (DecFIGS unity CU,
// CgsAttribSysModule.cpp:400-405, 435) are read up front in ARTIST's order (@0x8280E4E8..0x8280E4FC:
// vlt size, bin size, vlt data, bin data); the placement new's null test is the compiler's.
void AttribSysModule::RegisterSchema(AttribSysIO::RegisterSchemaRequest* lpRegisterSchemaRequest)
{
    CGS_ASSERT_AT(!IsSchemaLoaded(), "!IsSchemaLoaded()", CGS_ARTIST_FILE, 397);


    int32_t liVltSize = lpRegisterSchemaRequest->miSchemaVltDataSize;
    int32_t liBinSize = lpRegisterSchemaRequest->miSchemaBinDataSize;
    void*   lpVltData = lpRegisterSchemaRequest->mpSchemaVltData;
    void*   lpBinData = lpRegisterSchemaRequest->mpSchemaBinData;

    Attrib::Vault* lpNewVault = new (mpSchemaVault) Attrib::Vault(Attrib::Database::GetExportPolicies(),
                                                                  /*lAssetId*/ 0,
                                                                  lpVltData,
                                                                  static_cast<unsigned int>(liVltSize),
                                                                  /*lbType*/ 1,
                                                                  &mGarbageCollector);






    CGS_ASSERT_AT(lpNewVault == mpSchemaVault, "lpNewVault == mpSchemaVault", CGS_ARTIST_FILE, 417);

    if (lpNewVault->HasUnresolvedDependency())
    {
        lpNewVault->ResolveDependency(/*luIndex*/ 0, lpBinData, static_cast<unsigned int>(liBinSize),
                                      /*lbIsAsset*/ 1);
    }




    CGS_ASSERT_AT(!lpNewVault->HasUnresolvedDependency(), "!lpNewVault->HasUnresolvedDependency()", CGS_ARTIST_FILE, 428);

    lpNewVault->Initialize();




    AttribSysIO::SchemaRegisteredResponse lResponse;
    lpRegisterSchemaRequest->mpUserReceiverQueue->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lResponse), 4, 1);

    sbSchemaLoaded = true;
}















// @ 0x8280F9D0 -- unregister a data vault: assert the schema is live, drop the array's
// reference (slot unload at refcount 0), then acknowledge with a type-5 reply whose
// 4-byte payload is the request's miEventId (ProcessUnregisterVehicleAttribsResponse
// @0x8266EAA0 uses it as the GameData event-slot index).
void AttribSysModule::UnregisterVault(AttribSysIO::UnregisterVaultRequest* lpUnregisterVaultRequest)
{
    CGS_ASSERT_AT(IsSchemaLoaded(), "IsSchemaLoaded()", CGS_ARTIST_FILE, 462);
    CGS_ASSERT_AT(lpUnregisterVaultRequest != 0, "lpUnregisterVaultRequest != NULL", CGS_ARTIST_FILE, 463);
    mVaultArray.UnregisterVault(lpUnregisterVaultRequest);

    const s32 liEventId = lpUnregisterVaultRequest->miEventId;
    lpUnregisterVaultRequest->mpUserReceiverQueue->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&liEventId),
        5, static_cast<s32>(sizeof(liEventId)));
}
}

#undef CGS_ARTIST_FILE
