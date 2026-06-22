#ifndef BRN_GUI_PFX_HOOKS_H
#define BRN_GUI_PFX_HOOKS_H

#include "types.hpp"   // s32, u32, u8, f32, u64

// BrnGui PFX hook bundle layout. A serialised GUI post-FX hook bundle is a flat
// blob of PFXHookBundle -> PFXHook[] -> PFXHookNode[] -> PFXGroup[] linked by
// pointer tables. On X360 every inter-record link is a 32-bit pointer stored as a
// raw address, so on disk/in-buffer they are relocated by a base value when the
// blob is loaded (FixUp) and un-relocated back to file-relative offsets when it is
// written (FixDown). The link fields are therefore modelled as relocatable u32
// values, matching the X360 32-bit pointer width and the relocation arithmetic in
// the asm (member access stays by name; the value is the relocated address).
//
// Layout / method set from the DecFIGS DWARF (BrnGuiPFXHooks.h). FixDown body
// recovered from BURNOUT_X360_ARTIST.XEX @ 0x8250AFD8.
namespace BrnGui
{
    const u32 KI_MAX_PFX_ID_LENGTH = 32;   // BrnGuiPFXHooks.h:31

    // BrnGuiPFXHooks.h:189
    struct PFXHookNode
    {
        f32 mfStartTime;   // BrnGuiPFXHooks.h:191
        u32 mpGroup;       // BrnGuiPFXHooks.h:192 - relocatable PFXGroup* (32-bit)
    };

    // BrnGuiPFXHooks.h:204
    struct PFXHook
    {
        void Construct();
        void Construct(const char* lpacName);
        bool IsMenu() const;
        void SetMenu(bool lbMenu);
        void FixUp(u32 luBaseValue);     // BrnGuiPFXHooks.h:235
        void FixDown(u32 luBaseValue);   // BrnGuiPFXHooks.h:239

        char macName[KI_MAX_PFX_ID_LENGTH];   // BrnGuiPFXHooks.h:242  @ 0x00
        u32  muId;                            // BrnGuiPFXHooks.h:243  @ 0x20
        s32  miPriority;                      // BrnGuiPFXHooks.h:244  @ 0x24
        u32  meTransitionMode;                // BrnGuiPFXHooks.h:245  @ 0x28
        f32  mfTransitionTime;                // BrnGuiPFXHooks.h:246  @ 0x2C
        u8   mbIsMenu;                        // BrnGuiPFXHooks.h:247  @ 0x30
        u8   maPad53[3];                      // padding to align mpaNodes
        u32  mpaNodes;                        // BrnGuiPFXHooks.h:249  @ 0x34 - relocatable PFXHookNode*[]
        s32  miNodeCount;                     // BrnGuiPFXHooks.h:250  @ 0x38
    };

    // BrnGuiPFXHooks.h:265
    struct PFXHookBundle
    {
        s32 miHookCount;     // BrnGuiPFXHooks.h:268
        s32 miGroupCount;    // BrnGuiPFXHooks.h:269
        u32 mpaHooks;        // BrnGuiPFXHooks.h:271 - relocatable PFXHook*[]
        u32 mpaGroups;       // BrnGuiPFXHooks.h:272 - relocatable PFXGroup*[]
        u32 mSizeOfBundle;   // BrnGuiPFXHooks.h:274
    };
}

#endif
