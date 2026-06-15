#pragma once

#include "types.hpp"

namespace BrnGameState
{
// Minimal base-class scaffolding for the GameMode hierarchy that PursuitMode
// derives through (PursuitMode : OfflineGameMode : GameMode). Only the
// members/methods this TU's ledger attests are declared here; the full
// GameMode vtable and member layout are intentionally NOT modelled (they
// belong to the BrnGameMode / BrnOfflineGameMode TUs and would balloon scope).
//
// GetName is declared virtual + const at GameMode so PursuitMode::GetName is a
// genuine override — the DecFIGS DWARF (BrnGameMode.h:116) is authoritative for
// this shape, even though the X360 pseudocode renders the call as direct.
class GameMode
{
public:
    virtual const char* GetName() const;
};

class OfflineGameMode : public GameMode
{
};

class PursuitMode : public OfflineGameMode
{
public:
    virtual const char* GetName() const;
};
}
