#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysVaultAllocator.h"

namespace Attrib { class IGarbageCollector; }
namespace CgsMemory { class LinearMalloc; }
namespace CgsAttribSys { namespace AttribSysIO { struct UnregisterVaultRequest; } }

namespace CgsAttribSys
{
struct VaultSlot;   // pointer-only here; reconstructed in its own TU

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

    // Drop the vault named by the request from the array (called from
    // AttribSysModule::UnregisterVault / ProcessInputs).
    void UnregisterVault(AttribSysIO::UnregisterVaultRequest* lpUnregisterVaultRequest);

private:
    VaultSlot*                 mpaSlots;
    Attrib::IGarbageCollector* mpGarbageCollector;
    StreamedVaultAllocator     mVaultAllocator;
    s32                        miNumSlots;
    bool                       mbPrepared;
};
}
