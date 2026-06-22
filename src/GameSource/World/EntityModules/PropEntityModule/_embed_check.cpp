// Gate embedder for the BrnWorld-IO-events group: forces the PropVFXLocatorEvent layout
// + its EventQueue<...,10> Construct, and instantiates the AudioCarDataLoadedEvent queue
// (Construct/AddEvent/Append) and the OutputBuffer_PreScene accessors so the bodied
// .cpp's are link-reachable from the gate.
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"
#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h"

using BrnWorld::PropEntityIO::PropVFXLocatorEvent;

// PropVFXLocatorEvent layout pins (DWARF-faithful): Matrix44Affine@0, u32@64, EEventType@68.
static_assert(sizeof(PropVFXLocatorEvent) == 80, "PropVFXLocatorEvent size");
static_assert(alignof(PropVFXLocatorEvent) == 16, "PropVFXLocatorEvent align");

void _EmbedBrnWorldIoEvents(CgsModule::EventQueue<PropVFXLocatorEvent, 10>& lrVfxQueue,
                            CgsModule::EventQueue<BrnWorld::RaceCarEntityModuleIO::AudioCarDataLoadedEvent, 16>& lrAudioQueue,
                            BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PreScene& lrPreScene)
{
    lrVfxQueue.Construct();

    lrAudioQueue.Construct();
    BrnWorld::RaceCarEntityModuleIO::AudioCarDataLoadedEvent lEvent = {};
    lrAudioQueue.AddEvent(lEvent);
    lrAudioQueue.Append(lrAudioQueue);

    (void)lrPreScene.GetRaceCarAIInterface();
    (void)lrPreScene.IsRequestingRivalUpdate();
}
