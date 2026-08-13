#include "GameShared/GameClasses/Graphics/CgsBlendStateFactory.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Graphics/CgsResourceAllocatorCreate.h"
#include "pc/gcm/renderengine/renderstates.h"   // renderengine::ResourceDescriptor5

// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   CgsBlendStateFactory::Construct  @ 0x827EB2D8 .. 0x827EBBA0  (2248 bytes, 562 instructions)
//
// Builds the fixed set of nine blend states the renderer, the post-fx chain and the
// dispatch interpreter select between. Only Construct is X360-attested for this TU;
// Destruct / Prepare have no body anywhere in the image and GetState is inlined into
// its readers (see the header).
//
// -----------------------------------------------------------------------------
// LINK. This TU is NOT in tools/build/build_game_exe.bat, and BrnRendererModule.h is
// deliberately NOT changed to include the header, so nothing in the boot link
// constructs a CgsBlendStateFactory and no vtable for it is emitted there. Two
// reasons, both load-bearing:
//
//   1. Mounting the class into BrnRendererModule (which embeds it BY VALUE, and which
//      is reached from `static BrnGame::BrnGameModule gGameModule;` in BrnMain.cpp:45)
//      makes the vtable live in the linked exe and requires every virtual to be
//      defined in that link. All three of this class's virtuals ARE defined -- Construct
//      below, Destruct and Prepare in CgsStateFactoryLinkStubs.cpp -- but the sibling
//      CgsRasterizerStateFactory still declares itself TU-locally and non-polymorphic
//      in its own .cpp, so the three-header swap cannot be completed honestly yet.
//   2. Mounting it would buy nothing today anyway: NOTHING calls
//      BrnRendererModule::Construct -> mBlendStateFactory.Construct on this build, so
//      every slot would still read null, and a null slot handed to the frame bracket's
//      blend third is a live null dereference (the bracket compares wanted against
//      ImRendererBase::mgpLastState, which any immediate-mode draw leaves non-null, so
//      the inequality fires and shadow::Device::Xbox2SetStateLowLevelShadowed reads
//      lpu[0] with no null guard -- shadowingdevice.cpp:535-537 / 568-570).
//      The table becomes usable when something CALLS Construct, not when the class is
//      embedded.
// =============================================================================

// DWARF CgsBlendStateFactory.cpp:25 -- the definition of the private static table
// declared at CgsBlendStateFactory.h:57. X360 saBlendStates[0..8] ==
// 0x83010F70 .. 0x83010F90 (nine consecutive dwords; all nine read 0x00000000 in the
// image, i.e. the table is zero-initialised storage and Construct is what fills it).
renderengine::BlendMaterialState* CgsBlendStateFactory::saBlendStates[E_FACTORY_BLEND_STATE_COUNT] = {};

namespace
{
    // The packed per-channel blend word every one of the nine parameter blocks starts
    // from. X360 builds it once into r25 and splats it across all four channels of every
    // block: `lis r11, 0x706` @0x827EB32C + `ori r25, r11, 0x706` @0x827EB334.
    // Same constant, same role, as CgsImRendererBlendState.cpp's KU_BLEND_FACTOR_WORD.
    const u32 KU_BLEND_FACTOR_DEFAULT = 0x07060706u;

    // The rw resource allocator's Create slot; vtable offset +0x10 on X360. Construct
    // reaches the allocator through exactly one indirect call --
    //     0x827EB3B4  lwz   r11, 0(r29)        ; r29 = the allocator argument (r4)
    //     0x827EB3C8  lwz   r11, 0x10(r11)     ; vtable +0x10
    //     0x827EB3D0  bctrl                    ; (out, allocator, descriptor, 0)
    // -- which is the same call the two sibling factories and the immediate-mode
    // builders make.
    class ResourceAllocator
    {
    public:
        // NOT a vtable slot. Declaring this `virtual` put it at slot 0, which on the
        // rw::IResourceAllocator actually behind the reinterpret_cast is the VIRTUAL
        // DESTRUCTOR -- so the call allocated nothing and left the allocator's vptr
        // downgraded to the inert base for the rest of the run. Call the interface by
        // NAME instead; see CgsResourceAllocatorCreate.h.
        void* Create(
            void* lpStateHandlesOut,
            ResourceAllocator* /*lpAllocator*/,
            const void* lpDescriptor,
            int /*liFlags*/)
        {
            return CgsGraphics::ResourceAllocatorCreate(this, lpStateHandlesOut, lpDescriptor);
        }
    };

