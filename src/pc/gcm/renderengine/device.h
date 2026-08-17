#pragma once

#include "types.hpp"
#include <Windows.h>

struct IDirect3D9;
struct IDirect3DDevice9;

namespace renderengine
{
    extern bool gFullscreen;
    extern s32 gDisplayWidth;
    extern s32 gDisplayHeight;
    extern s32 gAdapterIndex;
    extern s32 gAspectRatioIndex;
    // The scene target's anti-aliasing knob. 0 = the console's own multisample format (default,
    // Xenos format 1 == 2 samples); 1 = force off; 2/4/8 = force that sample count. Sourced from
    // config.ini `[Settings] AntiAliasing` (BrnMain.cpp LoadConfig/SaveConfig) like the display
    // extent above it; the full semantics, and why 0 is not "off", are on the DEFINITION in
    // device.cpp. Consumed by the PC render-target leaf; also declared in
    // pc/gcm/renderengine/Xbox2SurfaceShims.h so the leaf need not pull <Windows.h> in through
    // this header (SAME OBJECT, one definition -- not a second global).
    extern s32 gAntiAliasing;
    // Whether the PC honours the console's per-material ALPHA-TO-MASK request (rung 9).
    // 1 = honour it (default, and what the shipped material data asks for); 0 = never apply
    // the vendor alpha-to-coverage hook, which leaves alpha-tested cut-outs with the hard
    // edge they had before rung 9. Sourced from config.ini `[Settings] AlphaToCoverage`
    // (BrnMain.cpp LoadConfig/SaveConfig) and seeded by Device::Initialize, exactly like
    // gAntiAliasing above it. Consumed by the alpha-to-mask leaves in XenonD3D9Shims.cpp.
    // It is an OFF SWITCH ONLY: there is no D3D9 way to ask for coverage a material did not
    // request, so 1 does not mean "force", it means "do what the console did".
    extern s32 gAlphaToCoverage;
    // Present synchronisation. 1 = D3DPRESENT_INTERVAL_DEFAULT (vertical sync, the behaviour
    // this build has always had); 0 = D3DPRESENT_INTERVAL_IMMEDIATE (uncapped presents). Sourced
    // from config.ini `[Display] VSync` (BrnMain.cpp LoadConfig/SaveConfig) and seeded by
    // Device::Initialize like the settings above. Read ONCE, at CreateDevice; the console's own
    // per-frame D3DDevice_SetRenderState_PresentInterval writes stay the no-op leaf they are.
    // 0 exists so a frame-time measurement is not the refresh rate.
    extern s32 gVSync;
    extern HWND hWnd;

    extern IDirect3D9* gD3D9;
    extern IDirect3DDevice9* gDevice;

    // The adapter's current display refresh rate in Hz, or 0 if it cannot be read.
    // ⭐ 2026-08-16 (boot audit F-P1-9). BrnGameModule::Construct's step 9 seeds both game
    // timers with 1/<refresh rate>, and the console reads that rate out of the display mode
    // it validated one step earlier (asserting 50 or 60). We had no mode query at all and
    // hardcoded 1/60 -- fine on a 60Hz panel, wrong on anything else, and silently so.
    u32 GetDisplayRefreshRate();

    // The bound D3D surface-state object (the colour/depth surfaces + their format/size). On X360
    // this is the GPU D3DSURFACES descriptor renderengine::Device::SetState installs; declared here
    // (forward) so the render-target / immediate-mode layers reference it by name. Layout lives in
    // its own renderengine TU.
    class RenderTargetState;

    // The colour-clear parameter block. DWARF names the type renderengine::ClearColorParameters
    // (source SDKs/EATech/include/ps3/gcm/renderengine/device.h:528) but the PS3 form holds a single
    // packed `uint32_t m_color` (RGBA32, with SetColor/GetColor). THE X360 FORM IS FOUR FLOATS, and
    // the asm settles it: Clear @0x82B61C88 forwards its own r3 verbatim to D3DDevice_ClearF's
    // `const D3DVECTOR4* pColor` (`mr r6, r28` @0x82B61D58), and its caller
    // BrnRendererModule::BeginQuarterResBuffer @0x82408C38 builds four consecutive floats on the stack
    // (@0x82408CCC/D0/D4/D8) and passes their address. Rung 1 arbitrates, so the X360 form is modelled.
    //
    // This COMPLETES the forward declaration already in the tree at
    // GameShared/.../ImRenderBuffer/CgsImRenderBufferTemplate.h:68 (same `struct` key, same
    // namespace); that file needs no change.
    struct ClearColorParameters
    {
        f32 mafColourRGBA[4];
    };

