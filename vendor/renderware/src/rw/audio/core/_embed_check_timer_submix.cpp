// Embed check for the rw::audio::core TimerHandle / SubMixConnector homes -- forces the
// headers to parse/instantiate standalone (no .cpp dependency) and pins the SubMix /
// SubMixConnector / TimerHandle layouts the bodied functions rely on.
#include "rw/audio/core/TimerHandle.h"
#include "rw/audio/core/SubMixConnector.h"

namespace
{
void EmbedCheckTimerSubMix()
{
    using namespace rw::audio::core;
    sizeof(TimerHandle);
    sizeof(SubMix);
    sizeof(SubMixConnector);
}
}
