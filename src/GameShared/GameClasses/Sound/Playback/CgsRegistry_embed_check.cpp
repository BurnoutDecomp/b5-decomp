// Tiny embed/usage check for the CgsSound::Playback Object + Registry homes.
// Exercises the bodied functions so the gate sees them instantiated together.
#include "GameShared/GameClasses/Sound/Playback/CgsObject.h"
#include "GameShared/GameClasses/Sound/Playback/CgsRegistry.h"

namespace
{
void EmbedCheck()
{
    using namespace CgsSound::Playback;

    RegistrySpec lSpec;
    lSpec.mu32EntityCount   = 8;
    lSpec.muDataSize        = 0;
    lSpec.muStringTableSize = 0;

    Registry lRegistry(lSpec);

    Entity lEntity = { Name("entity"), Name("type") };
    lRegistry.AddEntity(lEntity);

    (void)lRegistry;
}
}