    // Which colour target(s) Clear touches. DWARF nests this as
    // renderengine::RenderTargetState::TargetID (source
    // SDKs/EATech/include/ps3/gcm/renderengine/states.h:216). It is declared at namespace scope HERE,
    // not in that class, only because RenderTargetState has no header home in this tree yet -- its
    // definition is TU-local in PostFxRenderTargetPCLeaf.cpp. Nothing else in the tree declares this
    // enum, so this is its one home; MOVE it into the class the day that header exists.
    //
    // *** THE VALUES ARE X360's, AND THEY DIFFER FROM THE DWARF's. *** The PS3 DWARF numbers this
    // COLOR0..COLOR3 = 0..3, DEPTHSTENCIL = 4, COLOR_ALL = 5. The X360 body @0x82B61C88 maps its
    // parameter through a five-way compare (0x82B61D04..0x82B61D4C) onto D3D clear masks:
    // 4 -> 0xF, 0 -> 0x1, 1 -> 0x2, 2 -> 0x4, 3 -> 0x8, and there is no case 5. 0xF is exactly the OR
    // of the other four, which identifies the ladder as the Xenon D3DCLEAR_TARGET0..TARGET3 bits and
    // their union D3DCLEAR_TARGET -- the ZBUFFER (0x10) and STENCIL (0x20) bits never appear on any
    // path. The tree models the same Xenon bit set independently at ImmediateModePCLeaf.cpp:466-468.
    // So on X360 the value 4 means ALL FOUR COLOUR TARGETS, not depth/stencil. Importing the DWARF
    // numbering would turn BrnRendererModule::BeginQuarterResBuffer's colour clear into a depth clear
    // and make the BlitDepth two lines later redundant.
    enum ETargetId : u32
    {
        E_TARGETID_COLOUR0    = 0,
        E_TARGETID_COLOUR1    = 1,
        E_TARGETID_COLOUR2    = 2,
        E_TARGETID_COLOUR3    = 3,
        E_TARGETID_COLOUR_ALL = 4
    };

    class Device
    {
    public:
        static bool Initialize();
        static void Start();
        static bool FrameBegin();
        static bool FrameBeginNoClear();
        static void ShowPixelBuffer();

        // Bind a render-target (surface) state on the device (DWARF renderengine::device.h:1042;
        // X360 guest renderengine__Device__SetState). The post-fx render-target wrapper calls this to
        // install its colour/depth surfaces before drawing.
        static void SetState(const RenderTargetState* lpState);

        // [PC bring-up] The world-pass default render states (opaque: Z test+write,
        // no blend; transparent: Z test only + alpha blend). Stands in for the
        // technique state-group binds until the MaterialState path lands.
        // Defined in XenonD3D9Shims.cpp.
        static void SetWorldPassDefaultStates(bool lbTransparentPass);

        // X360 0x82B61C88. DWARF: `void Clear(const ClearColorParameters &,
        // renderengine::RenderTargetState::TargetID);` at SOURCE device.h:1527 (dwarfdump file line
        // 1135). Clear the selected colour target(s) to lrClearColour.
        //
        // COLOUR ONLY: the body never sets D3DCLEAR_ZBUFFER or D3DCLEAR_STENCIL on any path; the
        // depth/stencil clear is a separate overload, DWARF source device.h:1530 (dwarfdump file line
        // 1138), `void Clear(const ClearDepthStencilParameters &);` -- whose PC counterpart already
        // exists as DeviceClearDepthStencil in ImmediateModePCLeaf.cpp.
        //
        // TWO parameters, not the five Hex-Rays prints. `Clear(const D3DVECTOR4*, int, int, int,
        // DWORD)` is the PPC float-ABI artefact this project has documented once already (see
        // CgsPerfMonCpu.h on AddMonitor): the tail call is D3DDevice_ClearF(pDevice=r3, Flags=r4,
        // pRect=r5, pColor=r6, Z=f1, Stencil=r8) and the float Z SKIPS the r7 GPR slot, so Hex-Rays
        // sees a hole and invents arguments for it. Nothing is read uninitialised: Z is
        // flt_82001C98 = 1.0f (DATA_DUMP.md, +0x0000) and Stencil is `li r8, 0` @0x82B61D54.
        //
        // Declared `static`, and that MATCHES the console rather than diverging from it: the X360
        // body @0x82B61C88 is already object-free. Its incoming r3 is the COLOUR block (`mr r28, r3`
        // @0x82B61C98, then `mr r7, r28 # pClearColor` @0x82B61CDC and `mr r6, r28 # pColor`
        // @0x82B61D58), and pDevice is loaded fresh from the file-scope off_83271608
        // (@0x82B61CEC, @0x82B61D68) -- exactly what the host class does with its `gDevice`/`gD3D9`
        // pair. IDA's prototype says the same thing: `Clear(const D3DVECTOR4 *a1, ...)`. There is no
        // `this` on either side, so there is nothing here to FLAG.
        //
        // The BODY is its own TU (guest 0x82B61C88) and is NOT written here -- writing it would mean
        // modelling the predicated D3DDevice_BeginTiling branch, which is separate Xenos tiling
        // machinery. Declaration-only, and NOTHING IN THE LINKED SOURCE LIST CALLS IT today (see
        // REPORT.md section 2), so it costs the link nothing until a caller lands.
        static void Clear(const ClearColorParameters& lrClearColour, ETargetId leTarget);
    };
}
