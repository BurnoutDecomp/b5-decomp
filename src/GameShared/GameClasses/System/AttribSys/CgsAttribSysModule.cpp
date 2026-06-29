#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysModuleIO.h" // AttribSysIO::InputBuffer + requests + Inp
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"          // LinearMalloc::Malloc (schema vault)
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Windows/CgsLogWindow.h" // the debug log window
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/attribsys.h"   // Attrib::Database

namespace CgsAttribSys
{
// ---- file-scope statics (CgsAttribSysModule.cpp:31/34) ----------------------------------
// sbSchemaLoaded enforces "only one schema loaded at a time"; spVaultArray is published by
// Construct (X360 dword_83011BB4) and handed back by GetVaultArray.
bool        AttribSysModule::sbSchemaLoaded = false;
VaultArray* AttribSysModule::spVaultArray   = nullptr;

namespace
{
    // The module's debug log window (X360 off_82F31050) and its running line counter
    // (X360 dword_83011BAC), constructed in AttribSysModule::Construct. Pure debug plumbing.
    CgsDev::DebugUI::LogWindow sLogWindow;
    s32                        siLogLineCount = 0;

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
               "leEnumIndex <= AttribSysModule::E_PREPARESTAGE_DONE");
    return leOld;
}

// The GC callback body lives in the garbage-collector's own TU; trap here so the vtable links.
void AttribSysGarbageCollector::ReleaseData(u8 /*luType*/, Attribute::HashInt /*lAssetId*/,
                                            void* /*lpData*/, unsigned int /*luSize*/)
{
    CGS_ASSERT(false, "AttribSysGarbageCollector::ReleaseData (deferred TU)");
}

// @ 0x8280AD50 -- one-time construction. Builds the base module, the GC, vault array and debug
// log window, clears the schema-vault pointer + the "schema loaded" flag, publishes the vault
// array into the static accessor, and marks this as a "new" (lock-free) module.
void AttribSysModule::Construct()
{
    ModuleSingleBuffered::Construct();

    // Reset the per-frame memory-manager flag (sbHasLinearAllocator) and the debug line count.
    AttribSysMemoryManager::Construct();
    siLogLineCount = 0;

    mGarbageCollector.Construct();
    mpSchemaVault  = nullptr;
    mePrepareStage = E_PREPARESTAGE_START;
    meReleaseStage = E_RELEASESTAGE_DONE;

    mVaultArray.Construct(&mGarbageCollector);

    // mDebugComponent's construction (X360 the no-op call on this+0x22C, which Hex-Rays
    // misnames BaseCollisionGenerator::Destruct) folds to an empty body.

    sLogWindow.Construct(20);

    sbSchemaLoaded = false;
    spVaultArray   = &mVaultArray;

    // AttribSysModule is a "new" module: it manages its own IO rather than the base lock path.
    mbIsNewModule = true;
}

// @ 0x828025E8 -- hand back the live vault array (the static published by Construct).
const VaultArray* AttribSysModule::GetVaultArray() const
{
    return spVaultArray;
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

            CGS_ASSERT(lpLinearAlloc   != nullptr, "lpLinearAlloc != NULL");
            CGS_ASSERT(lpAttribSysHeap != nullptr, "lpAttribSysHeap != NULL");
            CGS_ASSERT(lpGameTalkHeap  != nullptr, "lpGameTalkHeap != NULL");
            CGS_ASSERT(lpEastlHeap     != nullptr, "lpEastlHeap != NULL");
            CGS_ASSERT(liMaxNumVaults > 0,         "liMaxNumVaults > 0");

            AttribSysMemoryManager::Prepare(lpLinearAlloc, lpAttribSysHeap,
                                            lpGameTalkHeap, lpEastlHeap);
            mVaultArray.Prepare(liMaxNumVaults, lpLinearAlloc);

            mpSchemaVault = static_cast<Attrib::Vault*>(
                lpLinearAlloc->Malloc(KI_SCHEMA_VAULT_BYTES));
            CGS_ASSERT(mpSchemaVault != nullptr, "mpSchemaVault != NULL");

            mePrepareStage++;
            // fall through
        case E_PREPARESTAGE_DONE:
            meReleaseStage = E_RELEASESTAGE_START;
            mePrepareStage = E_PREPARESTAGE_DONE;
            return true;
        default:
            CGS_ASSERT(false, "Invalid prepare state.\n");
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
            CGS_ASSERT(false, "Invalid release state.\n");
            return false;
    }
}

