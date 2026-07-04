#ifndef CGS_ATTRIB_SYS_DEBUG_COMPONENT_H
#define CGS_ATTRIB_SYS_DEBUG_COMPONENT_H

#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"

// CgsAttribSys::AttribSysDebugComponent -- the attribute-system's in-game debug component
// (name "AttribSys", path "Core"). Derives from CgsDev::DebugComponent. GetName @ 0x827DB668
// and GetPath @ 0x827DB678 each do an indirect lwz through a const char* rodata pointer
// (off_82F30E88 -> "AttribSys", off_82F30E8C -> "Core"); those pointers are homed as the
// KPAC_DEBUG_COMPONENT_NAME / KPAC_DEBUG_COMPONENT_PATH translation-unit globals in the .cpp.

namespace CgsAttribSys
{
    class AttribSysDebugComponent : public CgsDev::DebugComponent
    {
    protected:
        const char* GetName() const override;
        const char* GetPath() const override;
    };
}

#endif
