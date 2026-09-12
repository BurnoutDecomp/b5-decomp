#include "GameShared/GameClasses/Graphics/CgsBlendStateFactory.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Graphics/CgsResourceAllocatorCreate.h"
#include "pc/gcm/renderengine/renderstates.h"   // renderengine::ResourceDescriptor5

// Reconstructed from the shipped console build of
//   CgsBlendStateFactory::Construct  (2248 bytes, 562 instructions)
//
// Builds the fixed set of nine blend states the renderer, the post-fx chain and the
// dispatch interpreter select between. Only Construct is console-attested for this TU;
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
//      is reached from `static BrnGame::BrnGameModule gGameModule;` in BrnMain.cpp)
//      makes the vtable live in the linked exe and requires every virtual to be
//      defined in that link. All three of this class's virtuals ARE defined -- Construct
//      below, Destruct and Prepare as flagged fillers at the end of this file -- but the sibling
//      CgsRasterizerStateFactory still declares itself TU-locally and non-polymorphic
//      in its own .cpp, so the three-header swap cannot be completed honestly yet.
//   2. Mounting it would buy nothing today anyway: NOTHING calls
//      BrnRendererModule::Construct -> mBlendStateFactory.Construct on this build, so
//      every slot would still read null, and a null slot handed to the frame bracket's
//      blend third is a live null dereference (the bracket compares wanted against
//      ImRendererBase::mgpLastState, which any immediate-mode draw leaves non-null, so
//      the inequality fires and shadow::Device::Xbox2SetStateLowLevelShadowed reads
//      lpu[0] with no null guard -- see shadowingdevice.cpp).
//      The table becomes usable when something CALLS Construct, not when the class is
//      embedded.
// =============================================================================

// The definition of the private static table declared in the header. The console's
// saBlendStates[0..8] are nine consecutive words; all nine read zero in the shipped
// image, i.e. the table is zero-initialised storage and Construct is what fills it.
renderengine::BlendMaterialState* CgsBlendStateFactory::saBlendStates[E_FACTORY_BLEND_STATE_COUNT] = {};

namespace
{
    // The packed per-channel blend word every one of the nine parameter blocks starts
    // from. The console builds it once and splats it across all four channels of every
    // block. Same constant, same role, as CgsImRendererBlendState.cpp's
    // KU_BLEND_FACTOR_WORD.
    const u32 KU_BLEND_FACTOR_DEFAULT = 0x07060706u;

    // The rw resource allocator's Create slot, at vtable +0x10. Construct reaches the
    // allocator through exactly one indirect call: load the allocator argument's vtable,
    // take its +0x10 slot, and call it with (out, allocator, descriptor, 0) -- the same
    // call the two sibling factories and the immediate-mode builders make.
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
    // allocator, then initialise it. This is the block the original compiler INLINED nine
    // times (each inlining keeps its own 32-byte out buffer; the five-word handle array
    // is shared and zeroed once before the first slot). Re-rolled here, exactly the
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

        // A five-iteration, 4-byte-stride copy out of the allocator's out buffer into the
        // handle array Initialize is given.
        for (int liHandle = 0; liHandle < 5; ++liHandle)
            lapStateHandles[liHandle] = lapAllocatedHandles[liHandle];

        return static_cast<renderengine::BlendMaterialState*>(
            renderengine::BlendState::Initialize(lapStateHandles, lpParameters));
    }
}