    // Carve the backing store for one blend state through the supplied resource
    // allocator, then initialise it. This is the block the X360 compiler INLINED nine
    // times (each inlining keeps its own 32-byte out buffer; the five-word handle array
    // is shared and zeroed once at 0x827EB2FC..0x827EB318). Re-rolled here, exactly the
    // shape the sibling CgsRasterizerStateFactory.cpp's CreateRasterizerState already
    // has:
    //     GetResourceDescriptor  -> allocator vtable +0x10 -> copy 5 handle words
    //                            -> BlendState::Initialize
    renderengine::BlendMaterialState* CreateBlendState(
        ResourceAllocator* lpAllocator,
        const renderengine::BlendStateParameters* lpParameters)
    {
        renderengine::ResourceDescriptor5 lDescriptor;
        renderengine::BlendState::GetResourceDescriptor(&lDescriptor, lpParameters);

        // FLAG PC-platform choice: the console leaves the out buffer uninitialised and
        // copies all five words out of it regardless. The zero-initialisation here is the
        // convention CgsResourceAllocatorCreate.h documents (the helper writes only the
        // first KU_CREATE_OUT_LANES lanes, so the lanes past the third stay null rather
        // than becoming garbage). No observable difference: BlendState::Initialize reads
        // lane 0 only.
        renderengine::BlendMaterialState* lapAllocatedHandles[5] = {};
        renderengine::BlendMaterialState* lapStateHandles[5] = {};

        lpAllocator->Create(lapAllocatedHandles, lpAllocator, &lDescriptor, 0);

        // 0x827EB3D4..0x827EB3F4: li r11,5 / mtctr / lwz,stw,addi 4 / bdnz -- the five-word
        // copy out of the allocator's out buffer into the handle array Initialize is given.
        for (int liHandle = 0; liHandle < 5; ++liHandle)
            lapStateHandles[liHandle] = lapAllocatedHandles[liHandle];

        return static_cast<renderengine::BlendMaterialState*>(
            renderengine::BlendState::Initialize(lapStateHandles, lpParameters));
    }
}

