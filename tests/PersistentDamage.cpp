// Harness for run_persistent_damage.py (crash parity G67-D1 / G67-D2; the re-colour legs added by
// FX-RCEM4 for CHAIN-RECOLOUR, 2026-09-24).
// Compiles the SHIPPED text of RaceCar::IncreasePersistentDamage (inlined @0x822F4264),
// RaceCarEntityModule::GetPersistentDamageCarCount @0x822A4A38 and the taken-down AI block of
// ProcessRaceCarCrashCompleteEvents (0x822F40C8..0x822F4390) against minimal stand-ins, then
// drives Road Rage takedowns through them. Pre-fix the block only logged a PARK line and
// mfPersistentDamage had no writer, so a taken-down rival never carried damage into its respawn.
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <limits>

typedef float         f32;
typedef int32_t       s32;
typedef uint32_t      u32;
typedef uint8_t       u8;
typedef uint64_t      u64;

static int giAsserts = 0;
#define CGS_ASSERT(cond, msg) do { if (!(cond)) { ++giAsserts; std::printf("ASSERT: %s\n", msg); } } while (0)

namespace CgsDev { namespace Log {
struct Sink { template <class T> Sink& operator<<(const T&) { return *this; } };
Sink* gpDebugPrint = nullptr;
} }

namespace BrnGameState { struct GameModeParams {
    static const u64 KU_FLAG_AI_PERSISTENT_DAMAGE  = 0x40000000ull;
    static const u64 KU_FLAG_SET_OPPONENTS_TO_COPS = 0x400000000ull;
}; }

enum ERaceCarType { E_RACE_CAR_TYPE_PLAYER = 0, E_RACE_CAR_TYPE_AI = 1, E_RACE_CAR_TYPE_NETWORK = 2,
                    E_RACE_CAR_TYPE_INACTIVE = 3, E_RACE_CAR_TYPE_COUNT = 4 };
enum { E_GLOBAL_RACE_CAR_INDEX_COUNT = 35 };

class RaceCar
{
public:
    ERaceCarType GetType() const { return static_cast<ERaceCarType>(muType); }
    f32 GetPersistentDamage() const { return mfPersistentDamage; }
    bool IncreasePersistentDamage();
    // CHAIN-RECOLOUR (FX-RCEM4, 2026-09-24): the colour pair the block's re-colour legs write.
    s32  GetColourIndex() const { return miColourIndex; }
    s32  GetColourPalette() const { return miColourPalette; }
    void SetColourIndex(s32 liColourIndex) { miColourIndex = liColourIndex; }
    f32 mfPersistentDamage = 0.0f;
    u8  muType = E_RACE_CAR_TYPE_INACTIVE;
    s32 miColourIndex = 11, miColourPalette = 0;
};

#include "increase.inc"

struct ActiveRaceCar
{
    RaceCar* mpRaceCar = nullptr;
    bool     mbTakenDown = false;
    bool IsTakenDown() const { return mbTakenDown; }
    RaceCar* GetGlobalRaceCar() const { return mpRaceCar; }
};

class RaceCarEntityModule
{
public:
    RaceCar maRaceCars[E_GLOBAL_RACE_CAR_INDEX_COUNT];
    u64     mxFlags = 0;
    bool GetGameModeFlag(u64 lxMask) const { return (mxFlags & lxMask) != 0; }
    s32 GetPersistentDamageCarCount() const;
    void TakenDownBlock(ActiveRaceCar* lpActiveRaceCar, u32 luActiveRaceCarIndex);
    // CHAIN-RECOLOUR (FX-RCEM4): the palette resource the re-colour asserts read, and a
    // GetRandomCarColour @0x822EA088 stand-in (its own logic is tested by run_fxrcem4_recolour.py).
    struct Palette { s32 miNumColours = 20; s32 GetNumColours() const { return miNumColours; } };
    struct Palettes { Palette maPalettes[4]; };
    struct PaletteResource { Palettes mPalettes; Palettes* operator->() { return &mPalettes; } } mCarColoursResource;
    int miRandomColourCalls = 0; s32 miRandomColourPalette = -2, miRandomColourCurrent = -2;
    s32 GetRandomCarColour(s32 liPaletteIndex, s32 liColourIndex)
    { ++miRandomColourCalls; miRandomColourPalette = liPaletteIndex; miRandomColourCurrent = liColourIndex; return 7; }
};

