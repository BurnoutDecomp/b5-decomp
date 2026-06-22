// Embed check for BrnWorld::CrashIO::NetworkInputInterface.
// Instantiates the interface by value, exercises Construct / MarkRaceCarForUpdate / operator=,
// and pins the X360-attested layout (8-byte bitset, 0x790 queue stride, 0x3C90 total) so a
// layout/signature regression in the home or generics fails the gate. Not part of the game build.
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleNetworkIOInterfaces.h"

static_assert(sizeof(BrnWorld::CrashIO::NetworkInputInterface::CrashingTrafficUpdateQueue) == 0x790,
              "CrashingTrafficUpdateQueue must be 0x790 bytes (12-byte base header + 16-aligned 24*80 maEvents)");
static_assert(sizeof(BrnWorld::CrashIO::NetworkInputInterface) == 0x3C90,
              "NetworkInputInterface must be 0x3C90 bytes (16-aligned bitset + 8 * 0x790 queues)");

int NetworkInputInterface_embed_check()
{
    using namespace BrnWorld::CrashIO;

    NetworkInputInterface lInput;
    lInput.Construct();

    lInput.MarkRaceCarForUpdate(0);
    lInput.MarkRaceCarForUpdate(BrnWorld::KI_MAX_ACTIVE_RACE_CARS - 1);

    NetworkInputInterface lCopy;
    lCopy.Construct();
    lCopy = lInput;                       // operator= @ 0x823C8A78

    return static_cast<int>(sizeof(lCopy));
}
