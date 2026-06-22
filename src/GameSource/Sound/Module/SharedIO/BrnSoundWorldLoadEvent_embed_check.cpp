// Gate embedder for the BrnSound::Module::Io::SoundWorldLoadEvent event-queue group:
// pins the SoundWorldLoadEvent layout and references the two X360-emitted base-queue
// methods (AddEvent @0x822C9B88, Append @0x823C4238) instantiated by
// BaseEventQueue_SoundWorldLoadEvent_AddEventAppend.cpp so they are link-reachable
// from the gate.
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"
#include "GameSource/Sound/Module/SharedIO/BrnSoundRootSharedIO.h"

using BrnSound::Module::Io::SoundWorldLoadEvent;

// SoundWorldLoadEvent layout pins (DWARF-faithful): eLoadEvent@0, u16@4; sizeof 8.
static_assert(sizeof(SoundWorldLoadEvent) == 8, "SoundWorldLoadEvent size");
static_assert(alignof(SoundWorldLoadEvent) == 4, "SoundWorldLoadEvent align");

void _EmbedBrnSoundWorldLoadEvent(CgsModule::BaseEventQueue<SoundWorldLoadEvent>& lrQueue)
{
    SoundWorldLoadEvent lEvent = {};
    lrQueue.AddEvent(lEvent);
    lrQueue.Append(lrQueue);
}
