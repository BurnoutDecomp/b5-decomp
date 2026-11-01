#include "GameShared/GameClasses/RenderWare/FixableVolume.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include <cstring>

// ARTIST FixUp 828A87A0 and FixDown 828A8830 store the converted word
// before reporting unsupported types. The serialized word occupies mpVTable's
// representation until FixUp; memcpy preserves those bytes without a typed alias.

namespace BrnPhysics
{
namespace Props
{
    void FixableVolume::FixUp()
    {
        u32 luType;
        memcpy(&luType, &mpVTable, sizeof(luType));
        mpVTable = rw::collision::gVolumeVTable[luType];
        switch (luType)
        {
            case 1: case 2: case 3: case 4: case 5: return;
            default:
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("Unsupported type\n", "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\renderware\\FixableVolume.cpp", 92);
                CgsDev::Assert::EndAssert();
                return;
        }
    }

    void FixableVolume::FixDown()
    {
        const u32 luType = mpVTable->muTypeID;
        memcpy(&mpVTable, &luType, sizeof(luType));
        switch (luType)
        {
            case 1: case 2: case 3: case 4: case 5: return;
            default:
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("Unsupported type\n", "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\renderware\\FixableVolume.cpp", 156);
                CgsDev::Assert::EndAssert();
                return;
        }
    }
}
}