// Clear the nine-slot table, then build all nine blend states into it, asserting each
// slot came back non-null.
//
// SIGNATURE. `virtual void Construct(rw::IResourceAllocator*)` per the original
// declaration, and the attested calling convention agrees on both counts that matter:
// the allocator arrives in the SECOND argument slot while the first is never read (an
// unused implicit `this`, so the function is a non-static member), and nothing in the
// image calls it directly (vtable dispatch, so nothing consumes a return value either --
// the decompiler's trailing `result` is only whatever CgsDev::Assert::EndAssert happened
// to leave behind on the last assert path).
//
// THE PARAMETER BLOCKS. All nine share one renderengine::BlendStateParameters shape and
// differ in only SIX fields, so the invariant thirteen are set once and the six varying
// ones are set explicitly at every slot (no value is left to carry over from the
// previous slot -- the console rebuilds the whole block each time, into its own stack
// slot). The invariant part is field-for-field identical to the one
// CgsGraphics::ImRendererBase::ConstructBlendState builds
// (see CgsImRendererBlendState.cpp), which is an independent corroboration of this
// decode of the block's layout.
void CgsBlendStateFactory::Construct(rw::IResourceAllocator* lpAllocator)
{
    // The table base is formed once and a nine-iteration loop stores 0 through it, four
    // bytes at a time.
    for (u32 luSlot = 0; luSlot < E_FACTORY_BLEND_STATE_COUNT; ++luSlot)
        saBlendStates[luSlot] = nullptr;

    // The allocator is passed straight through to the TU-local single-entry-point shim.
    // reinterpret_cast, not static_cast, because the shim is not related to
    // rw::IResourceAllocator by inheritance; the POINTER VALUE is unchanged (the shim has
    // no vtable and no bases) and ResourceAllocatorCreate casts it straight back.
    ResourceAllocator* lpAllocatorShim = reinterpret_cast<ResourceAllocator*>(lpAllocator);

    // ---- the thirteen fields every one of the nine blocks sets identically ----------
    renderengine::BlendStateParameters lParameters = {};
    lParameters.maBlendFactor[1] = KU_BLEND_FACTOR_DEFAULT;   // block +0x04, every block
    lParameters.maBlendFactor[2] = KU_BLEND_FACTOR_DEFAULT;   // block +0x08
    lParameters.maBlendFactor[3] = KU_BLEND_FACTOR_DEFAULT;   // block +0x0C
    lParameters.muState5         = 15u;
    lParameters.muState6         = 15u;
    lParameters.muState7         = 15u;
    lParameters.muState8         = 135u;
    lParameters.muState9         = 0xFFFFFFFFu;
    lParameters.mbState10        = 0u;
    lParameters.mbState11        = 0u;
    lParameters.mbState12        = 0u;
    lParameters.mbState13        = 0u;
    lParameters.mbState14        = 0u;

    // ---- slot 0 -- Opaque_Modulate_NoAlphaTest_DestRGBA -----------------------------
    // maBlendFactor[0] is the splatted default with its low 16 bits re-inserted from
    // 0x383 shifted left one: (0x383 << 1) = 0x0706, which is what the low half already
    // held, so the stored word is unchanged at 0x07060706.
    // mbHasCustomBlendFactors = 0, so
    // BlendState::Initialize writes its KU_DEFAULT_BLEND_FACTOR into maState[0..3] and
    // these four words are dead -- the console stores them anyway, and so does this.
    lParameters.maBlendFactor[0]        = 0x07060706u;
    lParameters.mbHasCustomBlendFactors = 0u;
    lParameters.muState4                = 15u;
    lParameters.muState15               = 7u;
    lParameters.muState17               = 0u;
    lParameters.mbState16               = 0u;
    saBlendStates[E_FACTORY_BLEND_STATE_OPAQUE_MODULATE_NO_ALPHA_TEST_DEST_RGBA] =
        CreateBlendState(lpAllocatorShim, &lParameters);
    CGS_ASSERT(saBlendStates[E_FACTORY_BLEND_STATE_OPAQUE_MODULATE_NO_ALPHA_TEST_DEST_RGBA],
               "saBlendStates[ eFactoryBlendState_Opaque_Modulate_NoAlphaTest_DestRGBA ]");

    // ---- slot 1 -- Transparent_Modulate_NoAlphaTest_DestRGBA ------------------------
    // Identical to slot 0 except that mbHasCustomBlendFactors is now 1, so the four
    // factor words below are the ones that reach the object. The word itself is the same
    // 0x383 insert -> 0x07060706.
    lParameters.maBlendFactor[0]        = 0x07060706u;
    lParameters.mbHasCustomBlendFactors = 1u;
    lParameters.muState4                = 15u;
    lParameters.muState15               = 7u;
    lParameters.muState17               = 0u;
    lParameters.mbState16               = 0u;
    saBlendStates[E_FACTORY_BLEND_STATE_TRANSPARENT_MODULATE_NO_ALPHA_TEST_DEST_RGBA] =
        CreateBlendState(lpAllocatorShim, &lParameters);
    CGS_ASSERT(saBlendStates[E_FACTORY_BLEND_STATE_TRANSPARENT_MODULATE_NO_ALPHA_TEST_DEST_RGBA],
               "saBlendStates[ eFactoryBlendState_Transparent_Modulate_NoAlphaTest_DestRGBA ]");

    // ---- slot 2 -- Transparent_Additive_NoAlphaTest_DestRGBA ------------------------
    // The low 16 bits are replaced with (0x83 << 1) = 0x0106, giving 0x07060106.
    lParameters.maBlendFactor[0]        = 0x07060106u;
    lParameters.mbHasCustomBlendFactors = 1u;
    lParameters.muState4                = 15u;
    lParameters.muState15               = 7u;
    lParameters.muState17               = 0u;
    lParameters.mbState16               = 0u;
    saBlendStates[E_FACTORY_BLEND_STATE_TRANSPARENT_ADDITIVE_NO_ALPHA_TEST_DEST_RGBA] =
        CreateBlendState(lpAllocatorShim, &lParameters);
    CGS_ASSERT(saBlendStates[E_FACTORY_BLEND_STATE_TRANSPARENT_ADDITIVE_NO_ALPHA_TEST_DEST_RGBA],
               "saBlendStates[ eFactoryBlendState_Transparent_Additive_NoAlphaTest_DestRGBA ]");

    // ---- slot 3 -- Transparent_Subtractive_NoAlphaTest_DestRGBA ---------------------
    // (0x93 << 1) = 0x0126 inserted into the low half -> 0x07060126.
    lParameters.maBlendFactor[0]        = 0x07060126u;
    lParameters.mbHasCustomBlendFactors = 1u;
    lParameters.muState4                = 15u;
    lParameters.muState15               = 7u;
    lParameters.muState17               = 0u;
    lParameters.mbState16               = 0u;
    saBlendStates[E_FACTORY_BLEND_STATE_TRANSPARENT_SUBTRACTIVE_NO_ALPHA_TEST_DEST_RGBA] =
        CreateBlendState(lpAllocatorShim, &lParameters);
    CGS_ASSERT(saBlendStates[E_FACTORY_BLEND_STATE_TRANSPARENT_SUBTRACTIVE_NO_ALPHA_TEST_DEST_RGBA],
               "saBlendStates[ eFactoryBlendState_Transparent_Subtractive_NoAlphaTest_DestRGBA ]");

    // ---- slot 4 -- Transparent_AdditiveAlphaOne_NoAlphaTest_DestRGBA ----------------
    // 0x101 inserted into the low half as-is (NOT shifted), giving 0x07060101.
    lParameters.maBlendFactor[0]        = 0x07060101u;
    lParameters.mbHasCustomBlendFactors = 1u;
    lParameters.muState4                = 15u;
    lParameters.muState15               = 7u;
    lParameters.muState17               = 0u;
    lParameters.mbState16               = 0u;
    saBlendStates[E_FACTORY_BLEND_STATE_TRANSPARENT_ADDITIVE_ALPHA_ONE_NO_ALPHA_TEST_DEST_RGBA] =
        CreateBlendState(lpAllocatorShim, &lParameters);
    CGS_ASSERT(saBlendStates[E_FACTORY_BLEND_STATE_TRANSPARENT_ADDITIVE_ALPHA_ONE_NO_ALPHA_TEST_DEST_RGBA],
               "saBlendStates[ eFactoryBlendState_Transparent_AdditiveAlphaOne_NoAlphaTest_DestRGBA ]");

    // ---- slot 5 -- Transparent_AdditiveRGB_NoAlphaTest_DestRGB ----------------------
    // The only slot whose word-0 is built by a sequence rather than one insert; every
    // step below writes the same word, and nothing reads it until GetResourceDescriptor,
    // so only the FINAL value is observable:
    //     start                       0x07060706   (splatted default)
    //     low 5 bits <- 6             : unchanged
    //     byte 2 (big-endian) <- 1    : 0x07060106
    //     clear the 0x000000E0 field
    //     clear the 0x001F0000 field  (together & 0xFFE0FF1F) -> 0x07000106
    //     byte 0 (MSB) <- 1           : 0x01000106
    //     clear the 0x00E00000 field  : unchanged
    //     FINAL                       0x01000106
    // (The three mask immediates 0x000000E0 / 0x001F0000 / 0x00E00000 are contiguous,
    // non-overlapping bit runs, so this word is a packed multi-field register and those
    // are three of its field boundaries. What the fields MEAN is not claimed here.)
    lParameters.maBlendFactor[0]        = 0x01000106u;
    lParameters.mbHasCustomBlendFactors = 1u;
    lParameters.muState4                = 15u;
    lParameters.muState15               = 7u;
    lParameters.muState17               = 0u;
    lParameters.mbState16               = 0u;
    saBlendStates[E_FACTORY_BLEND_STATE_TRANSPARENT_ADDITIVE_RGB_NO_ALPHA_TEST_DEST_RGB] =
        CreateBlendState(lpAllocatorShim, &lParameters);
    CGS_ASSERT(saBlendStates[E_FACTORY_BLEND_STATE_TRANSPARENT_ADDITIVE_RGB_NO_ALPHA_TEST_DEST_RGB],
               "saBlendStates[ eFactoryBlendState_Transparent_AdditiveRGB_NoAlphaTest_DestRGB ]");

    // ---- slot 6 -- Transparent_AdditiveInvDestColor_NoAlphaTest_DestRGBA ------------
    // 0x109 inserted into the low half as-is -> 0x07060109.
    lParameters.maBlendFactor[0]        = 0x07060109u;
    lParameters.mbHasCustomBlendFactors = 1u;
    lParameters.muState4                = 15u;
    lParameters.muState15               = 7u;
    lParameters.muState17               = 0u;
    lParameters.mbState16               = 0u;
    saBlendStates[E_FACTORY_BLEND_STATE_TRANSPARENT_ADDITIVE_INV_DEST_COLOR_NO_ALPHA_TEST_DEST_RGBA] =
        CreateBlendState(lpAllocatorShim, &lParameters);
    CGS_ASSERT(saBlendStates[E_FACTORY_BLEND_STATE_TRANSPARENT_ADDITIVE_INV_DEST_COLOR_NO_ALPHA_TEST_DEST_RGBA],
               "saBlendStates[ eFactoryBlendState_Transparent_AdditiveInvDestColor_NoAlphaTest_DestRGBA ]");

    // ---- slot 7 -- NoColourWrite_NoAlphaTest ----------------------------------------
    // The low half of the splatted default is cleared -> 0x07060000. This is the first
    // slot to set muState4 (ColorWriteEnable) to 0 -- which is exactly what its name says
    // -- and mbHasCustomBlendFactors goes back to 0.
    lParameters.maBlendFactor[0]        = 0x07060000u;
    lParameters.mbHasCustomBlendFactors = 0u;
    lParameters.muState4                = 0u;
    lParameters.muState15               = 7u;
    lParameters.muState17               = 0u;
    lParameters.mbState16               = 0u;
    saBlendStates[E_FACTORY_BLEND_STATE_NO_COLOUR_WRITE_NO_ALPHA_TEST] =
        CreateBlendState(lpAllocatorShim, &lParameters);
    CGS_ASSERT(saBlendStates[E_FACTORY_BLEND_STATE_NO_COLOUR_WRITE_NO_ALPHA_TEST],
               "saBlendStates[ eFactoryBlendState_NoColourWrite_NoAlphaTest ]");

    // ---- slot 8 -- NoColourWrite_AlphaTest ------------------------------------------
    // Same 0x07060000 word and the same muState4 = 0, and it is the ONLY slot that turns
    // the alpha test on: mbState16 = 1, with muState15 = 4 and muState17 = 0x80. Per
    // blendstate.h those three land in maState[15] AlphaFunc, maState[16]
    // AlphaTestEnable and maState[17] AlphaRef.
    lParameters.maBlendFactor[0]        = 0x07060000u;
    lParameters.mbHasCustomBlendFactors = 0u;
    lParameters.muState4                = 0u;
    lParameters.muState15               = 4u;
    lParameters.muState17               = 0x80u;
    lParameters.mbState16               = 1u;
    saBlendStates[E_FACTORY_BLEND_STATE_NO_COLOUR_WRITE_ALPHA_TEST] =
        CreateBlendState(lpAllocatorShim, &lParameters);
    CGS_ASSERT(saBlendStates[E_FACTORY_BLEND_STATE_NO_COLOUR_WRITE_ALPHA_TEST],
               "saBlendStates[ eFactoryBlendState_NoColourWrite_AlphaTest ]");
}

// FLAG PC-platform leaf: no console body or caller; vtable filler.
void CgsBlendStateFactory::Destruct()
{
}

// FLAG PC-platform leaf: no console body or caller; vtable filler.
bool CgsBlendStateFactory::Prepare()
{
    return true;
}
