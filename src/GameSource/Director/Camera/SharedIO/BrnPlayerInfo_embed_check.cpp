// Embed/exercise check for BrnDirector::Camera::VehicleInfo (compile-only gate).
// Embeds the type by value and exercises both recovered functions: the copy ctor and
// operator=. Not part of the shipped TU.

#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"

namespace
{
    using BrnDirector::Camera::VehicleInfo;

    void ExerciseVehicleInfo(const VehicleInfo& src)
    {
        VehicleInfo a(src);   // copy ctor @0x8221CDC8
        VehicleInfo b;        // default ctor
        b = a;                // operator= @0x821F49C8
        a = b;

        // Touch a couple of members by name so the embed isn't optimised to nothing.
        b.mbEngineOn = a.mbEngineOn;
        b.mfHardestImpact = a.mfHardestImpact + 1.0f;
    }

    struct Holder
    {
        VehicleInfo mInfo;   // embed by value
    };

    void Use()
    {
        Holder h;
        ExerciseVehicleInfo(h.mInfo);
    }
}
