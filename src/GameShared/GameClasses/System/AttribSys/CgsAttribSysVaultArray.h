#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysVaultAllocator.h"

namespace Attrib { struct IGarbageCollector; }   // struct -- must match attribloadandgo.h's class-key (MSVC mangling)
namespace CgsMemory { class LinearMalloc; }
namespace CgsDev { struct StrStreamBase; }
namespace CgsAttribSys { namespace AttribSysIO { struct RegisterVaultRequest;
                                                 struct UnregisterVaultRequest; } }

namespace CgsAttribSys
{
struct VaultArray;
struct VaultSlot;   // pointer-only here; reconstructed in its own TU

// @ 0x82803888 -- debug dump of the whole vault array (header + per-slot line describing
// empty/occupied, the 64-bit resource id, the ref count, and the streamed-vault state). A
// namespace-scope free operator; declared friend of VaultArray so it can read the private
// mpaSlots/miNumSlots. Body in CgsAttribSysVaultArray.cpp.
CgsDev::StrStreamBase& operator<<(CgsDev::StrStreamBase& lStream, const VaultArray& lVaultArray);

// Layout from the DecFIGS DWARF (CgsAttribSysVaultArray.h). Construct, Prepare and
// UnregisterVault are declared here (the slice AttribSysModule needs); the remaining
// accessors are added when their TUs are reconstructed.
class VaultArray
{
public:
    void Construct(Attrib::IGarbageCollector* lpGarbageCollector);

    // Reserve liMaxNumVaults slots out of the linear allocator and ready the streamed
    // vault allocator (called from AttribSysModule::Prepare).
    void Prepare(s32 liMaxNumVaults, CgsMemory::LinearMalloc* lpLinearAllocator);

    // @ 0x8280E978 -- register the request's vault into a slot (ref-count bump or
    // VaultSlot::DoLoad; called from AttribSysModule::RegisterVault). Body deferred with
    // the vault-load interior (GetFreeSlotIndex + the Attrib SDK runtime); the PC
    // AttribSysModule gates the call while the schema is unloaded (link stub trap).
    void RegisterVault(AttribSysIO::RegisterVaultRequest* lpRegisterVaultRequest);

    // Drop the vault named by the request from the array (called from
    // AttribSysModule::UnregisterVault / ProcessInputs).
    void UnregisterVault(AttribSysIO::UnregisterVaultRequest* lpUnregisterVaultRequest);

    // The debug dump reads the private mpaSlots/miNumSlots (@ 0x82803888).
    friend CgsDev::StrStreamBase& operator<<(CgsDev::StrStreamBase& lStream,
                                             const VaultArray& lVaultArray);

private:
    VaultSlot*                 mpaSlots;
    Attrib::IGarbageCollector* mpGarbageCollector;
    StreamedVaultAllocator     mVaultAllocator;
    s32                        miNumSlots;
    bool                       mbPrepared;
};
}
