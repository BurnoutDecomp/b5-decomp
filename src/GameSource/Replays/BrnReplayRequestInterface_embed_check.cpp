// Embed check: instantiate BrnReplays::ReplayIO::RequestInterface by value and
// exercise RegisterSerialiser / Append. Compile-only sanity for the recovered
// home + bodies.
#include "GameSource/Replays/BrnReplayRequestInterface.h"
#include "GameSource/Replays/BrnReplayBaseSerialiser.h"

#include <cstring>

using BrnReplays::BaseSerialiser;
using BrnReplays::ReplayIO::RequestInterface;

// Minimal concrete serialiser exposing a settable id for the check. BaseSerialiser
// keeps meId protected; this test-only subclass writes it through the protected
// member so the embed check can register at a chosen slot without touching the
// (other-TU-owned) construction path.
namespace
{
    struct TestSerialiser : public BaseSerialiser
    {
        explicit TestSerialiser(BrnReplays::ESerialiserId leId) { meId = leId; }
    };
}

int BrnReplayRequestInterface_embed_check()
{
    RequestInterface lIface;
    std::memset(&lIface, 0, sizeof(lIface));

    TestSerialiser lCar(BrnReplays::E_ID_RACECAR_ENTITY);
    lIface.RegisterSerialiser(&lCar);

    RequestInterface lOther;
    std::memset(&lOther, 0, sizeof(lOther));

    TestSerialiser lGame(BrnReplays::E_ID_GAME_MODULE);
    lOther.RegisterSerialiser(&lGame);

    // Merge lOther's game-module serialiser into lIface (disjoint slots -> OK).
    lIface.Append(&lOther);

    return (lIface.mapSerialisers[BrnReplays::E_ID_RACECAR_ENTITY] == &lCar &&
            lIface.mapSerialisers[BrnReplays::E_ID_GAME_MODULE] == &lGame)
               ? 0
               : 1;
}
