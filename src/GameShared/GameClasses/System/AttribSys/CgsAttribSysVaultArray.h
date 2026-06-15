#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysVaultAllocator.h"

namespace Attrib { class IGarbageCollector; }

namespace CgsAttribSys
{
struct VaultSlot;   // pointer-only here; reconstructed in its own TU

// Layout from the DecFIGS DWARF (CgsAttribSysVaultArray.h). Only Construct is
// declared (the only function this TU owns); Prepare and the accessors are added
// when their TUs are reconstructed.
class VaultArray
{
public:
    void Construct(Attrib::IGarbageCollector* lpGarbageCollector);

private:
    VaultSlot*                 mpaSlots;
    Attrib::IGarbageCollector* mpGarbageCollector;
    StreamedVaultAllocator     mVaultAllocator;
    s32                        miNumSlots;
    bool                       mbPrepared;
};
}
