#include "GameSource/Gui/SatNav/BrnMainMap.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnGui::MainMapComponent - the sat-nav main-map screen component bodies.
//
// This TU currently reconstructs RecvEvent only. The remaining ledger functions
// (Construct, Update, SnapToLocation, ApplyZoom, SetZoom, SetStandardDefZoomParams)
// are held back because they depend on entities that are not yet recovered/homed:
//   - the hand-written VMX world-rect transform pipeline runs through sub_8245A080
//     (external/unknown vector helper) plus the still-todo CalculateViewPaddingOffset
//     and CalculatePositionedWorldRect methods (Update / SnapToLocation / ApplyZoom);
//   - SetZoom needs CgsGui::StateInterface::OutputGuiEvent<BrnGui::GuiAudioTriggerEvent>
//     and Update needs OutputViewState<GuiEventRenderMainMap> / BrnGui::MapTransform,
//     none of which have a reconstructed home;
//   - SetStandardDefZoomParams copies the two file-static zoom-scale float tables
//     (flt_82F259EC -> flt_82F259DC), whose .rodata float values are not present in the
//     dossier or the IDA export and must not be guessed.

namespace BrnGui
{
    // -------------------------------------------------------------------------
    // BrnGui::MainMapComponent::RecvEvent
    //
    // X360 ARTIST @0x82458370. Asserts the event is non-null, then -- on the
    // "set map active" event (type 213) whose first word is zero -- latches the
    // event's active byte into the embedded MapManager's mbEnabled flag, and
    // finally forwards the event to MapManager::RecvEvent unconditionally.
    //
    // The event payload is an opaque CgsModule::Event blob here (same treatment as
    // MapManager::RecvEvent): the first word is read as a u32 and the active flag is
    // the byte at +8, matching the asm's `lwz r11,0(event)` / `lbz r11,8(event)`.
    // -------------------------------------------------------------------------
    void MainMapComponent::RecvEvent(const CgsModule::Event* lpEvent, int32_t liEventType)
    {
        CGS_ASSERT(lpEvent != NULL, " invalid event passed ");

        // Event 213 with a zero leading word carries the map-active toggle in byte 8.
        const int32_t KI_SET_MAP_ACTIVE_EVENT = 213;
        if (liEventType == KI_SET_MAP_ACTIVE_EVENT &&
            *reinterpret_cast<const u32*>(lpEvent) == 0u)
        {
            mMapManager.mbEnabled =
                (*(reinterpret_cast<const u8*>(lpEvent) + 8) != 0u);
        }

        mMapManager.RecvEvent(lpEvent, liEventType);
    }
}
