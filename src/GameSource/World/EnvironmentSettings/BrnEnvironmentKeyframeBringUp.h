#pragma once
// [FLAG PC bring-up] see BrnEnvironmentKeyframeBringUp.cpp -- the shipped noon keyframe
// (ENV_KF_Paradise_ingame_junk_city_1200), embedded so the world effects layer carries the game's own
// daytime post-fx while the environment manager's streamer is not live. DELETE-WHEN it is.
#include "types.hpp"
#include "GameSource/World/EnvironmentSettings/BrnEnvironmentKeyframe.h"

namespace BrnWorld
{
namespace EnvironmentSettings
{
    const Keyframe& GetBringUpKeyframeCity1200();
}
}
