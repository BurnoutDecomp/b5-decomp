#include "GameSource/Physics/ContactSpies/BrnContactSpyEvents.h"
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"  // CgsSceneManager::SceneManagerIO::PotentialContact (committed source record)
#include "GameShared/GameClasses/Core/CgsAssert.h"                            // CgsDev::Assert Begin/Fire/End triad

// BrnPhysics::ContactSpy::BaseContact::Construct  @ X360 0x8259F6F8
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Static factory that stamps a resolved
// BaseContact from (a) the spy's five accumulated stress/geometry vectors and (b) the
// scene-manager potential contact that produced it. The potential contact is the committed
// CgsSceneManager::SceneManagerIO::PotentialContact: its muVolumeInstanceIdA/B are 8-byte
// packed handles whose HIGH dword is the 32-bit entity word (VolumeInstanceId::muId >> 32),
// and muPolyTagB is the sub-type tag. The X360 reads the entity words with `ld; srdi r,r,32`
// (== the high dword == the entity word) and stores them into mEntityIdA/mEntityIdB.
//
// Collision tag: if the high (owner) byte of mEntityIdB is non-zero the tag is forced to the
// invalid sentinel 0xFFFF8000 (X360 `sth -1 @8; sth -0x8000 @0xA` == big-endian word
// 0xFFFF8000); otherwise it is muPolyTagB with its two 16-bit halves SWAPPED
// (X360 `sth lo16 @8; sth hi16 @0xA` == big-endian word (lo16<<16)|hi16). Both values are
// written here as the logical u32 so the field matches the console byte-for-byte on any host.
namespace BrnPhysics
{
namespace ContactSpy
{
    BaseContact* BaseContact::Construct(
        BaseContact* lpResult,
        const Vector3* lpInSpy,
        const CgsSceneManager::SceneManagerIO::PotentialContact* lpInPotentialContact)
    {
        CGS_ASSERT(lpInSpy != nullptr, "lpInSpy != NULL");
        CGS_ASSERT(lpInPotentialContact != nullptr, "lpInPotentialContact != NULL");

        // Entity words = high dword of each 8-byte VolumeInstanceId (X360 ld;srdi 32;stw).
        lpResult->mEntityIdA.muValue =
            static_cast<u32>(lpInPotentialContact->muVolumeInstanceIdA.muId >> 32);
        lpResult->mEntityIdB.muValue =
            static_cast<u32>(lpInPotentialContact->muVolumeInstanceIdB.muId >> 32);

        // Five 16-byte stress/geometry vectors copied straight out of the spy
        // (spy +0x00/0x10/0x20/0x30/0x40 -> result +0x10/0x20/0x30/0x40/0x50).
        lpResult->mFrictionStress = lpInSpy[0];
        lpResult->mNormalStress   = lpInSpy[1];
        lpResult->mNormal         = lpInSpy[2];
        lpResult->mPointOnA       = lpInSpy[3];
        lpResult->mPointOnB       = lpInSpy[4];

        // X360 tests the high (owner) byte of the entity word at result+4 (lbz 4(r31)).
        if (((lpResult->mEntityIdB.muValue >> 24) & 0xFFu) != 0u)
        {
            lpResult->mCollisionTagB.muValue = 0xFFFF8000u;
        }
        else
        {
            const u32 luPolyTag = lpInPotentialContact->muPolyTagB;
            lpResult->mCollisionTagB.muValue =
                ((luPolyTag & 0xFFFFu) << 16) | (luPolyTag >> 16);
        }

        return lpResult;
    }
}
}
