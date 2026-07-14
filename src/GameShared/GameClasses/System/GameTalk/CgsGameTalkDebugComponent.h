#ifndef GAMESHARED_GAMECLASSES_SYSTEM_GAMETALK_CGSGAMETALKDEBUGCOMPONENT_H
#define GAMESHARED_GAMECLASSES_SYSTEM_GAMETALK_CGSGAMETALKDEBUGCOMPONENT_H

#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"

// CgsGameTalk::GameTalkDebugComponent -- the GameTalk system's in-game debug component
// (name "GameTalk", path "Core"). Derives from CgsDev::DebugComponent. GetName @ 0x827DB690
// and GetPath @ 0x827DB6A0 each do an indirect lwz through a const char* rodata pointer
// (off_82F30E90 -> "GameTalk", off_82F30E94 -> "Core"); those pointers are homed as the
// KAC_DEBUG_COMPONENT_NAME / KAC_DEBUG_COMPONENT_PATH translation-unit globals in CgsGameTalk.cpp.
// OnActivate @ 0x82836C20 chains the (empty) base activation, then sizes the shared "Core/GameTalk"
// log window (off_82F32FD4 == sLogWindow, brought up by GameTalk::Construct) to 250x60.
//
// In the X360 build GameTalk::mDebugComponent is a GameTalkDebugComponent; the committed
// CgsGameTalk.h types that member as the CgsDev::DebugComponent base (same 12-byte layout --
// this subclass adds only vtable overrides, no data members).

namespace CgsGameTalk
{
    class GameTalkDebugComponent : public CgsDev::DebugComponent
    {
    protected:
        void OnActivate() override;
        const char* GetName() const override;
        const char* GetPath() const override;
    };
}

#endif // GAMESHARED_GAMECLASSES_SYSTEM_GAMETALK_CGSGAMETALKDEBUGCOMPONENT_H