// @ 0x827EB2D8 -- clear the nine-slot table, then build all nine blend states into it,
// asserting each slot came back non-null.
//
// SIGNATURE. `virtual void Construct(rw::IResourceAllocator*)` per the DWARF
// (CgsBlendStateFactory.h:36 / .cpp:44), and the asm agrees on both counts that matter:
// the allocator arrives in r4 while r3 is never read (an unused implicit `this`, so the
// function is a non-static member), and no function in the export set calls 0x827EB2D8
// directly (vtable dispatch, so nothing consumes r3 as a return value either --
// Hex-Rays' trailing `result` is the value CgsDev::Assert::EndAssert happened to leave
// in r3 on the last assert path).
//
// THE PARAMETER BLOCKS. All nine share one renderengine::BlendStateParameters shape and
// differ in only SIX fields, so the invariant thirteen are set once and the six varying
// ones are set explicitly at every slot (no value is left to carry over from the
// previous slot -- the console rebuilds the whole block each time, into its own stack
// slot). The invariant part is field-for-field identical to the one
// CgsGraphics::ImRendererBase::ConstructBlendState @0x827ED118 builds
// (CgsImRendererBlendState.cpp:76-91), which is an independent corroboration of this
// decode of the block's layout.
void CgsBlendStateFactory::Construct(rw::IResourceAllocator* lpAllocator)
{
    // 0x827EB2EC..0x827EB328: the table base is formed once and a nine-iteration loop
    // (li r9,9 @0x827EB308) stores 0 through it, four bytes at a time.
    for (u32 luSlot = 0; luSlot < E_FACTORY_BLEND_STATE_COUNT; ++luSlot)
        saBlendStates[luSlot] = nullptr;

    // The allocator is passed straight through to the TU-local single-entry-point shim.
    // reinterpret_cast, not static_cast, because the shim is not related to
    // rw::IResourceAllocator by inheritance; the POINTER VALUE is unchanged (the shim has
    // no vtable and no bases) and ResourceAllocatorCreate casts it straight back.
    ResourceAllocator* lpAllocatorShim = reinterpret_cast<ResourceAllocator*>(lpAllocator);

    // ---- the thirteen fields every one of the nine blocks sets identically ----------
    renderengine::BlendStateParameters lParameters = {};
    lParameters.maBlendFactor[1] = KU_BLEND_FACTOR_DEFAULT;   // stw r25 @+0x04, every block
    lParameters.maBlendFactor[2] = KU_BLEND_FACTOR_DEFAULT;   // stw r25 @+0x08
    lParameters.maBlendFactor[3] = KU_BLEND_FACTOR_DEFAULT;   // stw r25 @+0x0C
    lParameters.muState5         = 15u;                       // r30 = 0xF  @0x827EB340
    lParameters.muState6         = 15u;                       // r30
    lParameters.muState7         = 15u;                       // r30
    lParameters.muState8         = 135u;                      // r26 = 0x87 @0x827EB344
    lParameters.muState9         = 0xFFFFFFFFu;               // r28 = -1   @0x827EB348
    lParameters.mbState10        = 0u;                        // r31 = 0
    lParameters.mbState11        = 0u;
    lParameters.mbState12        = 0u;
    lParameters.mbState13        = 0u;
    lParameters.mbState14        = 0u;

    // ---- slot 0 -- Opaque_Modulate_NoAlphaTest_DestRGBA -----------------------------
    // Block 0x827EB32C..0x827EB3AC. maBlendFactor[0] is the splatted default with its low
    // 16 bits re-inserted from r27 = 0x383 (`li r27,0x383` @0x827EB338,
    // `rlwimi r11,r27,1,16,31` @0x827EB370): (0x383 << 1) = 0x0706, which is what the low
    // half already held, so the stored word is unchanged at 0x07060706.
    // mbHasCustomBlendFactors = 0 (`stb r31, var_2B0` @0x827EB3A0), so
    // BlendState::Initialize writes its KU_DEFAULT_BLEND_FACTOR into maState[0..3] and
    // these four words are dead -- the console stores them anyway, and so does this.
    lParameters.maBlendFactor[0]        = 0x07060706u;
    lParameters.mbHasCustomBlendFactors = 0u;
    lParameters.muState4                = 15u;   // r30, stw @0x827EB3AC
    lParameters.muState15               = 7u;    // r24 = 7 @0x827EB34C, stw @0x827EB398
    lParameters.muState17               = 0u;    // r31,  stw @0x827EB39C
    lParameters.mbState16               = 0u;    // r31,  stb @0x827EB3A4
    saBlendStates[E_FACTORY_BLEND_STATE_OPAQUE_MODULATE_NO_ALPHA_TEST_DEST_RGBA] =
        CreateBlendState(lpAllocatorShim, &lParameters);                   // stw @0x827EB408
    CGS_ASSERT(saBlendStates[E_FACTORY_BLEND_STATE_OPAQUE_MODULATE_NO_ALPHA_TEST_DEST_RGBA],
               "saBlendStates[ eFactoryBlendState_Opaque_Modulate_NoAlphaTest_DestRGBA ]");

    // ---- slot 1 -- Transparent_Modulate_NoAlphaTest_DestRGBA ------------------------
    // Block 0x827EB434..0x827EB49C. Identical to slot 0 except that
    // mbHasCustomBlendFactors is now 1 (`stb r27, var_370` @0x827EB490, with r27 reloaded
    // to 1 at 0x827EB464), so the four factor words below are the ones that reach the
    // object. The word itself is the same rlwimi of 0x383 -> 0x07060706 (@0x827EB45C).
    lParameters.maBlendFactor[0]        = 0x07060706u;
    lParameters.mbHasCustomBlendFactors = 1u;
    lParameters.muState4                = 15u;
    lParameters.muState15               = 7u;
    lParameters.muState17               = 0u;
    lParameters.mbState16               = 0u;
    saBlendStates[E_FACTORY_BLEND_STATE_TRANSPARENT_MODULATE_NO_ALPHA_TEST_DEST_RGBA] =
        CreateBlendState(lpAllocatorShim, &lParameters);                   // stw @0x827EB4F4
    CGS_ASSERT(saBlendStates[E_FACTORY_BLEND_STATE_TRANSPARENT_MODULATE_NO_ALPHA_TEST_DEST_RGBA],
               "saBlendStates[ eFactoryBlendState_Transparent_Modulate_NoAlphaTest_DestRGBA ]");

    // ---- slot 2 -- Transparent_Additive_NoAlphaTest_DestRGBA ------------------------
    // Block 0x827EB51C..0x827EB584. `li r9,0x83` @0x827EB524 and
    // `rlwimi r11,r9,1,16,31` @0x827EB548 replace the low 16 bits with (0x83 << 1) =
    // 0x0106, giving 0x07060106.
    lParameters.maBlendFactor[0]        = 0x07060106u;
    lParameters.mbHasCustomBlendFactors = 1u;
    lParameters.muState4                = 15u;
    lParameters.muState15               = 7u;
    lParameters.muState17               = 0u;
    lParameters.mbState16               = 0u;
    saBlendStates[E_FACTORY_BLEND_STATE_TRANSPARENT_ADDITIVE_NO_ALPHA_TEST_DEST_RGBA] =
        CreateBlendState(lpAllocatorShim, &lParameters);                   // stw @0x827EB5DC
    CGS_ASSERT(saBlendStates[E_FACTORY_BLEND_STATE_TRANSPARENT_ADDITIVE_NO_ALPHA_TEST_DEST_RGBA],
               "saBlendStates[ eFactoryBlendState_Transparent_Additive_NoAlphaTest_DestRGBA ]");

    // ---- slot 3 -- Transparent_Subtractive_NoAlphaTest_DestRGBA ---------------------
    // Block 0x827EB604..0x827EB66C. `li r9,0x93` @0x827EB60C, `rlwimi r11,r9,1,16,31`
    // @0x827EB630: (0x93 << 1) = 0x0126 -> 0x07060126.
    lParameters.maBlendFactor[0]        = 0x07060126u;
    lParameters.mbHasCustomBlendFactors = 1u;
    lParameters.muState4                = 15u;
    lParameters.muState15               = 7u;
    lParameters.muState17               = 0u;
    lParameters.mbState16               = 0u;
    saBlendStates[E_FACTORY_BLEND_STATE_TRANSPARENT_SUBTRACTIVE_NO_ALPHA_TEST_DEST_RGBA] =
        CreateBlendState(lpAllocatorShim, &lParameters);                   // stw @0x827EB6C4
    CGS_ASSERT(saBlendStates[E_FACTORY_BLEND_STATE_TRANSPARENT_SUBTRACTIVE_NO_ALPHA_TEST_DEST_RGBA],
               "saBlendStates[ eFactoryBlendState_Transparent_Subtractive_NoAlphaTest_DestRGBA ]");

    // ---- slot 4 -- Transparent_AdditiveAlphaOne_NoAlphaTest_DestRGBA ----------------
    // Block 0x827EB6EC..0x827EB754. `li r9,0x101` @0x827EB6F4 and
    // `insrwi r11,r9,16,16` @0x827EB718 -- an insert of the LOW 16 BITS as-is (not
    // shifted), giving 0x07060101.
    lParameters.maBlendFactor[0]        = 0x07060101u;
    lParameters.mbHasCustomBlendFactors = 1u;
    lParameters.muState4                = 15u;
    lParameters.muState15               = 7u;
    lParameters.muState17               = 0u;
    lParameters.mbState16               = 0u;
    saBlendStates[E_FACTORY_BLEND_STATE_TRANSPARENT_ADDITIVE_ALPHA_ONE_NO_ALPHA_TEST_DEST_RGBA] =
        CreateBlendState(lpAllocatorShim, &lParameters);                   // stw @0x827EB7AC
    CGS_ASSERT(saBlendStates[E_FACTORY_BLEND_STATE_TRANSPARENT_ADDITIVE_ALPHA_ONE_NO_ALPHA_TEST_DEST_RGBA],
               "saBlendStates[ eFactoryBlendState_Transparent_AdditiveAlphaOne_NoAlphaTest_DestRGBA ]");

    // ---- slot 5 -- Transparent_AdditiveRGB_NoAlphaTest_DestRGB ----------------------
    // Block 0x827EB7D4..0x827EB860. The only slot whose word-0 is built by a sequence
    // rather than one insert; every step below is a store to the same stack word, and
    // nothing reads it until GetResourceDescriptor, so only the FINAL value is
    // observable:
    //     start                            0x07060706   (splatted default)
    //     rlwimi r11,r9(=3),1,27,31  @0x827EB800  low 5 bits <- 6 : unchanged
    //     stb r27(=1), +2            @0x827EB81C  byte 2 (big-endian) <- 1 : 0x07060106
    //     rlwinm r11,r11,0,27,23     @0x827EB828  & ~0x000000E0
    //     rlwinm r11,r11,0,16,10     @0x827EB834  & ~0x001F0000  (together & 0xFFE0FF1F)
    //                                             -> 0x07000106
    //     stb r27(=1), +0            @0x827EB850  byte 0 (MSB)  <- 1 : 0x01000106
    //     rlwinm r11,r11,0,11,7      @0x827EB85C  & ~0x00E00000 : unchanged
    //     FINAL                            0x01000106
    // (The three mask immediates 0x000000E0 / 0x001F0000 / 0x00E00000 are contiguous,
    // non-overlapping bit runs, so this word is a packed multi-field register and those
    // are three of its field boundaries. What the fields MEAN is not claimed here.)
    lParameters.maBlendFactor[0]        = 0x01000106u;
    lParameters.mbHasCustomBlendFactors = 1u;                              // stb @0x827EB844
    lParameters.muState4                = 15u;                             // stw @0x827EB858
    lParameters.muState15               = 7u;                              // stw @0x827EB83C
    lParameters.muState17               = 0u;                              // stw @0x827EB840
    lParameters.mbState16               = 0u;                              // stb @0x827EB848
    saBlendStates[E_FACTORY_BLEND_STATE_TRANSPARENT_ADDITIVE_RGB_NO_ALPHA_TEST_DEST_RGB] =
        CreateBlendState(lpAllocatorShim, &lParameters);                   // stw @0x827EB8B8
    CGS_ASSERT(saBlendStates[E_FACTORY_BLEND_STATE_TRANSPARENT_ADDITIVE_RGB_NO_ALPHA_TEST_DEST_RGB],
               "saBlendStates[ eFactoryBlendState_Transparent_AdditiveRGB_NoAlphaTest_DestRGB ]");

    // ---- slot 6 -- Transparent_AdditiveInvDestColor_NoAlphaTest_DestRGBA ------------
    // Block 0x827EB8E0..0x827EB948. `li r9,0x109` @0x827EB8E8, `insrwi r11,r9,16,16`
    // @0x827EB90C -> 0x07060109.
    lParameters.maBlendFactor[0]        = 0x07060109u;
    lParameters.mbHasCustomBlendFactors = 1u;
    lParameters.muState4                = 15u;
    lParameters.muState15               = 7u;
    lParameters.muState17               = 0u;
    lParameters.mbState16               = 0u;
    saBlendStates[E_FACTORY_BLEND_STATE_TRANSPARENT_ADDITIVE_INV_DEST_COLOR_NO_ALPHA_TEST_DEST_RGBA] =
        CreateBlendState(lpAllocatorShim, &lParameters);                   // stw @0x827EB9A0
    CGS_ASSERT(saBlendStates[E_FACTORY_BLEND_STATE_TRANSPARENT_ADDITIVE_INV_DEST_COLOR_NO_ALPHA_TEST_DEST_RGBA],
               "saBlendStates[ eFactoryBlendState_Transparent_AdditiveInvDestColor_NoAlphaTest_DestRGBA ]");

    // ---- slot 7 -- NoColourWrite_NoAlphaTest ----------------------------------------
    // Block 0x827EB9C8..0x827EBA2C. `clrrwi r11,r11,16` @0x827EB9F0 clears the low half
    // of the splatted default -> 0x07060000. This is the first slot to set muState4
    // (ColorWriteEnable) to 0 (`stw r31, var_20C` @0x827EBA2C) -- which is exactly what
    // its name says -- and mbHasCustomBlendFactors goes back to 0
    // (`stb r31, var_1F0` @0x827EBA20).
    lParameters.maBlendFactor[0]        = 0x07060000u;
    lParameters.mbHasCustomBlendFactors = 0u;
    lParameters.muState4                = 0u;
    lParameters.muState15               = 7u;    // r24, stw @0x827EBA18
    lParameters.muState17               = 0u;    // r31, stw @0x827EBA1C
    lParameters.mbState16               = 0u;    // r31, stb @0x827EBA24
    saBlendStates[E_FACTORY_BLEND_STATE_NO_COLOUR_WRITE_NO_ALPHA_TEST] =
        CreateBlendState(lpAllocatorShim, &lParameters);                   // stw @0x827EBA84
    CGS_ASSERT(saBlendStates[E_FACTORY_BLEND_STATE_NO_COLOUR_WRITE_NO_ALPHA_TEST],
               "saBlendStates[ eFactoryBlendState_NoColourWrite_NoAlphaTest ]");

    // ---- slot 8 -- NoColourWrite_AlphaTest ------------------------------------------
    // Block 0x827EBAAC..0x827EBB18. Same 0x07060000 word (`clrrwi` @0x827EBB04) and the
    // same muState4 = 0 (`stw r31, var_24C` @0x827EBB14), and it is the ONLY slot that
    // turns the alpha test on: mbState16 = 1 (`stb r27, var_22A` @0x827EBB10), with
    // muState15 = 4 (`li r11,4` @0x827EBAE4, `stw` @0x827EBAF8) and muState17 = 0x80
    // (`li r11,0x80` @0x827EBACC, `stw` @0x827EBAE0). Per blendstate.h those three land
    // in maState[15] AlphaFunc, maState[16] AlphaTestEnable and maState[17] AlphaRef.
    lParameters.maBlendFactor[0]        = 0x07060000u;
    lParameters.mbHasCustomBlendFactors = 0u;                              // stb @0x827EBB0C
    lParameters.muState4                = 0u;
    lParameters.muState15               = 4u;
    lParameters.muState17               = 0x80u;
    lParameters.mbState16               = 1u;
    saBlendStates[E_FACTORY_BLEND_STATE_NO_COLOUR_WRITE_ALPHA_TEST] =
        CreateBlendState(lpAllocatorShim, &lParameters);                   // stw @0x827EBB70
    CGS_ASSERT(saBlendStates[E_FACTORY_BLEND_STATE_NO_COLOUR_WRITE_ALPHA_TEST],
               "saBlendStates[ eFactoryBlendState_NoColourWrite_AlphaTest ]");
}
