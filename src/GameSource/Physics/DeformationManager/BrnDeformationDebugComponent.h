#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3, Vector4, Matrix44Affine, VecFloat
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"   // CgsDev::DebugComponent (base)
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"         // DebugUI::StringList
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnSharedDeformationEnums.h"  // ENextSensorDirection

// BrnPhysics::Deformation::DeformationDebugComponent -- the in-game debug visualiser/editor for the
// vehicle deformation rig. It is the (file-scope static) debug component the DeformationManager
// constructs/registers (BrnDeformationManager.cpp). Through the debug menu it lets you pick a
// deformable rig, a sensor and a driven/tag point, nudge their positions, drive the six per-axis
// compression sliders (and the "max drivetime / max total" presets), tweak the part-detachment /
// impulse-scale tuning globals, and -- in RenderWorld -- draw the whole rig (sensors, tag points,
// driven points, skinning offsets, driven-point connections, the drivetime-limits box, the deformed
// box, the detached wheels) into the 3D debug renderer.
//
// SOURCE-OF-TRUTH: bodies reconstructed from BURNOUT_X360_ARTIST.XEX (offset+branch authority);
// class shape / member names / method list from the DecFIGS DWARF
// (BrnDeformationDebugComponent.h:131/149). Derives from CgsDev::DebugComponent (DWARF :149).
//
// LAYOUT (members BY NAME + SEQUENCE; the X360 32-bit `this` offsets are noted for provenance and do
// NOT all reproduce on the 64-bit host -- every access is by member name). The base CgsDev::
// DebugComponent occupies this+0..11 on the console (vtable + mbActive + mpDebugLinkedListNext), so the
// first member below sits at console +0xC.

namespace BrnPhysics
{
namespace Deformation
{
    class DeformationManager;
    class DeformableObject;
    struct DeformationSensor;

    // BrnDeformationDebugComponent.h:131 (DWARF). A thin view over an AxisAlignedBox that returns a
    // single signed extent (one min/max corner lane) selected by ENextSensorDirection. The X360 body
    // (0x825DEC50) is a 6-case jump table: even cases read the min corner (this+0), odd cases the max
    // corner (this+16); the lane (x/y/z) is picked per axis pair. Modelled as a plain struct with the
    // two corner Vector3 lanes + the GetExtent selector (the only method the X360 ledger attests).
    struct DeformationAxisAlignedBox
    {
        Vector3 mMin;   // console this+0  -- box min corner
        Vector3 mMax;   // console this+16 -- box max corner

        // X360 0x825DEC50. Return the signed extent of the box along the given direction: the min
        // corner's lane for the POS_* directions, the max corner's lane for the NEG_* directions.
        f32 GetExtent(ENextSensorDirection leAxis) const;
    };

    class DeformationDebugComponent : public CgsDev::DebugComponent
    {
    public:
        DeformationDebugComponent() {}

        // X360 0x82606480. Two-phase construct: base DebugComponent::Construct (the X360 image folds the
        // trivial base body onto the BaseCollisionGenerator::Destruct address), cache the manager,
        // zero every slider / selector / flag, then refresh the selected-rig state.
        void Construct(DeformationManager* lpDeformationManager);

        // X360 0x82623178. Per-frame: if a rig is selected and it has a streamed deformation spec,
        // recompute its drive-time deformation limits.
        virtual void Update();

        // X360 0x82606548. Draw the whole selected rig into the 3D debug renderer (sensors / tag
        // points / driven points / skinning offsets / connections / boxes / detached wheels), gated by
        // the per-feature render flags.
        virtual void RenderWorld(CgsDev::Debug3DImmediateRender* lpRender);

        // X360 0x82623198. Register every slider/selector/flag/action with the debug menu (ranges,
        // steps, read-only state, change callbacks), then refresh the selected-rig + selected-sensor.
        virtual void OnActivate();

    private:
        // ---- selected-rig / sensor / point sliders + the compression solver ----------------------

        // X360 0x825DF030. Apply the six per-axis compression scales to the selected rig (rebuilds the
        // streamed deformation into the live sensors/points). Bodied in its own TU (the rw vec-float
        // inline-math home); declared here because the X360 ledger attests it on this class and the
        // compression callbacks call it BY NAME. The trailing 7th arg the asm passes (the constant 0.0
        // "roof-extra" lane) is folded into the six-axis form.
        void CompressSelectedRig(f32 lfRightSide, f32 lfLeftSide, f32 lfFloor,
                                 f32 lfRoof, f32 lfRear, f32 lfFront);

        // X360 0x82607160. Sweep each axis toward its maximum drive-time compression: for each of the
        // six directions, binary-narrow the compression scale until the deformed-box extent stops
        // growing, re-applying CompressSelectedRig and updating the deformed bbox each step.
        void CompressSelectedRig_MaxDrivetime();

        // X360 0x825DED20. Draw a hollow sphere at each detached wheel's world centre (4 wheels). Calls
        // back into StreamedDeformationSpec::GetW to resolve the wheel spec; asserts the wheel/spec.
        void DrawDetachedWheels(CgsDev::Debug3DImmediateRender* lpRender);

