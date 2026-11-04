#include "rw/audio/core/EaLayer3Dec.h"

namespace rw
{
namespace audio
{
namespace core
{

// ARTIST 0x82B95EA0
s32 EaLayer31Dec::CreateInstanceEvent(EaLayer31Dec *self)
{
    return self->CreateInstance(0);
}

} // namespace core
} // namespace audio
} // namespace rw
