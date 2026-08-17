#include "GameSource/World/EnvironmentMap/BrnEnvironmentMap.h"
#include "types.hpp"
#include "rw/math/vpu/vector3_operation.h"   // rw::math::vpu::Add

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnGraphics::EnvironmentMap::Construct @ 0x827B40D0
//   BrnGraphics::EnvironmentMap::Prepare   @ 0x827B4188
//   BrnGraphics::EnvironmentMap::Release   @ 0x827B4218
//   BrnGraphics::EnvironmentMap::Update    @ 0x827B4268

// (The former cpp-local rw::math::vpu::Vector3/Add and CgsGraphics::Camera
// stand-ins were retired 2026-07-24: the real homes (rw vpu vendor header via
// BrnEnvironmentMap.h, CgsCamera.h) now provide them.)

namespace BrnGraphics
{
    static const rw::math::vpu::Vector3 KAV_ENV_MAP_LOOK_DIRECTIONS[E_FACE_NUM] =
    {
        {  1.0f,  0.0f,  0.0f, 0.0f },
        { -1.0f,  0.0f,  0.0f, 0.0f },
        {  0.0f,  1.0f,  0.0f, 0.0f },
        {  0.0f, -1.0f,  0.0f, 0.0f },
        {  0.0f,  0.0f,  1.0f, 0.0f },
        {  0.0f,  0.0f, -1.0f, 0.0f }
    };

    static const rw::math::vpu::Vector3 KAV_ENV_MAP_UP_DIRECTIONS[E_FACE_NUM] =
    {
        { 0.0f, 1.0f,  0.0f, 0.0f },
        { 0.0f, 1.0f,  0.0f, 0.0f },
        { 0.0f, 0.0f, -1.0f, 0.0f },
        { 0.0f, 0.0f,  1.0f, 0.0f },
        { 0.0f, 1.0f,  0.0f, 0.0f },
        { 0.0f, 1.0f,  0.0f, 0.0f }
    };

