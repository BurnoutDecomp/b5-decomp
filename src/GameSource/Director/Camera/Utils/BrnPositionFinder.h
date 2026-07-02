#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3 (rw::math::vpu)

// BrnDirector::Camera::Utils::PositionFinder - a small "find me a safe camera
// position" latch: FindPosition arms it with a target + displacement, Update polls
// the director world map until a safe position lands in mPosition. DWARF home
// BrnPositionFinder.h:55; X360 layout (documented; access is BY NAME):
//   +0x00 mPosition  +0x10 mTarget  +0x20 mDisplacement
//   +0x30 mbIsInitialised  +0x31 mbFoundPosition  +0x32 mbConstructed
namespace BrnDirector
{
namespace Camera
{
    struct BehaviourSharedInfo;   // the per-frame shared info (see BehaviourRig.h slice)

namespace Utils
{
    struct PositionFinder
    {
        // DWARF h:59 -- nested parameter block; its private Construct (cpp:45) is its
        // own ledger function (nothing here consumes it).
        struct Parameters;

        // DWARF h:65/h:68 (cpp:53/cpp:62) -- their own ledger functions
        // (declaration-only here).
        void Construct();
        void Clear();

        // @0x821F8E68 (this TU, cpp:~70) -- arm the finder: latch the target +
        // displacement and reset the found flag.
        void FindPosition(Vector3 lTarget, Vector3 lDisplacement);

        // @0x8223FCD8 (this TU, cpp:~88) -- while armed and unresolved, ask the
        // world map for a safe position near the target.
        void Update(const BehaviourSharedInfo& lrSharedInfo);

        // DWARF h:80/h:83/h:86 -- trivial accessors (header-inline on the X360; the
        // flags/result they expose are the ones the two bodied functions maintain).
        bool IsInitialised() const     { return mbIsInitialised; }
        bool HasFoundPosition() const  { return mbFoundPosition; }
        const Vector3& GetPosition() const { return mPosition; }

    private:
        Vector3 mPosition;       // +0x00 (DWARF h:90; the world-map result)
        Vector3 mTarget;         // +0x10 (DWARF h:92)
        Vector3 mDisplacement;   // +0x20 (DWARF h:93)
        bool    mbIsInitialised; // +0x30 (DWARF h:95)
        bool    mbFoundPosition; // +0x31 (DWARF h:96)
        bool    mbConstructed;   // +0x32 (DWARF h:97)
    };
}
}
}
