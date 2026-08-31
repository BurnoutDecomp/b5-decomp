#pragma once

#include "GameShared/GameClasses/Sound/Playback/CgsVoice.h"

namespace CgsSound
{
namespace Playback
{
namespace Plugins
{

// DecFIGS CgsGinsuSlot.cpp:40; ARTIST DoStop @0x826C3618.  The Ginsu slot is
// the playback-side adapter which binds a loaded .gin data block to the
// GinsuPlayer plug-in selected by the authored feature registry.
class GinsuSlot : public ISlotImplementation
{
public:
    static const Name SK_SLOT_CLASSNAME;

    virtual bool DoPlay(const Slot& arSlot, PlayerVoice& arVoice,
                        Content& arContent, u32 au32Param);
    virtual bool DoStop(const Slot& arSlot, PlayerVoice& arVoice,
                        Content& arContent);
};

} // namespace Plugins
} // namespace Playback
} // namespace CgsSound
