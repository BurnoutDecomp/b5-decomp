#pragma once

// CgsAttribSys::AttribSysModule -- the engine module that owns the AttribSys attribute
// database. It is a CgsModule::ModuleSingleBuffered: Prepare() brings the AttribSys
// memory managers, vault array and schema vault up (resumable, staged), Update()/
// ProcessInputs() drain the queued vault/schema requests each frame, and Release()/
// Destruct() tear it all down. Only ONE schema may be loaded at a time
// (sbSchemaLoaded).
//
// Class shape recovered from the DecFIGS DWARF (CgsAttribSysModule.h), gated on the
// X360 ARTIST ledger; member placement + the staged state machines confirmed against
// the X360 asm (Construct 0x8280AD50, Prepare 0x82807620, Release 0x82802520,
// Update 0x8280FC60, ProcessInputs 0x8280FB60, RegisterSchema 0x8280E4A0,
// Destruct 0x82804380, GetVaultArray 0x828025E8).

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysMemoryManager.h"
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysVaultArray.h"
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysDebugComponent.h"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribloadandgo.h"

namespace CgsMemory
{
class LinearMalloc;
class HeapMalloc;
}

namespace EA
{
namespace GameTalk
{
    class GameTalkMessage;
}
}

namespace CgsAttribSys
{
namespace AttribSysIO
{
    struct InputBuffer;
    struct RegisterVaultRequest;
    struct RegisterSchemaRequest;
    struct UnregisterVaultRequest;
}

// The garbage-collector callback AttribSys invokes when it reclaims a vault. Concrete host
// of Attrib::IGarbageCollector: its construction folds to an empty body, ReleaseData's body
// lives in this TU (deferred trap), and its scalar deleting destructor thunk is attested at
// X360 0x827DD340. (DWARF CgsAttribSysModule.h:75.)
//
// The destructor is declared out-of-line (defined in this TU's .cpp) so MSVC synthesises the
// derived-class ??_G scalar-deleting-destructor at 0x827DD340 -- MSVC only emits that thunk
// (and pins this class's vtable to one TU) when the virtual destructor is NOT inline. This
// mirrors the base sibling home Attrib::IGarbageCollector (attribgarbagecollector.cpp
// @ 0x827DBA70).
class AttribSysGarbageCollector : public Attrib::IGarbageCollector
{
public:
    void Construct() { miTotalFreedSoFar = 0; }

    // Out-of-line so MSVC emits the ??_G scalar deleting destructor @ X360 0x827DD340.
    ~AttribSysGarbageCollector() ;

    // X360 0x827DD330 accumulates r7, the released block size. The native ABI
    // passes the 64-bit AssetID in r5, the data pointer in r6 and the size in r7.
    void ReleaseData(u8 luType, Attrib::Vault::AssetID lAssetId,
                     void* lpData, unsigned int luSize) ;

private:
    s32 miTotalFreedSoFar;   // CgsAttribSysModule.h:99
};

class AttribSysModule : public CgsModule::ModuleSingleBuffered
{
public:
    // Bring-up state machine (CgsAttribSysModule.h:115). operator++ steps it, asserting it
    // never runs past E_PREPARESTAGE_DONE.
    enum EPrepareStage
    {
        E_PREPARESTAGE_START   = 0,
        E_PREPARESTAGE_MEMORY  = 1,
        E_PREPARESTAGE_MANAGER = 2,
        E_PREPARESTAGE_DONE    = 3,
    };

    // Tear-down state machine (CgsAttribSysModule.h:123).
    enum EReleaseStage
    {
        E_RELEASESTAGE_START   = 0,
        E_RELEASESTAGE_MANAGER = 1,
        E_RELEASESTAGE_MEMORY  = 2,
        E_RELEASESTAGE_DONE    = 3,
    };

    // --- module lifecycle (AttribSysModule's own virtual interface) ---
    void Construct() ;

    // Resumable bring-up: base module, then the AttribSys memory managers, vault array and
    // a freshly-allocated schema vault. liMaxNumVaults > 0. X360 0x82807620.
    virtual bool Prepare(CgsMemory::LinearMalloc* lpLinearAlloc,
                 CgsMemory::HeapMalloc* lpAttribSysHeap,
                 CgsMemory::HeapMalloc* lpGameTalkHeap,
                 CgsMemory::HeapMalloc* lpEastlHeap,
                 s32 liMaxNumVaults);

