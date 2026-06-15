#pragma once

#include "types.hpp"

namespace BrnGameState
{
// Owning header for the GameMode base of the game-mode hierarchy. Reconstructed
// from the DecFIGS DWARF (BrnGameMode.h). Only the GetName virtual (the one the
// concrete modes override) is declared here so derived modes #include this single
// home instead of forking the type. GameMode's data members (maGameModeStates,
// meCurrentState, mCountdownState, mIntroState, ...) and its other virtuals belong
// to the GameMode TU and are added to this header when it is reconstructed.
class GameMode
{
public:
    virtual const char* GetName() const;
};
}
