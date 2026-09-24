// FX-FLOW (crash parity 2026-09-24, FOLLOWUPS item 14): the race-finish GUI records and their enum,
// read off the REAL headers.
//
//   BrnGui::EFinishType   DWARF BrnGuiEventTypeDefs.h:893 -- 1ST..8TH = 0..7, TIMED_OUT 8, WON 9,
//                         LOST 10, COUNT 11: the values X360 ModeManager::FinishCurrentMode
//                         @0x8234B978 writes into r30 for AddFinishedRaceEvent (asserts against 0 / 7
//                         @0x8234BABC / @0x8234BAE0; `li r30, 8 / 9 / 0xA / 0xB`).
//   GUI 371 GuiOvertakeEvent    DWARF :3875 {meActiveRaceCarIndex, u8 muNewPosition}, 8 bytes
//   GUI 372 GuiFinishRaceEvent  DWARF :3892 {meActiveRaceCarIndex, EFinishType meFinishType}, 8 bytes
//                         (AddGuiEvent<T> @0x823D9DB0 / @0x823D9E68 `li r6, 8`; the producer
//                         TranslateGuiInterfaceToGuiEvents @0x823E1D90 stores the slot @+0 and the
//                         position byte / finish type @+4, 0x823E2040..4C / 0x823E20F8..0x823E2104).
// Before this, the enum held one invented enumerator (E_FINISH_TYPE_NONE) and both GUI records were
// opaque u8[8] placeholders; the translator spelt the real layouts in TU-local wire records.
//
// The runner writes: finish_type.inc (the enum), gui_records.inc (the two BrnGui structs, from
// wherever the revision defines them), wire_records.inc (the translator's two wire records, from
// BrnGameModule.cpp) and enum_checks.inc (one Check per DWARF enumerator; a missing name is a
// failing Check, not a compile error).
#include "types.hpp"
#include "GameSource/BurnoutConstants.h"   // ::EActiveRaceCarIndex
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <type_traits>

namespace BrnGui
{
#include "finish_type.inc"
#include "gui_records.inc"
}

namespace wire
{
#include "wire_records.inc"
}

static unsigned gChecks = 0, gFailures = 0;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("  FAIL %s\n", lpcName);
    }
    else
    {
        std::printf("  ok   %s\n", lpcName);
    }
}

// Member detection: a placeholder record without the DWARF field fails the check instead of
// failing the build.
template <class T, class = void> struct HasSlot : std::false_type {};
template <class T> struct HasSlot<T, std::void_t<decltype(&T::meActiveRaceCarIndex)>> : std::true_type {};
template <class T, class = void> struct HasPosition : std::false_type {};
template <class T> struct HasPosition<T, std::void_t<decltype(&T::muNewPosition)>> : std::true_type {};
template <class T, class = void> struct HasFinishType : std::false_type {};
template <class T> struct HasFinishType<T, std::void_t<decltype(&T::meFinishType)>> : std::true_type {};

template <class T> static int SlotOffset()
{
    if constexpr (HasSlot<T>::value) return static_cast<int>(offsetof(T, meActiveRaceCarIndex));
    else return -1;
}
template <class T> static int PositionOffset()
{
    if constexpr (HasPosition<T>::value) return static_cast<int>(offsetof(T, muNewPosition));
    else return -1;
}
template <class T> static int FinishTypeOffset()
{
    if constexpr (HasFinishType<T>::value) return static_cast<int>(offsetof(T, meFinishType));
    else return -1;
}

// The queue copies the posted record's bytes; a consumer reads them back through the BrnGui type.
template <class T, class W> static T Posted(const W& lrWire)
{
    static_assert(sizeof(T) >= sizeof(W), "reading a posted record through a smaller type");
    T lRecord;
    std::memset(&lRecord, 0xCD, sizeof(lRecord));
    std::memcpy(&lRecord, &lrWire, sizeof(W));
    return lRecord;
}

