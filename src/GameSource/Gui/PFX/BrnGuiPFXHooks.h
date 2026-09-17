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
// [FLAG PC bring-up] ON THE x64 HOST THE LINKS STAY FILE-RELATIVE OFFSETS. The console's
// FixUp adds the resource's load base to every slot because its heap is below 4 GB and a
// slot is a full pointer there. The PC's GUI resource bank is NOT below 4 GB (measured
// 2026-09-17: the bank base was 0x1668CD23AE0, and the truncated rebase wrote through
// 0x00000000423AB2D0 -- the first boot with the type registered crashed inside
// PFXHookBundleResourceType::FixUp). A 32-bit slot cannot hold that address, and the
// record strides are fixed by the shipped data, so the slots are left as the offsets the
// converter wrote and every reader resolves them against the bundle's own address through
// the accessors below. Same rule as [[serialized-slots-stay-32-bit]]; the relocation
// arithmetic is simply deferred from load time to read time.
//
// Layout / method set from the DecFIGS DWARF (BrnGuiPFXHooks.h). FixDown body
// recovered from BURNOUT_X360_ARTIST.XEX @ 0x8250AFD8, FixUp @ 0x8250B038.
namespace BrnGui
{
    const u32 KI_MAX_PFX_ID_LENGTH = 32;   // BrnGuiPFXHooks.h:31

    // BrnGuiPFXHooks.h:147 -- one authored post-FX GROUP: the fade envelope and which of
    // the six effect types it drives, each by the collection name of its AttribSys asset
    // (BloomData / VignetteData / DepthOfFieldData / BlurData / TintData2d take a key) and,
    // for the 3D tint, by the colour cube's 64-bit resource id. Offsets are the ones
    // PFXNodeFader::Initialise @0x82504378 reads: the six use-bytes at +16..+21, the five
    // names at +22/+54/+86/+118/+150, the id at +184 (192 bytes per group; the shipped
    // PFXHOOKS.PFX carries the id as one little-endian u64, verified against the four
    // COLOURCUBEDICTIONARY.BIN entries it names).
    struct PFXGroup
    {
        s32  miID;                        // BrnGuiPFXHooks.h:152  @ 0x00
        f32  mfFadeIn;                    // :155  @ 0x04
        f32  mfFadeOut;                   // :156  @ 0x08
        f32  mfDuration;                  // :157  @ 0x0C
        u8   mbUseBloom;                  // :162  @ 0x10
        u8   mbUseVignette;               // :163  @ 0x11
        u8   mbUseDepthOfField;           // :164  @ 0x12
        u8   mbUseBlur;                   // :165  @ 0x13
        u8   mbUseTint2D;                 // :166  @ 0x14
        u8   mbUseTint3D;                 // :167  @ 0x15
        char macBloomDataID[KI_MAX_PFX_ID_LENGTH];         // :170  @ 0x16
        char macVignetteDataID[KI_MAX_PFX_ID_LENGTH];      // :171  @ 0x36
        char macDepthOfFieldDataID[KI_MAX_PFX_ID_LENGTH];  // :172  @ 0x56
        char macBlurDataID[KI_MAX_PFX_ID_LENGTH];          // :173  @ 0x76
        char macTint2DDataID[KI_MAX_PFX_ID_LENGTH];        // :175  @ 0x96
        u8   maPadB6[2];                                   // alignment to the u64 at 0xB8
        u64  muTint3DResourceId;          // :176  @ 0xB8

        s32 GetID() const       { return miID; }          // :57
        f32 GetFadeIn() const   { return mfFadeIn; }      // :66
        f32 GetFadeOut() const  { return mfFadeOut; }     // :75
        f32 GetDuration() const { return mfDuration; }    // :84
    };

    // BrnGuiPFXHooks.h:189
    struct PFXHookNode
    {
        f32 mfStartTime;   // BrnGuiPFXHooks.h:191
        u32 mpGroup;       // BrnGuiPFXHooks.h:192 - relocatable PFXGroup* (32-bit; an OFFSET on PC)
    };

    // BrnGuiPFXHooks.h:204
    struct PFXHook
    {
        void Construct();
        void Construct(const char* lpacName);
        bool IsMenu() const { return mbIsMenu != 0; }   // :217, the `lbz +0x30` every arbitrator test reads
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
        u32  mpaNodes;                        // BrnGuiPFXHooks.h:249  @ 0x34 - relocatable PFXHookNode*[] (an OFFSET on PC)
        s32  miNodeCount;                     // BrnGuiPFXHooks.h:250  @ 0x38
    };

    // BrnGuiPFXHooks.h:265
    struct PFXHookBundle
    {
        s32 miHookCount;     // BrnGuiPFXHooks.h:268
        s32 miGroupCount;    // BrnGuiPFXHooks.h:269
        u32 mpaHooks;        // BrnGuiPFXHooks.h:271 - relocatable PFXHook*[] (an OFFSET on PC)
        u32 mpaGroups;       // BrnGuiPFXHooks.h:272 - relocatable PFXGroup*[] (an OFFSET on PC)
        u32 mSizeOfBundle;   // BrnGuiPFXHooks.h:274

        // ---- [FLAG PC bring-up] the read-time relocation (see the banner) ----------------
        // Every slot in the blob is an offset from THIS record's first byte -- the same
        // quantity the console's FixUp added its load base to. The console's readers write
        // `mpaHooks[i]->...`; these are that dereference with the base applied.
        const u8* BlobBase() const { return reinterpret_cast<const u8*>(this); }
        const u32* HookTable() const  { return reinterpret_cast<const u32*>(BlobBase() + mpaHooks); }
        const u32* GroupTable() const { return reinterpret_cast<const u32*>(BlobBase() + mpaGroups); }
        const PFXHook* GetHook(s32 liIndex) const
        {
            return reinterpret_cast<const PFXHook*>(BlobBase() + HookTable()[liIndex]);
        }
        const PFXGroup* GetGroup(s32 liIndex) const
        {
            return reinterpret_cast<const PFXGroup*>(BlobBase() + GroupTable()[liIndex]);
        }
        // `lpHook->mpaNodes[liIndex]` and `lpNode->mpGroup` on the console.
        const PFXHookNode* GetHookNode(const PFXHook& lrHook, s32 liIndex) const
        {
            const u32* lpaNodes = reinterpret_cast<const u32*>(BlobBase() + lrHook.mpaNodes);
            return reinterpret_cast<const PFXHookNode*>(BlobBase() + lpaNodes[liIndex]);
        }
        const PFXGroup* GetNodeGroup(const PFXHookNode& lrNode) const
        {
            return lrNode.mpGroup != 0
                ? reinterpret_cast<const PFXGroup*>(BlobBase() + lrNode.mpGroup) : 0;
        }
    };
}

#endif
