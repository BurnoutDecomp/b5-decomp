#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiHudMessage.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82846528
//   (CgsGui::GuiHudMessageResource::FixUp)
//
// Behaviour-faithful to the X360 pseudocode. FixUp is a load-time pointer
// relocation: it rebases the resource's message-table pointer by `delta`, then walks
// the `count` entries that begin at that rebased base and adds `delta` to each.
//
//   int v3 = result[2];              // miHudMessageCount
//   *result += a2;                   // mppHudMessageData += delta
//   for (i = 0; i < result[2]; ++i)  // v4 steps by 4 == the CONSOLE pointer width
//       *(u32*)(*result + v4) += a2;
//   return result;
//
// (The pseudocode re-reads result[2] as the loop bound each iteration; count is not
// mutated, so a fixed bound is equivalent.)
//
// ⭐ [gateui r4] CE-3: THIS TU NO LONGER FORKS THE TYPE. Until round 4 it declared its
// OWN `namespace CgsGui { struct GuiHudMessageResource { u32 mppHudMessageData; ... } }`
// with a 4-byte pointer slot, while `CgsGuiHudMessage.h` -- the declaration
// `BrnHudMessageController.h` and every real consumer sees -- declares the same
// fully-qualified type with an 8-byte `GuiHudMessageData**`. Two definitions of one
// class in one program (verify_r3_fix3hud NOTE-6). The HEADER is the correct one: the
// shipped `build/game/HUDMESSAGES.HM` was measured byte for byte and its slot at the
// resource base is an EIGHT-byte one (`80 00 00 00 00 00 00 00`), with the 250 record
// offsets that follow spaced by the HOST `sizeof(GuiHudMessageData)` == 0x170. So the
// file is already transcoded to native-8 and the console's `v4 += 4` stride becomes the
// host pointer stride here -- expressed as an array subscript on the real member type,
// never as a byte count.
//
// The delta is likewise widened: on the console it is a 32-bit `int` only because its
// pointers are. See CgsGuiHudMessageType.cpp, which now passes GetLoadBase64 (the
// StreetDataResourceType / LanguageResourceType precedent).

namespace CgsGui
{
    GuiHudMessageResource* GuiHudMessageResource::FixUp(uintptr_t luDelta)
    {
        const s32 liCount = miHudMessageCount;

        // `*result += a2` -- the stored value is a serialised FILE OFFSET, not yet a
        // pointer, so the rebase is deliberately pointer arithmetic on the raw value.
        mppHudMessageData = reinterpret_cast<GuiHudMessageData**>(
            reinterpret_cast<uintptr_t>(mppHudMessageData) + luDelta);

        for (s32 liEntry = 0; liEntry < liCount; ++liEntry)
        {
            mppHudMessageData[liEntry] = reinterpret_cast<GuiHudMessageData*>(
                reinterpret_cast<uintptr_t>(mppHudMessageData[liEntry]) + luDelta);
        }

        return this;
    }
}
