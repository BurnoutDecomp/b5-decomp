#pragma once

#include "SDKs/EATech/rwcollision/volume_debug_access.h"

namespace BrnPhysics { namespace Props {

// Serialized volumes store a type word in the descriptor field until FixUp.
struct FixableVolume : public rw::collision::Volume
{
    void FixUp();
    void FixDown();
};

} }