// Templates, so a placeholder record's discarded branch is never instantiated.
template <class T> static bool OvertakeRoundTrip()
{
    wire::GuiOvertakeEventWire371 lWire;
    std::memset(&lWire, 0, sizeof(lWire));
    lWire.meActiveRaceCarIndex = static_cast< ::EActiveRaceCarIndex>(5);
    lWire.muNewPosition = 3;
    if constexpr (HasSlot<T>::value && HasPosition<T>::value)
    {
        const T lRead = Posted<T>(lWire);
        return static_cast<int>(lRead.meActiveRaceCarIndex) == 5 && lRead.muNewPosition == 3;
    }
    else
    {
        return false;
    }
}

template <class T> static bool FinishRoundTrip()
{
    wire::GuiFinishRaceEventWire372 lWire;
    std::memset(&lWire, 0, sizeof(lWire));
    lWire.meActiveRaceCarIndex = static_cast< ::EActiveRaceCarIndex>(6);
    lWire.meFinishType = static_cast<BrnGui::EFinishType>(9);
    if constexpr (HasSlot<T>::value && HasFinishType<T>::value)
    {
        const T lRead = Posted<T>(lWire);
        return static_cast<int>(lRead.meActiveRaceCarIndex) == 6 && static_cast<int>(lRead.meFinishType) == 9;
    }
    else
    {
        return false;
    }
}

int main()
{
#include "enum_checks.inc"

    const BrnGui::GuiOvertakeEvent lOvertake = {};
    std::printf("  GuiOvertakeEvent: sizeof %d, slot @%d, position @%d, id %d\n",
                static_cast<int>(sizeof(BrnGui::GuiOvertakeEvent)), SlotOffset<BrnGui::GuiOvertakeEvent>(),
                PositionOffset<BrnGui::GuiOvertakeEvent>(), static_cast<int>(lOvertake.GetEventType()));
    Check(SlotOffset<BrnGui::GuiOvertakeEvent>() == 0,
          "GuiOvertakeEvent::meActiveRaceCarIndex @+0 (DWARF :3878; `stw` slot @0x823E204C)");
    Check(PositionOffset<BrnGui::GuiOvertakeEvent>() == 4,
          "GuiOvertakeEvent::muNewPosition @+4 (DWARF :3879; `stb` position @0x823E2044)");
    Check(sizeof(BrnGui::GuiOvertakeEvent) == 8, "sizeof(GuiOvertakeEvent) == 8 (`li r6, 8` @0x823D9E4C)");
    Check(lOvertake.GetEventType() == 371, "GuiOvertakeEvent is GUI 371 (`li r5, 0x173` @0x823D9E50)");

    const BrnGui::GuiFinishRaceEvent lFinish = {};
    std::printf("  GuiFinishRaceEvent: sizeof %d, slot @%d, finish type @%d, id %d\n",
                static_cast<int>(sizeof(BrnGui::GuiFinishRaceEvent)), SlotOffset<BrnGui::GuiFinishRaceEvent>(),
                FinishTypeOffset<BrnGui::GuiFinishRaceEvent>(), static_cast<int>(lFinish.GetEventType()));
    Check(SlotOffset<BrnGui::GuiFinishRaceEvent>() == 0,
          "GuiFinishRaceEvent::meActiveRaceCarIndex @+0 (DWARF :3895; `stw` slot @0x823E2104)");
    Check(FinishTypeOffset<BrnGui::GuiFinishRaceEvent>() == 4,
          "GuiFinishRaceEvent::meFinishType @+4 (DWARF :3896; `stw` type @0x823E20FC)");
    Check(sizeof(BrnGui::GuiFinishRaceEvent) == 8, "sizeof(GuiFinishRaceEvent) == 8 (AddGuiEvent @0x823D9E68)");
    Check(lFinish.GetEventType() == 372, "GuiFinishRaceEvent is GUI 372 (AddGuiEvent @0x823D9E68)");

    Check(OvertakeRoundTrip<BrnGui::GuiOvertakeEvent>(),
          "a posted 371 wire record {slot 5, place 3} reads back as GuiOvertakeEvent {5, 3}");
    Check(FinishRoundTrip<BrnGui::GuiFinishRaceEvent>(),
          "a posted 372 wire record {slot 6, WON} reads back as GuiFinishRaceEvent {6, 9}");

    std::printf("FxFlowGuiRaceRecords: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
