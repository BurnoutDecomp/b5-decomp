// BrnGuiRaceCarInfoEvent.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX. BrnGui::GuiRaceCarInfoEvent::Construct
// (@0x823A7658) zero-initialises the fixed set of 8 per-active-race-car entries (the
// 16-byte position lanes, the identity qwords, the entry count, and the three per-entry
// flag-byte arrays). The 16-byte VMX zero-store is reproduced as a scalar Vector4 zero
// per the project's established VMX-as-scalar reconstruction (endian-independent;
// see BrnGuiEventTypeDefs.cpp DoWorstCase precedent).
//
// DoWorstCase (@0x823A6BB8) is NOT reconstructed here: it is a heavy per-entry VMX
// vmaddfp128 fused-multiply-add position transform. The TU is BLOCKED on it (an honest
// VMX-math boundary) and the method is left declared-only in the header.

#include "GameSource/Gui/BrnGuiRaceCarInfoEvent.h"

namespace BrnGui
{

// @ 0x823A7658
//   for (i=0; i<8; ++i) {
//       stvx128 0   @ maPosition[i]   (16-byte zero)
//       std     0   @ maIdentity[i]   (8-byte zero)
//       stb     0   @ maFlagA[i]      (byte zero)
//       stb     0   @ maFlagB[i]      (byte zero)
//       stb     0   @ maFlagC[i]      (byte zero)
//   }
//   stw 0 @ miNumEntries
GuiRaceCarInfoEvent* GuiRaceCarInfoEvent::Construct()
{
    for ( s32 liIndex = 0; liIndex < KI_NUM_ENTRIES; ++liIndex )
    {
        maPosition[liIndex].x = 0.0f;
        maPosition[liIndex].y = 0.0f;
        maPosition[liIndex].z = 0.0f;
        maPosition[liIndex].w = 0.0f;
        maIdentity[liIndex] = 0;
        maFlagA[liIndex] = 0;
        maFlagB[liIndex] = 0;
        maFlagC[liIndex] = 0;
    }

    miNumEntries = 0;
    return this;
}

} // namespace BrnGui
