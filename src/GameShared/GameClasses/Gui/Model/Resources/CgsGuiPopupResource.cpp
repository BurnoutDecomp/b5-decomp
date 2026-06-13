#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82851F08
//   (CgsGui::GuiPopupResource::FixDown)
//
// Inverse load-time relocation: walks `count` (this[1]) entry pointers stored at
// the base `this[0]`, subtracts `delta` from each, then rebases the base down by
// `delta`. Behaviour-faithful to the X360 pseudocode:
//
//   if (count > 0)
//       for i in [0, count):
//           entry = *(base + i)
//           if (lbDeep)            // a3: traverses entry's sub-list (see note)
//               <count-only walk of *(entry+104) items from entry+96>
//           *(base + i) -= delta
//   *base -= delta
//   return this
//
// NOTE: when `lbDeep` is set the original walks the entry's sub-array
// (`*(entry+104)` items starting at `entry+96`) but the loop body performs **no
// stores or side effects** in the recovered pseudocode — it only advances a
// counter/cursor. It is preserved here as a no-op guarded by lbDeep so the
// control-flow shape and the field reads are faithful, but it changes no state.

namespace CgsGui
{
    // Member names per burnout.wiki (GUI Popup -> GuiPopupResource). The wiki
    // documents offset 4 as two int16s (miPopupCount + miSizeOfPopupResource); the
    // recovered FixUp/FixDown pseudocode reads the count here, so its storage width
    // is left as the source dictates and only the name is adopted.
    struct GuiPopupResource
    {
        u32  mppPopupData;  // 0x00 GuiPopup** (rebased down in place)
        s32  miPopupCount;  // 0x04 number of popup entry pointers at *mppPopupData

        GuiPopupResource* FixDown(int liDelta, bool lbDeep);
    };

    GuiPopupResource* GuiPopupResource::FixDown(int liDelta, bool lbDeep)
    {
        if (miPopupCount > 0)
        {
            u32* lpaEntries = reinterpret_cast<u32*>(mppPopupData);
            for (s32 liEntry = 0; liEntry < miPopupCount; ++liEntry)
            {
                if (lbDeep)
                {
                    // Recovered as a side-effect-free traversal of the entry's
                    // sub-array; retained for control-flow fidelity only.
                    const u32 luEntry = lpaEntries[liEntry];
                    const s32 liSubCount = *reinterpret_cast<s32*>(luEntry + 104);
                    for (s32 liSub = 0; liSub < liSubCount; ++liSub)
                    {
                        // no observable effect in the source build
                    }
                }
                lpaEntries[liEntry] -= static_cast<u32>(liDelta);
            }
        }
        mppPopupData -= static_cast<u32>(liDelta);
        return this;
    }
}
