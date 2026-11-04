#include "rw/audio/core/EaLayer3Dec.h"

namespace rw
{
namespace audio
{
namespace core
{

// ARTIST 0x82B95EA8
s32 EaLayer32PcmDec::CreateInstanceEvent(EaLayer32PcmDec *self)
{
    return self->CreateInstance(1);
}

} // namespace core
} // namespace audio
} // namespace rw
