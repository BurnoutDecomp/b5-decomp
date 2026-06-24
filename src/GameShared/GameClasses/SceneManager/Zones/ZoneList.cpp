#include "GameShared/GameClasses/SceneManager/Zones/ZoneList.h"

// CgsSceneManager::ZoneList relocation helpers, reconstructed from BURNOUT_X360_ARTIST.XEX.
// This TU homes the two pointer-(de)relocation functions:
//
//   ZoneList::FixUp   @ 0x828D05B8  -- serialise-IN  (base-relative -> absolute)
//   ZoneList::FixDown @ 0x828D0640  -- serialise-OUT (absolute -> base-relative)
//
// X360 store-for-store notes:
//   - The four stored pointers (mpPoints @+0, mpZones @+4, mpuZonePointStarts @+8,
//     mpiZonePointCounts @+0xC) are each adjusted by the load base; muTotalZones (@+0x10)
//     is the embedded-Zone loop count and is NOT relocated.
//   - FixUp adds the base to the four pointers FIRST, then walks the (now absolute) zone
//     array and calls Zone::FixUp(base) on each (stride 0x30 == sizeof(Zone)).
//   - FixDown walks the zone array FIRST (while mpZones is still absolute), calling
//     Zone::FixDown(base) on each, THEN subtracts the base from the four pointers.
//   - All adjustments are 32-bit add/subf of the byte base address; modelled on the host
//     with usize pointer arithmetic.

namespace CgsSceneManager
{
    // X360 0x828D05B8.
    void ZoneList::FixUp(void* lpBaseAddress)
    {
        const usize luBase = reinterpret_cast<usize>(lpBaseAddress);

        // Add the base to each stored pointer (serialise-in: file-relative -> absolute).
        mpPoints = reinterpret_cast<rw::math::vpu::Vector2*>(
            reinterpret_cast<usize>(mpPoints) + luBase);
        mpZones = reinterpret_cast<Zone*>(
            reinterpret_cast<usize>(mpZones) + luBase);
        mpuZonePointStarts = reinterpret_cast<u32*>(
            reinterpret_cast<usize>(mpuZonePointStarts) + luBase);
        mpiZonePointCounts = reinterpret_cast<s16*>(
            reinterpret_cast<usize>(mpiZonePointCounts) + luBase);

        // Fix up every embedded zone (mpZones is now absolute).
        if (muTotalZones != 0)
        {
            for (u32 luZoneIndex = 0; luZoneIndex < muTotalZones; ++luZoneIndex)
            {
                mpZones[luZoneIndex].FixUp(lpBaseAddress);
            }
        }
    }

    // X360 0x828D0640.
    void ZoneList::FixDown(void* lpBaseAddress)
    {
        const usize luBase = reinterpret_cast<usize>(lpBaseAddress);

        // Fix down every embedded zone first (mpZones is still absolute here).
        if (muTotalZones != 0)
        {
            for (u32 luZoneIndex = 0; luZoneIndex < muTotalZones; ++luZoneIndex)
            {
                mpZones[luZoneIndex].FixDown(lpBaseAddress);
            }
        }

        // Subtract the base from each stored pointer (serialise-out: absolute -> file-relative).
        mpZones = reinterpret_cast<Zone*>(
            reinterpret_cast<usize>(mpZones) - luBase);
        mpuZonePointStarts = reinterpret_cast<u32*>(
            reinterpret_cast<usize>(mpuZonePointStarts) - luBase);
        mpiZonePointCounts = reinterpret_cast<s16*>(
            reinterpret_cast<usize>(mpiZonePointCounts) - luBase);
        mpPoints = reinterpret_cast<rw::math::vpu::Vector2*>(
            reinterpret_cast<usize>(mpPoints) - luBase);
    }
}
