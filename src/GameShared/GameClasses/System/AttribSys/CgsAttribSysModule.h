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
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribloadandgo.h"

namespace CgsMemory
{
class LinearMalloc;
class HeapMalloc;
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

// The AttribSys module's own debug overlay (registered with the debug menu in Prepare).
// The X360 build only ever reaches the inherited DebugComponent::Register on it, so the
// recon is a minimal named subclass; its registered variables live in its own TU.
class AttribSysDebugComponent : public CgsDev::DebugComponent
{
};

// The garbage-collector callback AttribSys invokes when it reclaims a vault. The X360
// ledger attests only its construction (folded to an empty body) and its presence in the
// vault array; ReleaseData's body lives in the GC's own TU. (DWARF CgsAttribSysModule.h:75.)
class AttribSysGarbageCollector : public Attrib::IGarbageCollector
{
public:
    void Construct() { miTotalFreedSoFar = 0; }

    void ReleaseData(u8 luType, Attribute::HashInt lAssetId,
                     void* lpData, unsigned int luSize) override;

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
    void Construct() override;

    // Resumable bring-up: base module, then the AttribSys memory managers, vault array and
    // a freshly-allocated schema vault. liMaxNumVaults > 0. X360 0x82807620.
    bool Prepare(CgsMemory::LinearMalloc* lpLinearAlloc,
                 CgsMemory::HeapMalloc* lpAttribSysHeap,
                 CgsMemory::HeapMalloc* lpGameTalkHeap,
                 CgsMemory::HeapMalloc* lpEastlHeap,
                 s32 liMaxNumVaults);

    bool Release() override;                     // X360 0x82802520
    void Destruct() override;                    // X360 0x82804380
    void Update(const AttribSysIO::InputBuffer* lpInputBuffer);   // X360 0x8280FC60

    // Drain every queued vault/schema request from the read-locked input buffer. X360 0x8280FB60.
    void ProcessInputs(const AttribSysIO::InputBuffer* lpInputBuffer);

    bool IsSchemaLoaded() const { return sbSchemaLoaded; }

    // X360 0x828025E8 -- the live vault array (returned via the module's static pointer).
    const VaultArray* GetVaultArray() const;

private:
    // X360 0x8280EBF0 (RegisterVault) / 0x8280E4A0 (RegisterSchema) / 0x8280F9D0 (UnregisterVault).
    void RegisterVault(AttribSysIO::RegisterVaultRequest* lpRegisterVaultRequest);
    void RegisterSchema(AttribSysIO::RegisterSchemaRequest* lpRegisterSchemaRequest);   // X360 0x8280E4A0
    void UnregisterVault(AttribSysIO::UnregisterVaultRequest* lpUnregisterVaultRequest);

    // --- members (DWARF order; X360 offsets 0x228.. ) ---
    AttribSysMemoryManager    mMemoryManager;     // X360 +0x228 (all-static front-end; see .h)
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
