#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstring>   // std::memcpy (models the Xbox XMemCpy block-copy intrinsic)

// BrnPhysics::Vehicle::VehicleOutputInterface + CrashingRaceCarInterface -- the bodied ledger
// functions homed by this group. Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// FLAG -- VehicleOutputInterface::AddTrafficState @0x825EC390 is declared on the class but
// intentionally NOT bodied: its X360 body is a deep VMX128 per-wheel projection routine reaching
// SimpleVehiclePhysics wheel/contact-frame internals whose full layout is not homed in any
// committed header, and whose per-wheel reciprocal-magnitude Newton-Raphson normalization + lane
// splats cannot be reconstructed BY NAME without fabricating a large accessor surface. It remains
// declaration-only until the BrnSimpleVehiclePhysics wheel-state TU lands its own ledger.

namespace BrnPhysics
{
namespace Vehicle
{
    // Number of race-car slots (BitArray<8> width).
    static const s32 KI_NUM_RACE_CARS = 8;

    // @0x822B4860  VehicleOutputInterface::GetRaceCarState (non-const)
    //   (dossier 'GetRaceCar' is a stripped-name artifact; DWARF has the const/non-const
    //   GetRaceCarState overloads at :343/:347, both returning RaceCarState*.)
    RaceCarState* VehicleOutputInterface::GetRaceCarState(s32 liRaceCarIndex)
    {
        CGS_ASSERT(static_cast<u32>(liRaceCarIndex) < 8u, "invalid index : ");
        CGS_ASSERT(mUsedRaceCars.IsBitSet(static_cast<u32>(liRaceCarIndex)),
                   "mUsedRaceCars.IsBitSet( liRaceCarIndex )");
        return &maRaceCarStates[liRaceCarIndex];
    }

    // @0x825C08D0  VehicleOutputInterface::GetRaceCarState (const)
    //   The const overload (DWARF :343); the only difference from the non-const @0x822B4860 is the
    //   dropped FireAssert file/line args.
    const RaceCarState* VehicleOutputInterface::GetRaceCarState(s32 liRaceCarIndex) const
    {
        CGS_ASSERT(static_cast<u32>(liRaceCarIndex) < 8u, "invalid index : ");
        CGS_ASSERT(mUsedRaceCars.IsBitSet(static_cast<u32>(liRaceCarIndex)),
                   "mUsedRaceCars.IsBitSet( liRaceCarIndex )");
        return &maRaceCarStates[liRaceCarIndex];
    }

    // The X360 asm-called symbol distinct from GetRaceCarState: returns &maRaceCarStates[i] for the
    // CrashingRaceCarInterface's per-in-use-car walk.
    const RaceCarState* VehicleOutputInterface::GetRaceCar(u32 luRaceCarIndex) const
    {
        return &maRaceCarStates[luRaceCarIndex];
    }

    // @0x823C89C8  VehicleOutputInterface::operator=
    //   (dossier 'operat' is a truncated name.) The X360 body copies the fixed head byte-for-byte,
    //   re-merges the two bounded event queues, block-copies the game-event queue, and byte-copies
    //   the aggressive-driving flags. Returns *this. ADDITIVE GROW: a real ledger func not in the
    //   DWARF member set, no field reordered/retyped.
    VehicleOutputInterface& VehicleOutputInterface::operator=(const VehicleOutputInterface& lOther)
    {
        // @+0x00: BitArray<8> head.
        mUsedRaceCars = lOther.mUsedRaceCars;

        // @+0x10: 8 x RaceCarState (1120-byte stride) via the Xbox block-copy intrinsic (XMemCpy),
        //         modelled as std::memcpy (RaceCarState is trivially copyable).
        for (s32 liCar = 0; liCar < KI_NUM_RACE_CARS; ++liCar)
        {
            std::memcpy(&maRaceCarStates[liCar], &lOther.maRaceCarStates[liCar], sizeof(RaceCarState));
        }

        // @+0x2310 / @+0x2620: reset the live count then merge the source's live events.
        mImpactEventQueue.Clear();
        mImpactEventQueue.Append(lOther.mImpactEventQueue);

        mTrafficStateQueue.Clear();
        mTrafficStateQueue.Append(lOther.mTrafficStateQueue);

        // @+0x65F0: GameEventQueue (0x610 bytes) -- raw block copy.
        std::memcpy(&mGameEventQueueStorage, &lOther.mGameEventQueueStorage, sizeof(mGameEventQueueStorage));

        // @+0x6C00: AggressiveDrivingFlags (5 bytes, the bdnz-5 tail loop).
        mAggressiveDrivingFlags = lOther.mAggressiveDrivingFlags;

        return *this;
    }

    // @0x823625C0  CrashingRaceCarInterface::SetFromVehicleOutputInterface
    //   For every race-car slot in use (mUsedRaceCars bit set) copy that car's
    //   RaceCarState::mbResetCarTransform flag (byte @1098) into mabCrashingRaceCars[]. The
    //   per-iteration index<8 guard is the inlined CgsBitArray bounds assert (loop-bounded, never
    //   fires); GetUsedCarsBitArray() == the interface's first member.
    void CrashingRaceCarInterface::SetFromVehicleOutputInterface(const VehicleOutputInterface* lpOutput)
    {
        for (s32 liIndex = 0; liIndex < KI_NUM_RACE_CARS; ++liIndex)
        {
            CGS_ASSERT(static_cast<u32>(liIndex) < 8u, "invalid index : < 8");

            if (lpOutput->GetUsedCarsBitArray().IsBitSet(static_cast<u32>(liIndex)))
            {
                const RaceCarState* lpState = lpOutput->GetRaceCar(static_cast<u32>(liIndex));
                mabCrashingRaceCars[liIndex] = lpState->mbResetCarTransform;
            }
        }
    }
}
}
