#include "GameShared/GameClasses/World/CgsWorldMap2D.h"

#include <cstdio>

// A labelled grid distinguishes rows from altitude and catches a transposed map.
// ARTIST's callers gather X/Z with mask 00 01 02 03 18 19 1A 1B ... before GetValue.
int main()
{
    const struct
    {
        u16 muWidth;
        u16 muHeight;
        u8 maValues[6];
    } lGrid = {3, 2, {0, 1, 2, 11, 12, 13}};

    CgsWorld::WorldMap2D lMap;
    lMap.Construct(&lGrid, Vector2{-30.0f, -20.0f, 0.0f, 0.0f},
                          Vector2{60.0f, 40.0f, 0.0f, 0.0f});

    int liFailures = 0;
    int liChecks = 0;
    const float kafHeights[] = {-1500.0f, 0.0f, 1500.0f};
    const auto Check = [&](u8 luActual, u8 luExpected)
    {
        ++liChecks;
        if (luActual != luExpected)
        {
            ++liFailures;
            std::printf("FAIL check %d: got %u, expected %u\n", liChecks, luActual, luExpected);
        }
    };

    for (int liRow = 0; liRow < 2; ++liRow)
    {
        for (int liColumn = 0; liColumn < 3; ++liColumn)
        {
            const float lfX = -20.0f + 20.0f * liColumn;
            const float lfZ = -10.0f + 20.0f * liRow;
            const u8 luExpected = lGrid.maValues[liRow * 3 + liColumn];
            Check(lMap.GetValue(Vector2{lfX, lfZ, 0.0f, 0.0f}), luExpected);
            for (float lfHeight : kafHeights)
            {
                Check(lMap.GetValue(Vector3{lfX, lfHeight, lfZ, 0.0f}), luExpected);
            }
        }
    }

    Check(lMap.GetValue(Vector3{-30.0f, 100.0f, -20.0f, 0.0f}), 0);
    Check(lMap.GetValue(Vector3{30.0f, 0.0f, 0.0f, 0.0f}), CgsWorld::KU_INVALID_WORLD_MAP_VALUE);
    Check(lMap.GetValue(Vector3{0.0f, 0.0f, 20.0f, 0.0f}), CgsWorld::KU_INVALID_WORLD_MAP_VALUE);
    Check(lMap.GetValue(Vector3{-60.0f, 0.0f, 0.0f, 0.0f}), CgsWorld::KU_INVALID_WORLD_MAP_VALUE);
    Check(lMap.GetValue(Vector3{0.0f, 0.0f, -50.0f, 0.0f}), CgsWorld::KU_INVALID_WORLD_MAP_VALUE);

    std::printf("WorldMap2D: %d/%d passed\n", liChecks - liFailures, liChecks);
    return liFailures != 0;
}
