#include "GameSource/World/BrnWorldModuleIO.h"

// BrnWorldModuleIO partfile (blocked-TU wave), beside its home BrnWorldModuleIO.cpp.

namespace BrnWorldIO
{

// The update input buffer's teardown, run by IOBufferStack::DestroyIOBuffer<UpdateInputBuffer>.
// Clears the per-active-race-car colour / paint-finish / car-select state (the two contact
// flags are left alone), tears down the two variable event queues, empties the takedown
// queue, re-seeds the payback slots exactly as Construct does, then the IOBuffer base
// teardown.
void UpdateInputBuffer::Destruct()
{
    for (EActiveRaceCarIndex leIndex = E_ACTIVE_RACE_CAR_INDEX_0;
         leIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT;
         leIndex++)
    {
        mau16RaceCarColourIndex[leIndex]         = 0;
        mau16RaceCarPaintFinishIndex[leIndex]    = 0;
        mabRaceCarColourIndexValid[leIndex]      = false;
        mabRaceCarPaintFinishIndexValid[leIndex] = false;
        mabCarSelectStatus[leIndex]              = false;
        mabCarSelectStatusValid[leIndex]         = false;
    }

    mGameActionQueue.Destruct();                            // +147572  VEQ<13312,16>
    mInWorldEventQueue.Destruct();                          // +297532  VEQ<4096,16>
    mTakedownEventQueue.Clear();                            // +160952  (miLength = 0)
    meActivePaybackType      = static_cast<BrnNetwork::EPaybackType>(3);   // +297524
    meActivePaybackAggressor = E_ACTIVE_RACE_CAR_INDEX_INVALID;            // +297528

    CgsModule::IOBuffer::Destruct();
}

}