    // ------------------------------------------------------------------------
    // The four env-map face-camera constants. The DecFIGS DWARF
    // (references/DecFIGS/dwarfdump/GameSource/World/EnvironmentMap/BrnEnvironmentMap.cpp)
    // names all four and puts them at file scope, in this declaration order:
    //     BrnEnvironmentMap.cpp:28  const float32_t KF_ENVMAP_NEAR_CLIP_PLANE;
    //     BrnEnvironmentMap.cpp:29  const float32_t KF_ENVMAP_FAR_CLIP_PLANE;
    //     BrnEnvironmentMap.cpp:30  const float32_t KF_ENVMAP_FOV_HORIZONTAL;
    //     BrnEnvironmentMap.cpp:31  const float32_t KF_ENVMAP_ASPECT_RATIO;
    //
    // NEAR / FAR are RECOVERED. Prepare @0x827B4188 loads them out of the X360
    // READ-ONLY literal pool, so IDA resolves the raw words:
    //     0x827B41E4  lfs f0, flt_82004014@l(r27) ; stfs f0, 0x15C(r31)
    //                 -> pseudocode `a1[21].vector4_f32[3] = 1036831949`
    //                 -> 0x3DCCCCCD == 0.1f      (m_nearClipPlane, camera+0x15C)
    //     0x827B41D4  lfs f0, flt_820CA818@l(r26) ; stfs f0, 0x160(r31)
    //                 -> pseudocode `a1[22].vector4_f32[0] = 1117126656`
    //                 -> 0x42960000 == 75.0f     (m_farClipPlane,  camera+0x160)
    //
    // ⚠ [FLAG BLOCKED-VALUE] FOV / ASPECT are **NOT** recovered. Prepare reads them
    // from flt_82F30E1C / flt_82F30E20 -- the SAME writable-data region that already
    // cost this campaign one dump request (CgsCamera.cpp:51-52 carries
    // KF_DEFAULT_FOVHORIZONTAL @flt_82F30FD4 and KF_DEFAULT_ASPECTRATIO @flt_82F30FD8,
    // both marked ".i64-recovered"), so Hex-Rays prints the SYMBOL, not the word:
    //     0x827B41B8  lfs f0, flt_82F30E20@l(r29) ; stfs f0, 0x158(r31)  (m_aspectRatio)
    //     0x827B41F4  lfs f1, flt_82F30E1C@l(r28) ; bl SetFovHorizontal  (m_fovHorizontal)
    // A repo-wide grep of the IDA export finds those two addresses in exactly ONE
    // file -- 0x827B4188.json, this function -- so no other body attests them, and no
    // initialiser writes them.
    //
    // The two values below are therefore INFERRED, not read, and the inference is
    // stated so it can be checked in one line the moment the bytes are dumped:
    //   * the target is a CUBE whose face is SQUARE -- BrnRendererMemory::
    //     CreateEnvmapBuffer @0x823F6C88 (BrnRendererMemory.cpp:846-848) sets
    //     dimensions KU_ENV_MAP_FACE_SIZE x KU_ENV_MAP_FACE_SIZE (128x128) and
    //     texture type E_TYPE_CUBE -- so a 1:1 pixel aspect forces
    //     KF_ENVMAP_ASPECT_RATIO == 1.0f;
    //   * a cube FACE subtends exactly 90 degrees, and the six face cameras are the
    //     axis-aligned +X/-X/+Y/-Y/+Z/-Z set above, so the only horizontal fov for
    //     which the six frusta tile the cube without a seam or an overlap is
    //     KF_ENVMAP_FOV_HORIZONTAL == pi/2 radians. (SetFovHorizontal consumes
    //     RADIANS: it computes tan(fov * 0.5f); and the sibling constant in the same
    //     data region, KF_DEFAULT_FOVHORIZONTAL, is 1.5707964f == pi/2, i.e. this
    //     build already stores that angle in exactly this spelling.)
    // CONDUCTOR DUMP REQUEST: 0x82F30E14 32 -- the four `const float32_t` in DWARF
    // declaration order should read 0.1f / 75.0f / <fov> / <aspect>; the first two
    // are independently known, so the dump also self-checks the layout assumption.
    // ------------------------------------------------------------------------
    const f32 KF_ENVMAP_NEAR_CLIP_PLANE = 0.1f;    // RECOVERED (flt_82004014)
    const f32 KF_ENVMAP_FAR_CLIP_PLANE  = 75.0f;   // RECOVERED (flt_820CA818)
    const f32 KF_ENVMAP_FOV_HORIZONTAL  = 1.5707964f;  // INFERRED (flt_82F30E1C un-dumped)
    const f32 KF_ENVMAP_ASPECT_RATIO    = 1.0f;        // INFERRED (flt_82F30E20 un-dumped)

    void EnvironmentMap::Construct()
    {
        mCameraPosition = { 0.0f, 0.0f, 0.0f, 0.0f };
        mfFOV = 0.0f;
        mfAspectRatio = 0.0f;

        for (u32 luEnvMapFace = 0; luEnvMapFace < E_FACE_NUM; ++luEnvMapFace)
        {
            maEnvMapCameras[luEnvMapFace].Construct();
        }
    }

    // X360 (WorldModule::Destruct @0x827BD0F0 tail call target): release the
    // face cameras -- same walk as Release, no return.
    //
    // ⚠ Both this walk and Release() below are INERT on the console and, since
    // 2026-08-17, here: CgsGraphics::Camera::Release() is the empty ICF-folded body
    // @0x8284CB38 (see the Prepare banner). They are kept because the loop is what
    // EnvironmentMap::Release @0x827B4218 emits -- six `bl` at the 0x170 Camera stride
    // -- and because they come alive the day Camera acquires real teardown.
    void EnvironmentMap::Destruct()
    {
        for (u32 luEnvMapFace = 0; luEnvMapFace < E_FACE_NUM; ++luEnvMapFace)
        {
            maEnvMapCameras[luEnvMapFace].Release();
        }
    }

