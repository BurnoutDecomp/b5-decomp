#ifndef BRN_GUI_PFX_HOOKS_H
#define BRN_GUI_PFX_HOOKS_H

#include "types.hpp"   // s32, u32, u8, f32, u64

// BrnGui PFX hook bundle layout. A serialised GUI post-FX hook bundle is a flat
// blob of PFXHookBundle -> PFXHook[] -> PFXHookNode[] -> PFXGroup[] linked by
// pointer tables. Every inter-record link is a native 32-bit pointer (DWARF
// BrnGuiPFXHooks.h:192/249/271/272); on disk it holds the file-relative offset, and the
// resource type relocates it by the load base when the blob is loaded (FixUp) and back
// when it is written (FixDown).
//
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
        bool GetUseBloom() const        { return mbUseBloom != 0; }          // :94
        bool GetUseVignette() const     { return mbUseVignette != 0; }       // :98
        bool GetUseDepthOfField() const { return mbUseDepthOfField != 0; }   // :102
        bool GetUseBlur() const         { return mbUseBlur != 0; }           // :106
        bool GetUseTint2D() const       { return mbUseTint2D != 0; }         // :110
        bool GetUseTint3D() const       { return mbUseTint3D != 0; }         // :114
    };

    // BrnGuiPFXHooks.h:189
    struct PFXHookNode
    {
        f32       mfStartTime;   // BrnGuiPFXHooks.h:191
        PFXGroup* mpGroup;       // BrnGuiPFXHooks.h:192
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
        PFXHookNode** mpaNodes;               // BrnGuiPFXHooks.h:249  @ 0x34
        s32  miNodeCount;                     // BrnGuiPFXHooks.h:250  @ 0x38
    };

    // BrnGuiPFXHooks.h:265
    struct PFXHookBundle
    {
        s32        miHookCount;     // BrnGuiPFXHooks.h:268
        s32        miGroupCount;    // BrnGuiPFXHooks.h:269
        PFXHook**  mpaHooks;        // BrnGuiPFXHooks.h:271
        PFXGroup** mpaGroups;       // BrnGuiPFXHooks.h:272
        size_t     mSizeOfBundle;   // BrnGuiPFXHooks.h:274
    };
}

#endif