        // X360 0x825B97F8. Mark the selected rig's "render car part" detach flag (the asm flags the
        // part matching giDebugOnlyRenderThisCarPart). The body's part-scan asserts "No longer works".
        void DetachPart();

        // ---- debug-menu callbacks (registered as VariableCallbackFunction / DebugCallbackFunction).
        //      These are static so they can be taken as plain C function pointers; the X360 passes the
        //      component as the callback user-data (2nd arg), ignoring the unused 1st (the value ptr).

        static void OnCompressionChange(void* lpValue, void* lpUserData);          // X360 0x825DF2E0
        static void OnCompressionPresetChange(void* lpValue, void* lpUserData);    // X360 0x826073A8
        static void OnSelectedRigChange(void* lpValue, void* lpUserData);          // X360 0x825DF408
        static void OnSelectedSensorChange(void* lpValue, void* lpUserData);       // X360 0x825DF310
        static void OnSelectedPointChange(void* lpValue, void* lpUserData);        // X360 0x825DF688
        static void OnPointPositionChange(void* lpValue, void* lpUserData);        // X360 0x825B98F8
        static void OnSensorPositionChange(void* lpValue, void* lpUserData);       // sensor X/Y/Z/scratch edit
        static void DetachPartCallback(void* lpUserData);                          // wraps DetachPart

        // The compression-tuning change forwards (registered for the joint penetration/force +
        // angular-velocity sliders); declared-only -- their bodies are separate TUs (the asm registers
        // these addresses, which the X360 image keeps outside this TU's function set).
        static void JointPentrationMultiplierCallback(void* lpValue, void* lpUserData);
        static void JointForceMultiplierCallback(void* lpValue, void* lpUserData);
        static void AngularVelocityDetachmentCallback(void* lpValue, void* lpUserData);

        // The two debug-menu action callbacks (DebugCallbackFunction = void(*)(void*)). ResetSensors is
        // the trivial folded body the X360 registers via the BaseCollisionGenerator::Destruct address;
        // ResetSelectedRig is the per-rig reset action. Declared-only here (bodied in their own TUs).
        static void ResetSensors(void* lpUserData);
        static void ResetSelectedRig(void* lpUserData);

    private:
        // ===========================================================================================
        // FULL MEMBER LAYOUT (DWARF BrnDeformationDebugComponent.h:315-358). Console offsets noted.
        // ===========================================================================================
        DeformationManager* mpDeformationManager;   // +0x0C (12)  the owning manager
        s32                 miSelectedRig;           // +0x10 (16)  selected rig index (0..27)
        DeformableObject*   mpSelectedRig;           // +0x14 (20)  the selected rig, or null
        s32                 miSelectedSensor;        // +0x18 (24)  selected sensor index
        DeformationSensor*  mpSelectedSensor;        // +0x1C (28)  the selected sensor, or null
        s32                 miSelectedPoint;         // +0x20 (32)  selected driven/tag point index

        bool                mbRenderDeformationRig;          // +0x24 (36)
        bool                mbRenderTagPoints;               // +0x25 (37)
        bool                mbRenderDrivenPoints;            // +0x26 (38)
        bool                mbRenderOffsets;                 // +0x27 (39)
        bool                mbRenderDrivenPointConnections;  // +0x28 (40)
        bool                mbDrawDrivetimeLimitsBox;        // +0x29 (41)
        bool                mbDrawDeformedBox;               // +0x2A (42)
        bool                mbDrawDetachedWheels;            // +0x2B (43)

        f32                 mfSensorX;        // +0x2C (44)
        f32                 mfSensorY;        // +0x30 (48)
        f32                 mfSensorZ;        // +0x34 (52)
        f32                 mfSensorScratch;  // +0x38 (56)

        f32                 mfCompressRightSide;  // +0x3C (60)  "Right side"
        f32                 mfCompressLeftSide;   // +0x40 (64)  "Left side"
        f32                 mfCompressFloor;      // +0x44 (68)  "Floor"
        f32                 mfCompressRoof;       // +0x48 (72)  "Roof"
        f32                 mfCompressRear;       // +0x4C (76)  "Rear"
        f32                 mfCompressFront;      // +0x50 (80)  "Front"

        s32                       miCompressPreset;          // +0x54 (84)  selected preset (0=None,1=MaxDrivetime,2=MaxTotal)
        CgsDev::DebugUI::StringList maCompressPresetStrings[3]; // +0x58..0x6C  {0,"None"},{1,"Max drivetime"},{2,"Max total"}

        f32                 mfPointX;  // +0x70 (112)
        f32                 mfPointY;  // +0x74 (116)
        f32                 mfPointZ;  // +0x78 (120)

        f32                 mfPenetrationLimitMultiplier;  // +0x7C (124)  joint penetration multiplier
        f32                 mfForceLimitMultiplier;        // +0x80 (128)  joint force multiplier

        f32                 mfAngularVelocityForDetachment;  // +0x84 (132)
        f32                 mfAngularVelocityDecay;          // +0x88 (136)
    };
}
}
