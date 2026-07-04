#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysDebugComponent.h"

// CgsAttribSys::AttribSysDebugComponent member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. GetName/GetPath return TU-local const char* rodata pointers
// (off_82F30E88 / off_82F30E8C), homed here as KPAC_DEBUG_COMPONENT_NAME / KPAC_DEBUG_COMPONENT_PATH.

namespace CgsAttribSys
{
    const char* KPAC_DEBUG_COMPONENT_NAME = "AttribSys";
    const char* KPAC_DEBUG_COMPONENT_PATH = "Core";

    // X360 0x827DB668 -- lwz r3, off_82F30E88 -> "AttribSys".
    const char* AttribSysDebugComponent::GetName() const
    {
        return KPAC_DEBUG_COMPONENT_NAME;
    }

    // X360 0x827DB678 -- lwz r3, off_82F30E8C -> "Core".
    const char* AttribSysDebugComponent::GetPath() const
    {
        return KPAC_DEBUG_COMPONENT_PATH;
    }
}