    bool EnvironmentMap::Release()
    {
        for (u32 luEnvMapFace = 0; luEnvMapFace < E_FACE_NUM; ++luEnvMapFace)
        {
            maEnvMapCameras[luEnvMapFace].Release();
        }

        return true;
    }

    // ------------------------------------------------------------------------
    // BrnGraphics::EnvironmentMap::Prepare @0x827B4188 (DWARF BrnEnvironmentMap.cpp:131,
    // `bool Prepare()`; called by WorldModule::Prepare @0x827D53B0 -- its ONLY xref --
    // which this tree already wires at BrnWorldModule.cpp:1160).
    //
    // Six iterations, `addi r31, r31, 0x170` per iteration == sizeof(CgsGraphics::Camera)
    // (368; the array is reached BY INDEX here, never by that guest stride), `li r3, 1`
    // on exit. The body is four inlined CgsGraphics::Camera setters per face; the DWARF
    // for this function names exactly those four -- Camera::SetAspectRatio,
    // Camera::SetFarClipPlane, Camera::SetNearClipPlane, Camera::SetFovHorizontal -- and
    // the Feb-2007 CgsCamera.h (rung 3, style/inlining only) gives their bodies, which
    // reproduce the X360 instruction stream store-for-store and call-for-call:
    //
    //   0x827B41B0  bl BaseCollisionGenerator__Destruct   <- Camera::Release(): ICF-FOLDED
    //                                                        to the EMPTY body @0x8284CB38
    //                                                        (a single `blr`; see below)
    //   0x827B41B8  lfs f0, flt_82F30E20                  ] Camera::SetAspectRatio(A):
    //   0x827B41BC  lfs f1, 0x140(r31)                    ]   m_aspectRatio = A;
    //   0x827B41C0  stfs f0, 0x158(r31)                   ]   SetFovHorizontal(m_fovHorizontal);
    //   0x827B41C4  bl SetFovHorizontal                   ]   UpdatePerspectiveProjectionMatrix();
    //   0x827B41CC  bl UpdatePerspectiveProjectionMatrix  ]
    //   0x827B41D4  lfs f0, flt_820CA818                  ] Camera::SetFarClipPlane(75.0f):
    //   0x827B41D8  stfs f0, 0x160(r31)                   ]   m_farClipPlane = F;
    //   0x827B41DC  bl UpdatePerspectiveProjectionMatrix  ]   UpdatePerspectiveProjectionMatrix();
    //   0x827B41E4  lfs f0, flt_82004014                  ] Camera::SetNearClipPlane(0.1f):
    //   0x827B41E8  stfs f0, 0x15C(r31)                   ]   m_nearClipPlane = N;
    //   0x827B41EC  bl UpdatePerspectiveProjectionMatrix  ]   UpdatePerspectiveProjectionMatrix();
    //   0x827B41F4  lfs f1, flt_82F30E1C                  ] Camera::SetFovHorizontal(FOV)
    //   0x827B41F8  bl SetFovHorizontal                   ]   (ends in its own Update)
    //
    // PPC FLOAT ABI: SetFovHorizontal takes ONE float. Hex-Rays renders it as
    // `SetFovHorizontal(a1, v4, v3)` with a bogus middle FXMVECTOR* parameter -- r4 is
    // never set at either call site; the value is f1 only (`lfs f1, 0x140(r31)` for the
    // first call, `lfs f1, flt_82F30E1C` for the second). The committed CgsCamera.h
    // declares the correct one-float form and that is what is called here.
    //
    // Camera+0x140/+0x158/+0x15C/+0x160 are maProjectionScalars[0]/[6]/[7]/[8] --
    // m_fovHorizontal / m_aspectRatio / m_nearClipPlane / m_farClipPlane -- pinned by
    // CgsCamera.h:215-222 and independently re-derived from SetFovHorizontal
    // @0x821F13B0 (it stores f1 to 0x140 and divides 1.0f by 0x158) and
    // UpdatePerspectiveProjectionMatrix @0x827EC778 (it reads 0x15C as `n` and 0x160 as
    // `f`).
    //
    // ⭐ 2026-08-17 (reflections step 2): the three setters are REAL MEMBERS now.
    // CgsCamera.h had no SetAspectRatio / SetNearClipPlane / SetFarClipPlane when this
    // body first landed, so all three inlines were written out here through the public
    // scalar block. They were added to CgsCamera.h from the same asm (the store/call
    // stream above IS their definition, and the Feb-2007 CgsCamera.h:202-227 bodies match
    // it verbatim), so this body now says what the source said.
    //
    // ⚠ THE LEADING Release() IS AN EMPTY CALL ON THE CONSOLE. 0x8284CB38 is a single
    // `blr` -- the identical-code-folding sink every empty body in the image collapsed
    // into (193 xrefs, the whole game's empty destructors, which is why the export
    // labels it CgsSceneManager::CgsCollision::BaseCollisionGenerator::Destruct) -- and
    // BrnGraphics::EnvironmentMap::Release @0x827B4218 loops calling THAT same address
    // over these same six cameras, while EnvironmentMap::Construct @0x827B40D0 loops
    // calling the DIFFERENT body sub_827F94E8 (the KF_DEFAULT_* reset). That pair is
    // what identifies 0x8284CB38 as CgsGraphics::Camera::Release() and 0x827F94E8 as
    // Camera::Construct(). CgsCamera.cpp used to give Release() the reset body; it was
    // corrected in this same change, so this call is now the no-op the console runs.
    // It was net-neutral either way: the four setters below overwrite every scalar the
    // reset touched, and mView (which the reset also re-identitied) is rebuilt by
    // EnvironmentMap::Update before any consumer reads it.
    // ------------------------------------------------------------------------
    bool EnvironmentMap::Prepare()
    {
        for (u32 luEnvMapFace = 0; luEnvMapFace < E_FACE_NUM; ++luEnvMapFace)
        {
            CgsGraphics::Camera& lrFaceCamera = maEnvMapCameras[luEnvMapFace];

            lrFaceCamera.Release();                                          // 0x827B41B0 (empty)

            lrFaceCamera.SetAspectRatio(KF_ENVMAP_ASPECT_RATIO);             // 0x827B41C0-41CC
            lrFaceCamera.SetFarClipPlane(KF_ENVMAP_FAR_CLIP_PLANE);          // 0x827B41D8-41DC
            lrFaceCamera.SetNearClipPlane(KF_ENVMAP_NEAR_CLIP_PLANE);        // 0x827B41E8-41EC
            lrFaceCamera.SetFovHorizontal(KF_ENVMAP_FOV_HORIZONTAL);         // 0x827B41F8
        }

        return true;
    }

    void EnvironmentMap::Update(rw::math::vpu::Vector3 lCameraPosition)
    {
        mCameraPosition = lCameraPosition;

        for (u32 luEnvMapFace = 0; luEnvMapFace < E_FACE_NUM; ++luEnvMapFace)
        {
            const rw::math::vpu::Vector3 lTargetPosition =
                rw::math::vpu::Add(lCameraPosition, KAV_ENV_MAP_LOOK_DIRECTIONS[luEnvMapFace]);

            maEnvMapCameras[luEnvMapFace].LookAt(
                lCameraPosition,
                KAV_ENV_MAP_UP_DIRECTIONS[luEnvMapFace],
                lTargetPosition);
            maEnvMapCameras[luEnvMapFace].SetPerspectiveProjectionMatrixRightHanded();
        }
    }
}