// @ 0x82804380 -- tear down the base module then the garbage collector.
void AttribSysModule::Destruct()
{
    ModuleSingleBuffered::Destruct();
    // mDebugComponent's destruction (X360 the no-op call on this+0x22C, which Hex-Rays
    // misnames BaseCollisionGenerator::Destruct) folds to an empty body.
}

// @ 0x8280FC60 -- per-frame entry: drain the queued requests, then (if a schema is loaded)
// run an attribute-database garbage-collect pass.
void AttribSysModule::Update(const AttribSysIO::InputBuffer* lpInputBuffer)
{
    CGS_ASSERT(lpInputBuffer != nullptr, "lpInputBuffer != NULL");

    ProcessInputs(lpInputBuffer);

    if (sbSchemaLoaded)
    {
        CGS_ASSERT(Attrib::Database::IsInitialized(), "Attribute database not initialized.");
        Attrib::Database::Get().CollectGarbage();
    }
}

// @ 0x8280FB60 -- drain every queued request from the read-locked input buffer, dispatching on
// the event type id (0 RegisterVault, 1 RegisterSchema, 2 UnregisterVault).
void AttribSysModule::ProcessInputs(const AttribSysIO::InputBuffer* lpInputBuffer)
{
    CGS_ASSERT(lpInputBuffer != nullptr, "lpInputBuffer != NULL");

    lpInputBuffer->LockForRead();

    const AttribSysIO::AttribSysEventQueue* lpQueue = AttribSysIO::Inp(lpInputBuffer);

    const CgsModule::Event* lpEvent = nullptr;
    s32 liSize = 0;
    for (s32 liEventType = lpQueue->GetFirstEvent(&lpEvent, &liSize);
         liEventType >= 0;
         liEventType = lpQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
    {
        switch (liEventType)
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
                CGS_ASSERT(false, "Unrecognised AttribSys event type\n");
                break;
        }

        lpQueue = AttribSysIO::Inp(lpInputBuffer);
    }

    lpInputBuffer->UnlockForRead();
}

// @ 0x8280E4A0 -- load the (single) schema: construct the schema vault over the request's
// .vlt image, resolve its dependency blob, initialise it into the database, then notify the
// requester. Only one schema may be loaded at a time.
void AttribSysModule::RegisterSchema(AttribSysIO::RegisterSchemaRequest* lpRegisterSchemaRequest)
{
    CGS_ASSERT(!IsSchemaLoaded(), "!IsSchemaLoaded()");

    Attrib::Vault* lpNewVault = nullptr;
    if (mpSchemaVault != nullptr)
    {
        // Placement-construct the schema vault over the request's serialised image. (The X360
        // build passes the schema-vault storage in as the Vault ctor's `this`.)
        Attrib::ExportManager& lExportPolicies = Attrib::Database::Get().GetExportPolicies();
        lpNewVault = new (mpSchemaVault) Attrib::Vault(
            lExportPolicies,
            /*lAssetId*/ 0,
            lpRegisterSchemaRequest->mpSchemaVltData,
            static_cast<unsigned int>(lpRegisterSchemaRequest->miSchemaVltDataSize),
            /*lbType*/ 1,
            &mGarbageCollector);
    }

    CGS_ASSERT(lpNewVault == mpSchemaVault, "lpNewVault == mpSchemaVault");

    if (lpNewVault->HasUnresolvedDependency())
    {
        lpNewVault->ResolveDependency(
            /*luIndex*/ 0,
            lpRegisterSchemaRequest->mpSchemaBinData,
            static_cast<unsigned int>(lpRegisterSchemaRequest->miSchemaBinDataSize),
            /*lbIsAsset*/ 1);
    }
    CGS_ASSERT(!lpNewVault->HasUnresolvedDependency(), "!lpNewVault->HasUnresolvedDependency()");

    lpNewVault->Initialize();

    // Acknowledge the load back to the requester (event type 4, payload size 1).
    AttribSysIO::SchemaRegisteredResponse lResponse;
    lpRegisterSchemaRequest->mpUserReceiverQueue->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lResponse), 4, 1);

    sbSchemaLoaded = true;
}
}
