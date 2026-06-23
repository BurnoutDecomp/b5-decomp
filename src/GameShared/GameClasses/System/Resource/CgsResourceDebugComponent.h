#ifndef CGS_RESOURCE_DEBUG_COMPONENT_H
#define CGS_RESOURCE_DEBUG_COMPONENT_H

#include "DebugSystem/Core/CgsDebugComponent.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceDebugPoolTypeList.h"  // embedded mPoolTypeList

namespace CgsResource
{
// The resource subsystem's in-game debug component (path "Core"). It derives from
// CgsDev::DebugComponent and embeds the per-pool-type resource-accounting table (DebugPoolTypeList)
// plus the resource debug sub-components the constructor brings up. Recovered from the DecFIGS
// DWARF and the X360 spine; GetPath @ 0x827E0988 (its own reviewed TU) returns "Core", the
// constructor @ 0x827E08D0 is the TU homed in CgsResourceDebugComponentCtor.cpp.
class DebugComponent : public CgsDev::DebugComponent
{
public:
    // X360 0x827E08D0. Brings the resource debug component up: installs the component vtables and
    // constructs the embedded DebugPoolTypeList accounting table. (PARTIAL: see the .cpp -- the
    // vtable installs of the unmodeled embedded debug sub-components, and their {0,1}-seeded
    // running-total pairs, are documented but not store-reproduced because those sub-objects'
    // layouts are not modeled.)
    DebugComponent();

    virtual const char* GetPath() const { return "Core"; }

private:
    // Embedded per-pool-type resource-accounting table (X360 constructs it at +0x480 inside the
    // component via DebugPoolTypeList::DebugPoolTypeList(a1 + 0x480)).
    DebugPoolTypeList mPoolTypeList;
};
}

#endif
