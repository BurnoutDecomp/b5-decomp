#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"

// CgsAssert.h is a placeholder in this tree, so the assert API is declared locally to
// match the established pattern (see SharedClasses/World/BrnWorldRegion.cpp). It lives
// in its real home (CgsDev) once that header is reconstructed.
namespace CgsDev
{
class Assert
{
public:
    static void BeginAssert();
    static void FireAssert(const char* lpcExpression, const char* lpcFile, int liLine);
    static void EndAssert();
};
}

namespace BrnGameState
{
// Hands the latest countdown value back to the caller (the ModeManager GUI feed) and
// reports whether it changed since the last query. The change flag is one-shot: it is
// cleared here so a subsequent call returns false until SetCountdownDisplay sets it
// again. Returns false (and writes nothing) when nothing changed.
bool GameMode::HasCountdownDisplayChanged(s32* lpiNewCountdownDisplay)
{
    if (!mbCountdownDisplayChanged)
    {
        return false;
    }

    if (lpiNewCountdownDisplay == nullptr)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lpiNewCountdownDisplay != NULL",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\gamestate\\modemanager\\gamemodes\\BrnGameMode.h",
            449);
        CgsDev::Assert::EndAssert();
    }

    *lpiNewCountdownDisplay = miCountdownDisplay;
    mbCountdownDisplayChanged = false;
    return true;
}
}