    bool Release() ;                     // X360 0x82802520
    void Destruct() ;                    // X360 0x82804380
    virtual void Update(const AttribSysIO::InputBuffer* lpInputBuffer);   // X360 0x8280FC60

    // Drain every queued vault/schema request from the read-locked input buffer. X360 0x8280FB60.
    void ProcessInputs(const AttribSysIO::InputBuffer* lpInputBuffer);

    bool IsSchemaLoaded() { return sbSchemaLoaded; }

    // X360 0x828025E8 -- the live vault array (returned via the module's static pointer).
    const VaultArray* GetVaultArray();

    // X360 0x8280FF50 -- the GameTalk message handler registered on the "AttribSys.xenon"
    // channel by CgsGameTalk::Prepare. A plain (context-free) MessageHandler, hence static:
    // if the inbound message really is on the AttribSys channel and carries an "Update" key,
    // its content is decoded as an AttribSys LiveLink edit stream. Mirrors the sibling
    // CgsDev::DebugGameTalkInterface::GameTalkMsgHandler.
    static void AttribulatorGameTalkHandler(EA::GameTalk::GameTalkMessage* lpMessage);

private:
    // X360 0x8280EBF0 (RegisterVault) / 0x8280E4A0 (RegisterSchema) / 0x8280F9D0 (UnregisterVault).
    void RegisterVault(AttribSysIO::RegisterVaultRequest* lpRegisterVaultRequest);
    void RegisterSchema(AttribSysIO::RegisterSchemaRequest* lpRegisterSchemaRequest);   // X360 0x8280E4A0
    void UnregisterVault(AttribSysIO::UnregisterVaultRequest* lpUnregisterVaultRequest);

    static void _AssertLayout()
    {
        CGS_STATIC_ASSERT((sizeof(CgsModule::ModuleSingleBuffered) == 0x228), "native single-buffered module base");
        CGS_STATIC_ASSERT((offsetof(AttribSysModule, mMemoryManager) == 0x228), "native memory manager");
        CGS_STATIC_ASSERT((offsetof(AttribSysModule, mDebugComponent) == 0x22C), "native debug component");
        CGS_STATIC_ASSERT((offsetof(AttribSysModule, mGarbageCollector) == 0x238), "native garbage collector");
        CGS_STATIC_ASSERT((offsetof(AttribSysModule, mpSchemaVault) == 0x240), "native schema vault pointer");
        CGS_STATIC_ASSERT((offsetof(AttribSysModule, mVaultArray) == 0x248), "native vault array");
        CGS_STATIC_ASSERT((offsetof(AttribSysModule, mePrepareStage) == 0x268), "native prepare stage");
        CGS_STATIC_ASSERT((offsetof(AttribSysModule, meReleaseStage) == 0x26C), "native release stage");
        CGS_STATIC_ASSERT((sizeof(Attrib::Vault) == 0x58), "native schema vault allocation");
        CGS_STATIC_ASSERT((sizeof(AttribSysModule) == 0x270), "native AttribSys module");
    }

    // --- members (DWARF order; X360 offsets 0x228.. ) ---
    AttribSysMemoryManager    mMemoryManager;     // X360 +0x228 (native lifecycle receiver)
    AttribSysDebugComponent   mDebugComponent;    // X360 +0x22C
    AttribSysGarbageCollector mGarbageCollector;  // X360 +0x238
    Attrib::Vault*            mpSchemaVault;       // X360 +0x240 (the one loaded schema vault)
    VaultArray                mVaultArray;         // X360 +0x248
    EPrepareStage             mePrepareStage;      // X360 +0x268
    EReleaseStage             meReleaseStage;      // X360 +0x26C

    // File-scope statics (CgsAttribSysModule.cpp:31/34). sbSchemaLoaded gates "only one
    // schema at a time"; spVaultArray is published by Construct and returned by GetVaultArray.
    static bool        sbSchemaLoaded;
    static VaultArray* spVaultArray;
};

// CgsAttribSysModule.h:230 -- post-increment for the prepare-stage enum, asserting it never
// steps past E_PREPARESTAGE_DONE. X360 0x828024C0.
AttribSysModule::EPrepareStage operator++(AttribSysModule::EPrepareStage& leStage, int);
}
