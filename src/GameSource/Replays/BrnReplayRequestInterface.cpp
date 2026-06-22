#include "GameSource/Replays/BrnReplayRequestInterface.h"

#include "GameSource/Replays/BrnReplayBaseSerialiser.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cstdint>

// Reconstructed from BURNOUT_X360_ARTIST.XEX (BrnReplays::ReplayIO::RequestInterface).
//
//   Append            @ 0x823A6868 : for each of the 11 serialiser slots, asserts
//                                     the two interfaces do not both have that slot
//                                     registered, then ORs the two slot pointers.
//   RegisterSerialiser@ 0x821F34A0 : asserts the serialiser id is in range and its
//                                     slot is empty, then stores it at that slot.
//
// The X360 build uses ELEVEN slots/ids (Append's counter starts at 11; the id range
// guard fires on `id >= 11`); the leaked PS3 source declares five. X360 asm is the
// authoritative count here -- see BrnReplayRequestInterface.h / BrnReplayShared.h.

namespace BrnReplays
{
namespace ReplayIO
{
    // @ 0x823A6868
    // Pointer-OR merge of another interface's registered serialisers into this one.
    // The X360 body walks all 11 slots: it fires the "only one of each type"
    // assert (BrnReplayRequestInterface.h:94) when BOTH this slot and the other
    // interface's matching slot are non-null, then merges via a bitwise OR of the
    // two pointer values (asm: `or r11, r11, r10` ; stw). The OR of two pointers
    // where at most one is non-null yields whichever is set (or null).
    void RequestInterface::Append(const RequestInterface* lpOther)
    {
        for (s32 liIndex = 0; liIndex < KI_MAX_SERIALISERS; ++liIndex)
        {
            CGS_ASSERT(
                !(mapSerialisers[liIndex] && lpOther->mapSerialisers[liIndex]),
                "Only one of each type of serialiser can be registered\n");

            std::uintptr_t luThis  =
                reinterpret_cast<std::uintptr_t>(mapSerialisers[liIndex]);
            std::uintptr_t luOther =
                reinterpret_cast<std::uintptr_t>(lpOther->mapSerialisers[liIndex]);

            mapSerialisers[liIndex] =
                reinterpret_cast<BaseSerialiser*>(luThis | luOther);
        }
    }

    // @ 0x821F34A0
    // Installs a serialiser into the slot keyed by its id. The X360 body re-checks
    // `id < E_ID_COUNT` several times (the inlined GetId() range guard from
    // BrnReplayBaseSerialiser.h:371 plus the local "lpSerialiser->GetId() <
    // E_ID_COUNT" guard at BrnReplayRequestInterface.h:104), then asserts the target
    // slot is empty ("Serialiser with id ... already registered\n",
    // BrnReplayRequestInterface.h:105), and finally stores the pointer at
    // mapSerialisers[id] (asm: `slwi r11, id, 2 ; stwx r31, r11, r24`).
    void RequestInterface::RegisterSerialiser(BaseSerialiser* lpSerialiser)
    {
        ESerialiserId leId = lpSerialiser->GetId();

        CGS_ASSERT(leId < E_ID_COUNT, "lpSerialiser->GetId() < E_ID_COUNT");
        CGS_ASSERT(mapSerialisers[leId] == 0,
                   "Serialiser with id already registered\n");

        mapSerialisers[leId] = lpSerialiser;
    }
}
}