// DWARF BrnRaceCarEntityModule.cpp:306 (TU-local in the CrashExit TU).
static const s32 KI_BLACK_CAR_COLOUR_INDEX = 6;

#include "count.inc"

void RaceCarEntityModule::TakenDownBlock(ActiveRaceCar* lpActiveRaceCar, u32 luActiveRaceCarIndex)
{
    (void)luActiveRaceCarIndex;
#include "block.inc"
}

int main()
{
    int liChecks = 0, liFailures = 0;
    auto Check = [&](bool lbPass, const char* lpcName) {
        ++liChecks;
        if (!lbPass) { ++liFailures; std::printf("FAIL: %s\n", lpcName); }
    };

    // ---- IncreasePersistentDamage: 0.3f steps in f32, reset to 0 at >= 1.0f -------------------
    {
        RaceCar lCar; lCar.muType = E_RACE_CAR_TYPE_AI;
        f32 lfExpect = 0.0f;
        for (int i = 0; i < 3; ++i)
        {
            lfExpect += 0.3f;
            const bool lbWrapped = lCar.IncreasePersistentDamage();
            Check(!lbWrapped && lCar.mfPersistentDamage == lfExpect, "0.3f step accumulates and does not wrap");
        }
        Check(lCar.IncreasePersistentDamage() && lCar.mfPersistentDamage == 0.0f,
              "fourth step reaches >= 1.0f: damage reset to 0 and re-colour requested");
        lCar.mfPersistentDamage = 0.7f;
        Check(lCar.IncreasePersistentDamage() && lCar.mfPersistentDamage == 0.0f, "0.7 + 0.3 == 1.0f wraps (blt is strict)");
        lCar.mfPersistentDamage = std::numeric_limits<f32>::quiet_NaN();
        Check(lCar.IncreasePersistentDamage() && lCar.mfPersistentDamage == 0.0f, "unordered sum resets like the console's blt");
    }

    // ---- GetPersistentDamageCarCount ----------------------------------------------------------
    {
        RaceCarEntityModule lModule;
        lModule.maRaceCars[0].muType = E_RACE_CAR_TYPE_PLAYER;  lModule.maRaceCars[0].mfPersistentDamage = 0.0f;
        lModule.maRaceCars[1].muType = E_RACE_CAR_TYPE_AI;      lModule.maRaceCars[1].mfPersistentDamage = 0.3f;
        lModule.maRaceCars[2].muType = E_RACE_CAR_TYPE_AI;      lModule.maRaceCars[2].mfPersistentDamage = 0.6f;
        lModule.maRaceCars[3].muType = E_RACE_CAR_TYPE_INACTIVE; lModule.maRaceCars[3].mfPersistentDamage = 0.9f;
        lModule.maRaceCars[34].muType = E_RACE_CAR_TYPE_NETWORK; lModule.maRaceCars[34].mfPersistentDamage = 0.3f;
        lModule.maRaceCars[5].muType = E_RACE_CAR_TYPE_AI;      lModule.maRaceCars[5].mfPersistentDamage = std::numeric_limits<f32>::quiet_NaN();
        Check(lModule.GetPersistentDamageCarCount() == 3, "count: damaged cars in the world (inactive and NaN skipped, slot 34 included)");
    }

    // ---- the taken-down block ------------------------------------------------------------------
    {
        RaceCarEntityModule lModule;
        for (int i = 1; i <= 6; ++i) lModule.maRaceCars[i].muType = E_RACE_CAR_TYPE_AI;
        lModule.maRaceCars[0].muType = E_RACE_CAR_TYPE_PLAYER;
        ActiveRaceCar lSlot; lSlot.mpRaceCar = &lModule.maRaceCars[1]; lSlot.mbTakenDown = true;

        lModule.TakenDownBlock(&lSlot, 1u);
        Check(lModule.maRaceCars[1].mfPersistentDamage == 0.0f, "flag clear (free roam): no persistent damage");

        lModule.mxFlags = BrnGameState::GameModeParams::KU_FLAG_AI_PERSISTENT_DAMAGE;
        lSlot.mbTakenDown = false;
        lModule.TakenDownBlock(&lSlot, 1u);
        Check(lModule.maRaceCars[1].mfPersistentDamage == 0.0f, "not taken down: no persistent damage");

        lSlot.mbTakenDown = true;
        lModule.TakenDownBlock(&lSlot, 1u);
        Check(lModule.maRaceCars[1].mfPersistentDamage == 0.3f, "Road Rage takedown of a clean rival carries 0.3");
        lModule.TakenDownBlock(&lSlot, 1u);
        Check(lModule.maRaceCars[1].mfPersistentDamage == 0.3f + 0.3f, "second takedown carries 0.6");

        lModule.maRaceCars[2].mfPersistentDamage = 0.3f;
        lModule.maRaceCars[3].mfPersistentDamage = 0.3f;
        ActiveRaceCar lFourth; lFourth.mpRaceCar = &lModule.maRaceCars[4]; lFourth.mbTakenDown = true;
        lModule.TakenDownBlock(&lFourth, 4u);
        Check(lModule.maRaceCars[4].mfPersistentDamage == 0.0f, "three rivals already damaged: a clean rival is not damaged");
        lModule.TakenDownBlock(&lSlot, 1u);
        Check(std::fabs(lModule.maRaceCars[1].mfPersistentDamage - 0.9f) < 1e-6f, "an already-damaged rival still accrues at the cap (0.9)");

        ActiveRaceCar lPlayer; lPlayer.mpRaceCar = &lModule.maRaceCars[0]; lPlayer.mbTakenDown = true;
        lModule.TakenDownBlock(&lPlayer, 0u);
        Check(lModule.maRaceCars[0].mfPersistentDamage == 0.0f, "player car: never persistent damage");

        // CHAIN-RECOLOUR (FX-RCEM4): the two re-colour legs of the block.
        Check(lModule.maRaceCars[4].miColourIndex == 7 && lModule.miRandomColourCalls == 1
                  && lModule.miRandomColourPalette == 0 && lModule.miRandomColourCurrent == -1,
              "re-colour 0x822F4174: the capped clean rival is re-coloured -- GetRandomCarColour(palette, -1)");
        Check(lModule.maRaceCars[1].miColourIndex == 11, "re-colour: no wrap yet -> rival 1 keeps its colour");
        lModule.TakenDownBlock(&lSlot, 1u);
        Check(lModule.maRaceCars[1].mfPersistentDamage == 0.0f && lModule.maRaceCars[1].miColourIndex == 7
                  && lModule.miRandomColourCalls == 2,
              "re-colour 0x822F4298: the takedown that wraps past 1.0 re-colours the rival");
        lModule.mxFlags |= BrnGameState::GameModeParams::KU_FLAG_SET_OPPONENTS_TO_COPS;
        lModule.maRaceCars[6].mfPersistentDamage = 0.3f;          // three damaged rivals again (2, 3, 6)
        ActiveRaceCar lCop; lCop.mpRaceCar = &lModule.maRaceCars[5]; lCop.mbTakenDown = true;
        lModule.TakenDownBlock(&lCop, 5u);
        Check(lModule.maRaceCars[5].mfPersistentDamage == 0.0f
                  && lModule.maRaceCars[5].miColourIndex == KI_BLACK_CAR_COLOUR_INDEX && lModule.miRandomColourCalls == 2,
              "re-colour: SET_OPPONENTS_TO_COPS (1<<34) -> colour 6, no random draw");
        Check(lModule.maRaceCars[0].miColourIndex == 11, "re-colour: never the player");
    }

    Check(giAsserts == 0, "no asserts on the console-valid inputs");
    std::printf("PersistentDamage: %d checks, %d failures\n", liChecks, liFailures);
    return liFailures ? 1 : 0;
}
