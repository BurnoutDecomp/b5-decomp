// Native sphere/triangle contact kernels. The Triangle4 paths retain the
// ARTIST VMX operation order and all four output lanes. The single-triangle
// swept path below retains its separately reconstructed implementation.

#include "GameShared/GameClasses/Geometric/Intersection/CgsTriangleSphere.h"

#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsSweptSphere.h"  // the swept kernel below
#include "GameShared/GameClasses/Geometric/Primitives/CgsTriangle.h"     // CgsGeometric::Triangle (IntersectTriangleSweptSphere)
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT (IntersectTriangleSweptSphere)
#include "rw/math/vpu/vector3_operation.h"                               // rw::math::vpu::IsValid(Vector3)
#include <cmath>     // std::sqrt (the vrsqrtefp lowering)
#include <cstring>   // std::memcpy (mask lane bit patterns)
#include <cstdlib>
#include <xtl.h>
#include <xboxmath.h>

namespace CgsGeometric
{
    // unk_83039220 -- ZERO in the image; written at static-init by sub_82C6DCF0
    // (CgsNumeric::CreateFloatVector(0.001f) -> stvx128). The PS3 initializer
    // builds the identical splat. Modelled at its post-init value; a namespace
    // const here because the only consumer is this TU (the X360 kernel is its
    // single exported reader).
    const VecFloat KF_MIN_PLANE_DIST(0.001f);

    Triangle4::Mask4 IntersectTriangle4Sphere_HackyBurnoutVersion(
        const Sphere&    lSphere,
        const Triangle4& lTriangles,
        VecFloat         lInPadding,
        Vector3& lContactNormal0, Vector3& lTriangleNormal0,
        Vector3Plus& lSphereContactPoint0, Vector3Plus& lTriangleContactPoint0,
        Vector3& lContactNormal1, Vector3& lTriangleNormal1,
        Vector3Plus& lSphereContactPoint1, Vector3Plus& lTriangleContactPoint1,
        Vector3& lContactNormal2, Vector3& lTriangleNormal2,
        Vector3Plus& lSphereContactPoint2, Vector3Plus& lTriangleContactPoint2,
        Vector3& lContactNormal3, Vector3& lTriangleNormal3,
        Vector3Plus& lSphereContactPoint3, Vector3Plus& lTriangleContactPoint3)
    {
        // ARTIST 8283D2E0: straight-line VMX operations and full-lane output stores.
        __vector4 v[128];
        v[1] = lInPadding.mV;
        v[6] = lTriangles.mVertex0X.mV; // 8283D2EC
        v[11] = lSphere.mData.mV; // 8283D2F4
        v[20] = __vspltw(v[11], 1); // 8283D2FC
        v[19] = __vspltw(v[11], 2); // 8283D300
        v[18] = __vspltw(v[11], 3); // 8283D308
        v[29] = lTriangles.mVertex1X.mV; // 8283D310
        v[21] = __vspltw(v[11], 0); // 8283D314
        v[0] = __vsubfp(v[29], v[6]); // 8283D318
        v[5] = lTriangles.mVertex0Y.mV; // 8283D31C
        v[28] = lTriangles.mVertex1Y.mV; // 8283D320
        v[13] = __vsubfp(v[28], v[5]); // 8283D328
        v[3] = lTriangles.mVertex0Z.mV; // 8283D32C
        v[27] = lTriangles.mVertex1Z.mV; // 8283D330
        v[12] = __vsubfp(v[27], v[3]); // 8283D338
        v[10] = __vspltisw(1); // 8283D340
        __vector4 spill30 = v[1]; // 8283D348
        v[26] = lTriangles.mVertex2X.mV; // 8283D34C
        v[9] = __vsubfp(v[26], v[29]); // 8283D350
        __vector4 spill90 = v[18]; // 8283D358
        v[30] = __vsubfp(v[6], v[26]); // 8283D35C
        v[25] = lTriangles.mVertex2Y.mV; // 8283D360
        v[8] = __vsubfp(v[25], v[28]); // 8283D364
        v[24] = lTriangles.mVertex2Z.mV; // 8283D368
        v[23] = __vsubfp(v[5], v[25]); // 8283D36C
        __vector4 spill80 = v[19]; // 8283D374
        v[22] = __vsubfp(v[3], v[24]); // 8283D378
        __vector4 spill50 = v[20]; // 8283D380
        v[11] = __vmulfp(v[0], v[0]); // 8283D384
        __vector4 spill60 = v[24]; // 8283D38C
        v[7] = __vsubfp(v[24], v[27]); // 8283D390
        v[24] = v[13]; // 8283D394
        v[4] = __vmulfp(v[9], v[9]); // 8283D398
        v[2] = __vmulfp(v[30], v[30]); // 8283D39C
        v[11] = __vmaddfp(v[13], v[13], v[11]); // 8283D3A0
        v[11] = __vmaddfp(v[12], v[12], v[11]); // 8283D3A4
        v[44] = v[11]; // 8283D3A8
        v[11] = __vmaddfp(v[8], v[8], v[4]); // 8283D3AC
        v[4] = __vmaddfp(v[23], v[23], v[2]); // 8283D3B0
        v[17] = __vor(v[44], v[44]); // 8283D3B4
        v[11] = __vmaddfp(v[7], v[7], v[11]); // 8283D3B8
        v[4] = __vmaddfp(v[22], v[22], v[4]); // 8283D3BC
        v[43] = v[11]; // 8283D3C0
        v[11] = __vcfsx(v[10], 1); // 8283D3C4
        v[42] = v[4]; // 8283D3C8
        v[4] = __vrsqrtefp(v[44]); // 8283D3CC
        v[10] = __vcfsx(v[10], 0); // 8283D3D0
        v[2] = __vrsqrtefp(v[43]); // 8283D3D4
        v[16] = __vor(v[43], v[43]); // 8283D3D8
        v[31] = __vrsqrtefp(v[42]); // 8283D3DC
        v[15] = __vor(v[42], v[42]); // 8283D3E0
        v[14] = __vmulfp(v[4], v[4]); // 8283D3E4
        v[1] = __vmulfp(v[4], v[11]); // 8283D3E8
        v[19] = __vmulfp(v[2], v[11]); // 8283D3EC
        v[18] = __vmulfp(v[31], v[31]); // 8283D3F0
        v[20] = __vmulfp(v[31], v[11]); // 8283D3F4
        v[17] = __vnmsubfp(v[17], v[14], v[10]); // 8283D3F8
        v[14] = __vmulfp(v[2], v[2]); // 8283D3FC
        v[15] = __vnmsubfp(v[15], v[18], v[10]); // 8283D400
        v[17] = __vmaddfp(v[17], v[1], v[4]); // 8283D404
        v[4] = __vnmsubfp(v[16], v[14], v[10]); // 8283D408
        v[15] = __vmaddfp(v[15], v[20], v[31]); // 8283D40C
        v[20] = v[0]; // 8283D410
        v[31] = __vmulfp(v[13], v[7]); // 8283D414
        v[14] = __vmulfp(v[0], v[17]); // 8283D418
        v[16] = __vmaddfp(v[4], v[19], v[2]); // 8283D41C
        v[19] = __vmulfp(v[12], v[9]); // 8283D420
        v[63] = __vmulfp(v[13], v[17]); // 8283D424
        v[0] = __vmulfp(v[0], v[8]); // 8283D428
        v[4] = __vnmsubfp(v[12], v[8], v[31]); // 8283D42C
        v[62] = __vmulfp(v[12], v[17]); // 8283D430
        v[61] = __vmulfp(v[9], v[16]); // 8283D434
        v[13] = __vnmsubfp(v[20], v[7], v[19]); // 8283D438
        v[60] = __vmulfp(v[8], v[16]); // 8283D43C
        v[12] = __vnmsubfp(v[24], v[9], v[0]); // 8283D440
        v[31] = __vor(v[63], v[63]); // 8283D444
        v[0] = __vmulfp(v[4], v[4]); // 8283D448
        __vector4 spilld0 = v[15]; // 8283D450
        v[59] = __vmulfp(v[7], v[16]); // 8283D454
        __vector4 spillc0 = v[14]; // 8283D45C
        v[57] = __vmulfp(v[23], v[15]); // 8283D460
        v[23] = __vor(v[61], v[61]); // 8283D464
        v[56] = __vmulfp(v[22], v[15]); // 8283D468
        __vector4 spilla0 = v[16]; // 8283D470
        v[58] = __vmulfp(v[30], v[15]); // 8283D474
        __vector4 spillb0 = v[17]; // 8283D47C
        __vector4 spill70 = v[25]; // 8283D484
        v[22] = __vor(v[60], v[60]); // 8283D488
        v[0] = __vmaddfp(v[13], v[13], v[0]); // 8283D48C
        v[30] = __vor(v[59], v[59]); // 8283D490
        v[55] = __vor(v[56], v[56]); // 8283D494
        v[0] = __vmaddfp(v[12], v[12], v[0]); // 8283D498
        v[9] = v[0]; // 8283D49C
        v[0] = __vrsqrtefp(v[0]); // 8283D4A0
        v[8] = __vmulfp(v[0], v[0]); // 8283D4A4
        v[7] = __vmulfp(v[0], v[11]); // 8283D4A8
        v[9] = __vnmsubfp(v[9], v[8], v[10]); // 8283D4AC
        v[8] = __vor(v[62], v[62]); // 8283D4B0
        v[9] = __vmaddfp(v[9], v[7], v[0]); // 8283D4B4
        v[0] = __vmulfp(v[4], v[9]); // 8283D4B8
        v[12] = __vmulfp(v[12], v[9]); // 8283D4BC
        v[13] = __vmulfp(v[13], v[9]); // 8283D4C0
        v[9] = __vmulfp(v[0], v[6]); // 8283D4C4
        v[7] = v[0]; // 8283D4C8
        v[4] = __vmulfp(v[63], v[12]); // 8283D4CC
        v[1] = __vmulfp(v[14], v[13]); // 8283D4D0
        v[54] = v[13]; // 8283D4D4
        v[48] = __vmulfp(v[57], v[12]); // 8283D4D8
        v[2] = __vmulfp(v[62], v[0]); // 8283D4DC
        v[46] = __vmulfp(v[58], v[13]); // 8283D4E0
        v[47] = __vmulfp(v[56], v[0]); // 8283D4E4
        v[24] = __vmulfp(v[60], v[12]); // 8283D4E8
        v[20] = __vmulfp(v[59], v[0]); // 8283D4EC
        v[18] = __vmulfp(v[61], v[13]); // 8283D4F0
        v[9] = __vmaddfp(v[13], v[5], v[9]); // 8283D4F4
        v[4] = __vnmsubfp(v[8], v[13], v[4]); // 8283D4F8
        v[8] = __vor(v[58], v[58]); // 8283D4FC
        v[31] = __vnmsubfp(v[31], v[0], v[1]); // 8283D500
        v[48] = __vnmsubfp(v[55], v[54], v[48]); // 8283D504
        v[54] = __vor(v[57], v[57]); // 8283D508
        v[2] = __vnmsubfp(v[14], v[12], v[2]); // 8283D50C
        v[46] = __vnmsubfp(v[54], v[7], v[46]); // 8283D510
        v[30] = __vnmsubfp(v[30], v[13], v[24]); // 8283D514
        v[23] = __vnmsubfp(v[23], v[12], v[20]); // 8283D518
        v[22] = __vnmsubfp(v[22], v[0], v[18]); // 8283D51C
        v[9] = __vmaddfp(v[12], v[3], v[9]); // 8283D520
        v[1] = __vmulfp(v[4], v[4]); // 8283D524
        v[7] = __vmulfp(v[48], v[48]); // 8283D528
        v[25] = __vmulfp(v[30], v[30]); // 8283D52C
        v[55] = v[9]; // 8283D530
        v[9] = v[12]; // 8283D534
        v[47] = __vnmsubfp(v[8], v[9], v[47]); // 8283D538
        v[9] = __vmaddfp(v[2], v[2], v[1]); // 8283D53C
        v[8] = __vmaddfp(v[23], v[23], v[25]); // 8283D540
        v[7] = __vmaddfp(v[47], v[47], v[7]); // 8283D544
        v[9] = __vmaddfp(v[31], v[31], v[9]); // 8283D548
        v[8] = __vmaddfp(v[22], v[22], v[8]); // 8283D54C
        v[54] = v[7]; // 8283D550
        v[7] = v[9]; // 8283D554
        v[9] = __vrsqrtefp(v[9]); // 8283D558
        v[1] = v[8]; // 8283D55C
        v[8] = __vrsqrtefp(v[8]); // 8283D560
        v[54] = __vmaddfp(v[46], v[46], v[54]); // 8283D564
        __vector4 spill40 = v[7]; // 8283D56C
        v[25] = __vmulfp(v[9], v[9]); // 8283D570
        v[20] = __vmulfp(v[9], v[11]); // 8283D574
        v[24] = __vmulfp(v[8], v[8]); // 8283D578
        v[19] = __vmulfp(v[8], v[11]); // 8283D57C
        v[7] = __vrsqrtefp(v[54]); // 8283D580
        v[1] = __vnmsubfp(v[1], v[24], v[10]); // 8283D584
        v[8] = __vmaddfp(v[1], v[19], v[8]); // 8283D588
        v[1] = __vmulfp(v[7], v[7]); // 8283D58C
        v[18] = spill40; // 8283D594
        v[25] = __vnmsubfp(v[18], v[25], v[10]); // 8283D598
        v[9] = __vmaddfp(v[25], v[20], v[9]); // 8283D59C
        v[25] = __vor(v[54], v[54]); // 8283D5A0
        v[54] = __vmulfp(v[4], v[9]); // 8283D5A4
        v[24] = __vmulfp(v[7], v[11]); // 8283D5A8
        v[20] = spill50; // 8283D5B0
        v[52] = __vmulfp(v[2], v[9]); // 8283D5B4
        v[19] = spill80; // 8283D5BC
        v[50] = __vmulfp(v[31], v[9]); // 8283D5C0
        v[4] = __vnmsubfp(v[25], v[1], v[10]); // 8283D5C8
        v[25] = spill70; // 8283D5D0
        v[53] = __vmulfp(v[30], v[8]); // 8283D5D4
        v[51] = __vmulfp(v[23], v[8]); // 8283D5DC
        v[49] = __vmulfp(v[22], v[8]); // 8283D5E0
        v[8] = __vmulfp(v[21], v[0]); // 8283D5E4
        v[17] = __vmulfp(v[44], v[17]); // 8283D5E8
        v[38] = KF_MIN_PLANE_DIST.mV; // 8283D5EC
        v[9] = __vmaddfp(v[4], v[24], v[7]); // 8283D5F0
        v[24] = spill60; // 8283D5F8
        v[7] = __vmulfp(v[54], v[6]); // 8283D5FC
        v[2] = __vmulfp(v[53], v[29]); // 8283D600
        v[48] = __vmulfp(v[48], v[9]); // 8283D604
        v[46] = __vmulfp(v[46], v[9]); // 8283D608
        v[47] = __vmulfp(v[47], v[9]); // 8283D60C
        v[7] = __vmaddfp(v[52], v[5], v[7]); // 8283D610
        v[2] = __vmaddfp(v[51], v[28], v[2]); // 8283D614
        v[9] = __vmulfp(v[48], v[26]); // 8283D618
        v[4] = v[7]; // 8283D61C
        v[31] = v[2]; // 8283D620
        v[4] = __vmaddfp(v[50], v[3], v[4]); // 8283D624
        v[31] = __vmaddfp(v[49], v[27], v[31]); // 8283D628
        v[9] = __vmaddfp(v[47], v[25], v[9]); // 8283D62C
        v[22] = v[4]; // 8283D630
        v[37] = v[31]; // 8283D634
        v[39] = v[9]; // 8283D638
        v[9] = __vmaddfp(v[20], v[13], v[8]); // 8283D63C
        v[39] = __vmaddfp(v[46], v[24], v[39]); // 8283D640
        v[9] = __vmaddfp(v[19], v[12], v[9]); // 8283D644
        v[1] = __vor(v[39], v[39]); // 8283D648
        v[45] = v[9]; // 8283D64C
        v[9] = __vmulfp(v[45], v[0]); // 8283D650
        v[8] = __vmulfp(v[45], v[13]); // 8283D654
        v[7] = __vmulfp(v[45], v[12]); // 8283D658
        v[9] = __vsubfp(v[21], v[9]); // 8283D65C
        v[8] = __vsubfp(v[20], v[8]); // 8283D660
        v[7] = __vsubfp(v[19], v[7]); // 8283D664
        v[9] = __vmaddfp(v[0], v[55], v[9]); // 8283D668
        v[8] = __vmaddfp(v[13], v[55], v[8]); // 8283D66C
        v[7] = __vmaddfp(v[12], v[55], v[7]); // 8283D670
        v[2] = __vmulfp(v[9], v[54]); // 8283D674
        v[23] = __vmulfp(v[9], v[53]); // 8283D678
        v[36] = __vmulfp(v[9], v[48]); // 8283D67C
        v[2] = __vmaddfp(v[8], v[52], v[2]); // 8283D680
        v[23] = __vmaddfp(v[8], v[51], v[23]); // 8283D684
        v[36] = __vmaddfp(v[8], v[47], v[36]); // 8283D688
        v[30] = v[2]; // 8283D68C
        v[23] = __vmaddfp(v[7], v[49], v[23]); // 8283D690
        v[30] = __vmaddfp(v[7], v[50], v[30]); // 8283D694
        v[31] = __vcmpgtfp(v[23], v[31]); // 8283D698
        v[35] = v[23]; // 8283D69C
        v[2] = __vcmpgtfp(v[30], v[4]); // 8283D6A0
        v[40] = v[30]; // 8283D6A4
        v[4] = __vspltisw(0); // 8283D6A8
        v[41] = v[2]; // 8283D6AC
        v[41] = __vsel(v[40], v[22], v[41]); // 8283D6B0
        v[40] = v[31]; // 8283D6B4
        v[22] = __vor(v[36], v[36]); // 8283D6B8
        v[36] = __vmulfp(v[30], v[52]); // 8283D6BC
        v[40] = __vsel(v[35], v[37], v[40]); // 8283D6C0
        v[37] = __vmulfp(v[30], v[54]); // 8283D6C4
        v[30] = __vmulfp(v[30], v[50]); // 8283D6C8
        v[35] = __vmulfp(v[23], v[53]); // 8283D6CC
        v[22] = __vmaddfp(v[7], v[46], v[22]); // 8283D6D0
        v[36] = __vsubfp(v[8], v[36]); // 8283D6D4
        v[37] = __vsubfp(v[9], v[37]); // 8283D6D8
        v[30] = __vsubfp(v[7], v[30]); // 8283D6DC
        v[36] = __vmaddfp(v[52], v[41], v[36]); // 8283D6E0
        v[52] = __vsubfp(v[9], v[35]); // 8283D6E4
        v[37] = __vmaddfp(v[54], v[41], v[37]); // 8283D6E8
        v[54] = __vmulfp(v[23], v[51]); // 8283D6EC
        v[30] = __vmaddfp(v[50], v[41], v[30]); // 8283D6F0
        v[50] = __vmulfp(v[43], v[16]); // 8283D6F4
        v[16] = __vsubfp(v[37], v[6]); // 8283D6F8
        v[44] = __vsubfp(v[36], v[5]); // 8283D6FC
        v[43] = __vsubfp(v[30], v[3]); // 8283D700
        v[30] = v[22]; // 8283D704
        v[52] = __vmaddfp(v[53], v[40], v[52]); // 8283D708
        v[54] = __vsubfp(v[8], v[54]); // 8283D70C
        v[23] = __vmulfp(v[23], v[49]); // 8283D710
        spillc0 = v[30]; // 8283D718
        v[30] = __vcmpgtfp(v[22], v[39]); // 8283D71C
        v[15] = __vmulfp(v[42], v[15]); // 8283D720
        v[16] = __vmulfp(v[16], v[14]); // 8283D724
        v[53] = __vsubfp(v[52], v[29]); // 8283D728
        v[52] = __vmulfp(v[22], v[48]); // 8283D72C
        v[54] = __vmaddfp(v[51], v[40], v[54]); // 8283D730
        v[23] = __vsubfp(v[7], v[23]); // 8283D734
        v[16] = __vmaddfp(v[44], v[63], v[16]); // 8283D738
        v[44] = __vmulfp(v[22], v[47]); // 8283D73C
        v[53] = __vmulfp(v[53], v[61]); // 8283D740
        v[52] = __vsubfp(v[9], v[52]); // 8283D744
        v[54] = __vsubfp(v[54], v[28]); // 8283D748
        v[23] = __vmaddfp(v[49], v[40], v[23]); // 8283D74C
        v[16] = __vmaddfp(v[43], v[62], v[16]); // 8283D750
        v[51] = __vsubfp(v[8], v[44]); // 8283D754
        v[53] = __vmaddfp(v[54], v[60], v[53]); // 8283D758
        v[23] = __vsubfp(v[23], v[27]); // 8283D75C
        v[16] = __vmaxfp(v[16], v[4]); // 8283D760
        v[53] = __vmaddfp(v[23], v[59], v[53]); // 8283D764
        v[17] = __vminfp(v[16], v[17]); // 8283D768
        v[16] = v[22]; // 8283D76C
        v[22] = __vmulfp(v[22], v[46]); // 8283D770
        v[16] = __vsel(v[16], v[1], v[30]); // 8283D774
        v[23] = __vmaxfp(v[53], v[4]); // 8283D778
        v[52] = __vmaddfp(v[48], v[16], v[52]); // 8283D77C
        v[51] = __vmaddfp(v[47], v[16], v[51]); // 8283D780
        v[6] = __vmaddfp(v[14], v[17], v[6]); // 8283D784
        v[22] = __vsubfp(v[7], v[22]); // 8283D788
        v[5] = __vmaddfp(v[63], v[17], v[5]); // 8283D78C
        v[3] = __vmaddfp(v[62], v[17], v[3]); // 8283D790
        v[14] = __vsubfp(v[51], v[25]); // 8283D794
        v[9] = __vsel(v[9], v[6], v[2]); // 8283D798
        v[22] = __vmaddfp(v[46], v[16], v[22]); // 8283D79C
        v[16] = __vsubfp(v[52], v[26]); // 8283D7A0
        v[8] = __vsel(v[8], v[5], v[2]); // 8283D7A4
        v[5] = __vspltisw(-1); // 8283D7A8
        spilld0 = v[9]; // 8283D7B0
        v[9] = __vminfp(v[23], v[50]); // 8283D7B4
        v[5] = __vcfsx(v[5], 0); // 8283D7B8
        v[22] = __vsubfp(v[22], v[24]); // 8283D7BC
        v[6] = __vmulfp(v[16], v[58]); // 8283D7C0
        v[29] = __vmaddfp(v[61], v[9], v[29]); // 8283D7C4
        v[28] = __vmaddfp(v[60], v[9], v[28]); // 8283D7C8
        v[27] = __vmaddfp(v[59], v[9], v[27]); // 8283D7CC
        v[9] = __vsel(v[7], v[3], v[2]); // 8283D7D0
        v[6] = __vmaddfp(v[14], v[57], v[6]); // 8283D7D4
        v[8] = __vsel(v[8], v[28], v[31]); // 8283D7D8
        v[9] = __vsel(v[9], v[27], v[31]); // 8283D7DC
        v[6] = __vmaddfp(v[22], v[56], v[6]); // 8283D7E0
        v[6] = __vmaxfp(v[6], v[4]); // 8283D7E4
        v[23] = spilld0; // 8283D7EC
        v[29] = __vsel(v[23], v[29], v[31]); // 8283D7F0
        v[6] = __vminfp(v[6], v[15]); // 8283D7F4
        v[26] = __vmaddfp(v[58], v[6], v[26]); // 8283D7F8
        v[25] = __vmaddfp(v[57], v[6], v[25]); // 8283D7FC
        v[24] = __vmaddfp(v[56], v[6], v[24]); // 8283D800
        v[26] = __vsel(v[29], v[26], v[30]); // 8283D804
        v[25] = __vsel(v[8], v[25], v[30]); // 8283D808
        v[24] = __vsel(v[9], v[24], v[30]); // 8283D80C
        v[8] = __vsubfp(v[26], v[21]); // 8283D810
        v[7] = __vsubfp(v[25], v[20]); // 8283D814
        v[6] = __vsubfp(v[24], v[19]); // 8283D818
        v[9] = __vmulfp(v[8], v[8]); // 8283D81C
        v[9] = __vmaddfp(v[7], v[7], v[9]); // 8283D820
        v[23] = __vmaddfp(v[6], v[6], v[9]); // 8283D824
        v[9] = __vrsqrtefp(v[23]); // 8283D828
        v[29] = __vmulfp(v[9], v[9]); // 8283D82C
        v[11] = __vmulfp(v[9], v[11]); // 8283D830
        v[10] = __vnmsubfp(v[23], v[29], v[10]); // 8283D834
        v[3] = __vmaddfp(v[10], v[11], v[9]); // 8283D838
        v[11] = __vnor(v[4], v[4]); // 8283D83C
        v[11] = __vmrglw(v[11], v[4]); // 8283D840
        v[9] = __vmulfp(v[7], v[3]); // 8283D844
        v[18] = spill90; // 8283D84C
        v[10] = __vmulfp(v[8], v[3]); // 8283D850
        v[8] = __vpermwi(v[11], 39); // 8283D854
        v[27] = __vmulfp(v[6], v[3]); // 8283D858
        spill80 = v[4]; // 8283D860
        spillc0 = v[3]; // 8283D868
        spilla0 = v[23]; // 8283D874
        v[26] = __vpermwi(v[26], 27); // 8283D878
        v[11] = __vand(v[8], v[11]); // 8283D87C
        spill90 = v[12]; // 8283D884
        v[25] = __vpermwi(v[25], 198); // 8283D888
        v[24] = __vpermwi(v[24], 177); // 8283D88C
        v[8] = __vpermwi(v[11], 108); // 8283D890
        v[17] = __vmulfp(v[9], v[5]); // 8283D894
        v[9] = __vmaddfp(v[9], v[18], v[20]); // 8283D898
        v[22] = __vmulfp(v[10], v[5]); // 8283D89C
        v[16] = __vmulfp(v[27], v[5]); // 8283D8A0
        v[10] = __vmaddfp(v[10], v[18], v[21]); // 8283D8A4
        v[27] = __vmaddfp(v[27], v[18], v[19]); // 8283D8A8
        v[6] = __vpermwi(v[17], 198); // 8283D8AC
        v[28] = __vpermwi(v[9], 198); // 8283D8B0
        v[7] = __vpermwi(v[22], 27); // 8283D8B4
        v[9] = __vpermwi(v[11], 99); // 8283D8B8
        spilld0 = v[16]; // 8283D8C0
        v[5] = __vpermwi(v[16], 177); // 8283D8C4
        spillb0 = v[18]; // 8283D8CC
        v[29] = __vpermwi(v[10], 27); // 8283D8D0
        v[10] = __vpermwi(v[11], 75); // 8283D8D4
        v[19] = __vsel(v[7], v[6], v[8]); // 8283D8D8
        v[21] = __vsel(v[7], v[6], v[9]); // 8283D8DC
        v[27] = __vpermwi(v[27], 177); // 8283D8E0
        v[3] = __vsel(v[7], v[6], v[11]); // 8283D8E4
        v[12] = __vsel(v[7], v[6], v[9]); // 8283D8E8
        v[4] = __vsel(v[7], v[6], v[10]); // 8283D8EC
        v[21] = __vsel(v[21], v[5], v[8]); // 8283D8F0
        v[20] = __vsel(v[19], v[5], v[11]); // 8283D8F4
        v[12] = __vsel(v[12], v[5], v[8]); // 8283D8F8
        v[14] = __vsel(v[4], v[5], v[9]); // 8283D8FC
        v[21] = __vpermwi(v[21], 108); // 8283D900
        v[4] = __vsel(v[3], v[5], v[10]); // 8283D904
        v[20] = __vpermwi(v[20], 177); // 8283D908
        v[19] = __vsel(v[7], v[6], v[10]); // 8283D90C
        v[63] = __vpermwi(v[12], 108); // 8283D910
        lTriangleNormal0.mV = v[14]; // 8283D914
        v[15] = __vpermwi(v[4], 198); // 8283D918
        v[19] = __vsel(v[19], v[5], v[9]); // 8283D91C
        lTriangleNormal1.mV = v[21]; // 8283D920
        v[21] = __vsel(v[7], v[6], v[8]); // 8283D924
        lTriangleNormal2.mV = v[20]; // 8283D928
        v[7] = __vsel(v[7], v[6], v[11]); // 8283D930
        v[6] = __vsel(v[29], v[28], v[9]); // 8283D934
        v[21] = __vsel(v[21], v[5], v[11]); // 8283D938
        v[20] = __vsel(v[29], v[28], v[10]); // 8283D93C
        lTriangleNormal3.mV = v[15]; // 8283D940
        v[7] = __vsel(v[7], v[5], v[10]); // 8283D948
        lContactNormal0.mV = v[19]; // 8283D94C
        v[21] = __vpermwi(v[21], 177); // 8283D950
        lContactNormal1.mV = v[63]; // 8283D954
        v[6] = __vsel(v[6], v[27], v[8]); // 8283D958
        v[15] = __vsel(v[29], v[28], v[8]); // 8283D95C
        v[7] = __vpermwi(v[7], 198); // 8283D960
        v[29] = __vsel(v[29], v[28], v[11]); // 8283D964
        v[28] = __vsel(v[20], v[27], v[9]); // 8283D968
        v[6] = __vpermwi(v[6], 108); // 8283D96C
        v[19] = __vsel(v[26], v[25], v[10]); // 8283D970
        v[5] = __vsel(v[15], v[27], v[11]); // 8283D974
        lContactNormal2.mV = v[21]; // 8283D978
        v[29] = __vsel(v[29], v[27], v[10]); // 8283D980
        v[27] = __vsel(v[19], v[24], v[9]); // 8283D984
        v[5] = __vpermwi(v[5], 177); // 8283D988
        lContactNormal3.mV = v[7]; // 8283D98C
        lSphereContactPoint0.mV = v[28]; // 8283D994
        v[29] = __vpermwi(v[29], 198); // 8283D998
        lSphereContactPoint1.mV = v[6]; // 8283D99C
        v[0] = __vmulfp(v[22], v[0]); // 8283D9A4
        lSphereContactPoint2.mV = v[5]; // 8283D9A8
        v[7] = __vnor(v[31], v[31]); // 8283D9B0
        v[31] = __vsel(v[26], v[25], v[11]); // 8283D9B4
        v[12] = spill90; // 8283D9BC
        v[1] = spill30; // 8283D9C8
        v[3] = spillc0; // 8283D9D0
        v[6] = __vnor(v[2], v[2]); // 8283D9D8
        v[4] = spill80; // 8283D9E0
        lSphereContactPoint3.mV = v[29]; // 8283D9E4
        v[29] = __vsel(v[26], v[25], v[9]); // 8283D9E8
        lTriangleContactPoint0.mV = v[27]; // 8283D9EC
        v[9] = __vnor(v[30], v[30]); // 8283D9F8
        v[0] = __vmaddfp(v[17], v[13], v[0]); // 8283D9FC
        v[13] = __vsel(v[26], v[25], v[8]); // 8283DA00
        v[8] = __vsel(v[29], v[24], v[8]); // 8283DA04
        v[13] = __vsel(v[13], v[24], v[11]); // 8283DA08
        v[11] = __vpermwi(v[8], 108); // 8283DA0C
        v[8] = __vaddfp(v[18], v[1]); // 8283DA10
        v[13] = __vpermwi(v[13], 177); // 8283DA14
        v[0] = __vmaddfp(v[16], v[12], v[0]); // 8283DA18
        lTriangleContactPoint1.mV = v[11]; // 8283DA1C
        v[12] = __vsel(v[31], v[24], v[10]); // 8283DA24
        v[10] = __vmulfp(v[23], v[3]); // 8283DA28
        v[12] = __vpermwi(v[12], 198); // 8283DA2C
        lTriangleContactPoint2.mV = v[13]; // 8283DA30
        v[13] = __vand(v[6], v[7]); // 8283DA38
        v[13] = __vand(v[13], v[9]); // 8283DA3C
        lTriangleContactPoint3.mV = v[12]; // 8283DA40
        v[12] = lTriangles.mEdge2Cosigns.mV; // 8283DA44
        v[11] = lTriangles.mEdge1Cosigns.mV; // 8283DA4C
        v[12] = __vcmpgefp(v[0], v[12]); // 8283DA50
        v[11] = __vcmpgefp(v[0], v[11]); // 8283DA54
        v[5] = lTriangles.mEdge0Cosigns.mV; // 8283DA58
        v[10] = __vcmpgefp(v[8], v[10]); // 8283DA5C
        v[13] = __vnor(v[13], v[13]); // 8283DA60
        v[8] = __vcmpgefp(v[0], v[5]); // 8283DA64
        v[0] = __vcmpgefp(v[0], v[4]); // 8283DA68
        v[12] = __vor(v[9], v[12]); // 8283DA6C
        v[11] = __vor(v[7], v[11]); // 8283DA70
        v[9] = __vor(v[6], v[8]); // 8283DA74
        v[0] = __vor(v[13], v[0]); // 8283DA78
        v[12] = __vand(v[11], v[12]); // 8283DA7C
        v[11] = __vsubfp(v[45], v[55]); // 8283DA80
        v[12] = __vand(v[9], v[12]); // 8283DA84
        v[9] = lTriangles.mValidMasks.mV; // 8283DA88
        v[0] = __vand(v[0], v[12]); // 8283DA8C
        v[13] = __vcmpgefp(v[11], v[38]); // 8283DA90
        v[0] = __vand(v[13], v[0]); // 8283DA94
        v[0] = __vand(v[9], v[0]); // 8283DA98
        v[1] = __vand(v[0], v[10]); // 8283DA9C
        return Triangle4::Mask4(v[1]);
    }
}

// The swept Triangle4 path uses the original 2D-circle helper and VMX order.
namespace CgsGeometric
{
    __vector4 Intersect2DCircleWithTriangleSOA(__vector4 a1, __vector4 a2, __vector4 a3, __vector4 a4, __vector4 a5, __vector4 a6, __vector4 a7, __vector4 a8, __vector4 a9, __vector4 a10, __vector4 a11, __vector4 a12, __vector4 a13, __vector4 a14, __vector4 a15, __vector4 a16, __vector4 a17, __vector4 a18, __vector4& out0, __vector4& out1)
    {
        __vector4 v[128];
        v[1] = a1;
        v[2] = a2;
        v[3] = a3;
        v[4] = a4;
        v[5] = a5;
        v[6] = a6;
        v[7] = a7;
        v[8] = a8;
        v[9] = a9;
        v[10] = a10;
        v[11] = a11;
        v[12] = a12;
        v[13] = a13;
        __vector4 stack224 = a14;
        __vector4 stack240 = a15;
        __vector4 stack256 = a16;
        __vector4 stack272 = a17;
        __vector4 stack288 = a18;
        v[27] = __vsubfp(v[7], v[2]); // 82839AC0
        v[26] = __vsubfp(v[5], v[2]); // 82839AC8
        v[0] = __vspltisw(0); // 82839ACC
        v[25] = __vsubfp(v[9], v[2]); // 82839AD0
        v[15] = v[3]; // 82839AD4
        v[24] = __vsubfp(v[6], v[1]); // 82839AD8
        v[22] = v[1]; // 82839ADC
        v[23] = __vsubfp(v[4], v[1]); // 82839AE0
        v[62] = v[5]; // 82839AE4
        v[21] = stack224; // 82839AE8
        v[16] = __vsubfp(v[8], v[1]); // 82839AEC
        v[31] = __vsubfp(v[1], v[4]); // 82839AF4
        v[30] = __vsubfp(v[1], v[6]); // 82839AF8
        __vector4 stackm48 = v[22]; // 82839B00
        v[29] = __vsubfp(v[2], v[5]); // 82839B04
        v[61] = v[2]; // 82839B08
        v[28] = __vsubfp(v[2], v[7]); // 82839B0C
        v[60] = v[6]; // 82839B10
        v[19] = __vsubfp(v[1], v[8]); // 82839B14
        v[59] = v[7]; // 82839B18
        v[20] = stack240; // 82839B1C
        v[18] = __vsubfp(v[2], v[9]); // 82839B20
        v[27] = __vmulfp(v[12], v[27]); // 82839B24
        v[26] = __vmulfp(v[10], v[26]); // 82839B28
        v[25] = __vmulfp(v[21], v[25]); // 82839B2C
        v[27] = __vnmsubfp(v[13], v[24], v[27]); // 82839B30
        v[26] = __vnmsubfp(v[11], v[23], v[26]); // 82839B34
        v[25] = __vnmsubfp(v[20], v[16], v[25]); // 82839B38
        v[24] = __vmulfp(v[30], v[30]); // 82839B3C
        v[27] = __vcmpgefp(v[27], v[0]); // 82839B40
        v[26] = __vcmpgefp(v[26], v[0]); // 82839B44
        v[25] = __vcmpgefp(v[25], v[0]); // 82839B48
        v[27] = __vand(v[26], v[27]); // 82839B4C
        v[26] = __vmulfp(v[31], v[10]); // 82839B50
        v[23] = __vand(v[27], v[25]); // 82839B54
        v[25] = __vmulfp(v[30], v[12]); // 82839B58
        __vector4 stackm16 = v[23]; // 82839B60
        v[27] = __vmaddfp(v[29], v[11], v[26]); // 82839B64
        v[26] = __vmaddfp(v[28], v[13], v[25]); // 82839B68
        v[25] = __vmulfp(v[31], v[31]); // 82839B6C
        v[17] = __vcmpgefp(v[0], v[27]); // 82839B70
        v[10] = __vmulfp(v[27], v[10]); // 82839B74
        v[11] = __vmulfp(v[27], v[11]); // 82839B78
        v[22] = __vcmpgefp(v[0], v[26]); // 82839B7C
        v[16] = __vmaddfp(v[29], v[29], v[25]); // 82839B80
        v[25] = __vmaddfp(v[28], v[28], v[24]); // 82839B84
        v[24] = __vmulfp(v[19], v[21]); // 82839B88
        v[12] = __vmulfp(v[26], v[12]); // 82839B8C
        v[3] = __vcmpgefp(v[3], v[16]); // 82839B90
        v[23] = v[16]; // 82839B94
        v[16] = v[25]; // 82839B98
        v[24] = __vmaddfp(v[18], v[20], v[24]); // 82839B9C
        __vector4 stackm32 = v[16]; // 82839BA4
        v[3] = __vand(v[3], v[17]); // 82839BA8
        v[16] = __vsel(v[15], v[23], v[3]); // 82839BAC
        v[23] = v[1]; // 82839BB0
        v[15] = __vor(v[0], v[3]); // 82839BB4
        v[63] = v[3]; // 82839BB8
        v[14] = __vsel(v[23], v[4], v[3]); // 82839BBC
        v[3] = v[16]; // 82839BC0
        v[16] = __vcmpgefp(v[16], v[25]); // 82839BC4
        v[23] = v[25]; // 82839BC8
        v[63] = __vsel(v[61], v[62], v[63]); // 82839BCC
        v[61] = v[14]; // 82839BD0
        stackm48 = v[3]; // 82839BD8
        v[3] = __vand(v[16], v[22]); // 82839BDC
        v[62] = v[3]; // 82839BE0
        v[14] = __vor(v[15], v[3]); // 82839BE4
        v[15] = __vmulfp(v[26], v[13]); // 82839BE8
        v[13] = __vor(v[63], v[63]); // 82839BEC
        v[63] = v[3]; // 82839BF0
        v[62] = __vsel(v[61], v[60], v[62]); // 82839BF4
        v[63] = __vsel(v[13], v[59], v[63]); // 82839BF8
        v[16] = stackm48; // 82839C00
        v[16] = __vsel(v[16], v[23], v[3]); // 82839C04
        v[3] = __vmulfp(v[24], v[21]); // 82839C08
        v[21] = __vmulfp(v[24], v[20]); // 82839C0C
        v[61] = __vcmpgefp(v[16], v[25]); // 82839C10
        v[13] = __vand(v[61], v[22]); // 82839C14
        v[31] = __vsubfp(v[31], v[10]); // 82839C18
        stackm48 = v[2]; // 82839C20
        stackm32 = v[9]; // 82839C28
        v[9] = __vor(v[63], v[63]); // 82839C2C
        v[63] = __vsubfp(v[30], v[12]); // 82839C30
        v[29] = __vsubfp(v[29], v[11]); // 82839C38
        v[20] = __vor(v[62], v[62]); // 82839C3C
        v[30] = __vsel(v[16], v[25], v[13]); // 82839C40
        v[62] = __vsubfp(v[28], v[15]); // 82839C44
        v[25] = __vor(v[14], v[13]); // 82839C48
        v[4] = __vaddfp(v[10], v[4]); // 82839C4C
        v[5] = __vaddfp(v[11], v[5]); // 82839C50
        v[23] = stackm16; // 82839C58
        v[28] = __vsel(v[20], v[6], v[13]); // 82839C5C
        v[16] = stack256; // 82839C60
        v[20] = __vsel(v[9], v[7], v[13]); // 82839C64
        v[27] = __vcmpgefp(v[27], v[16]); // 82839C6C
        v[11] = __vsubfp(v[18], v[21]); // 82839C70
        v[0] = __vcmpgefp(v[0], v[24]); // 82839C74
        v[31] = __vmulfp(v[31], v[31]); // 82839C78
        v[14] = stack272; // 82839C7C
        v[13] = __vor(v[63], v[63]); // 82839C84
        v[26] = __vcmpgefp(v[26], v[14]); // 82839C88
        v[13] = __vmulfp(v[13], v[13]); // 82839C8C
        v[29] = __vmaddfp(v[29], v[29], v[31]); // 82839C90
        v[31] = __vor(v[62], v[62]); // 82839C94
        v[31] = __vmaddfp(v[31], v[31], v[13]); // 82839C98
        v[13] = __vnor(v[17], v[27]); // 82839C9C
        v[27] = __vnor(v[22], v[26]); // 82839CA0
        v[26] = __vaddfp(v[12], v[6]); // 82839CA4
        v[12] = __vsubfp(v[19], v[3]); // 82839CA8
        v[17] = stack288; // 82839CAC
        v[22] = __vaddfp(v[15], v[7]); // 82839CB0
        v[16] = __vcmpgefp(v[30], v[29]); // 82839CB4
        v[12] = __vmulfp(v[12], v[12]); // 82839CB8
        v[13] = __vand(v[16], v[13]); // 82839CBC
        v[9] = stackm32; // 82839CC4
        v[10] = __vsel(v[30], v[29], v[13]); // 82839CC8
        v[6] = __vsel(v[28], v[4], v[13]); // 82839CCC
        v[4] = __vcmpgefp(v[24], v[17]); // 82839CD0
        v[7] = __vor(v[25], v[13]); // 82839CD4
        v[5] = __vsel(v[20], v[5], v[13]); // 82839CD8
        v[13] = __vcmpgefp(v[10], v[31]); // 82839CDC
        v[12] = __vmaddfp(v[11], v[11], v[12]); // 82839CE0
        v[11] = __vaddfp(v[21], v[9]); // 82839CE4
        v[0] = __vnor(v[0], v[4]); // 82839CE8
        v[13] = __vand(v[13], v[27]); // 82839CEC
        v[7] = __vor(v[7], v[13]); // 82839CF0
        v[5] = __vsel(v[5], v[22], v[13]); // 82839CF4
        v[6] = __vsel(v[6], v[26], v[13]); // 82839CF8
        v[13] = __vsel(v[10], v[31], v[13]); // 82839CFC
        v[10] = __vaddfp(v[3], v[8]); // 82839D00
        v[13] = __vcmpgefp(v[13], v[12]); // 82839D04
        v[0] = __vand(v[13], v[0]); // 82839D08
        v[13] = __vsel(v[6], v[10], v[0]); // 82839D0C
        v[12] = __vsel(v[5], v[11], v[0]); // 82839D10
        v[0] = __vor(v[7], v[0]); // 82839D14
        v[13] = __vsel(v[13], v[1], v[23]); // 82839D18
        v[12] = __vsel(v[12], v[2], v[23]); // 82839D1C
        v[1] = __vor(v[0], v[23]); // 82839D20
        out0 = v[13]; // 82839D24
        out1 = v[12]; // 82839D28
        return v[1];
    }

    Triangle4::Mask4 IntersectTriangle4SweptSphere(
        const SweptSphere& lSweptSphere,
        const Triangle4&   lTriangles,
        Vector3& lContactNormal0, Vector3& lTriangleNormal0,
        Vector3Plus& lSphereContactPoint0, Vector3Plus& lTriangleContactPoint0,
        Vector3& lContactNormal1, Vector3& lTriangleNormal1,
        Vector3Plus& lSphereContactPoint1, Vector3Plus& lTriangleContactPoint1,
        Vector3& lContactNormal2, Vector3& lTriangleNormal2,
        Vector3Plus& lSphereContactPoint2, Vector3Plus& lTriangleContactPoint2,
        Vector3& lContactNormal3, Vector3& lTriangleNormal3,
        Vector3Plus& lSphereContactPoint3, Vector3Plus& lTriangleContactPoint3)
    {
        const Vector3Plus lPosition = lSweptSphere.GetPositionAndRadius();
        const Vector3Plus lDirection = lSweptSphere.GetDirectionAndLength();
        __vector4 v[128];
        v[11] = lPosition.mV; // 8283EF68
        v[38] = __vspltw(v[11], 0); // 8283EF70
        v[37] = __vspltw(v[11], 1); // 8283EF78
        v[36] = __vspltw(v[11], 2); // 8283EF80
        v[0] = lDirection.mV; // 8283EF88
        v[0] = __vspltw(v[0], 3); // 8283EF90
        v[10] = lDirection.mV; // 8283EF94
        v[126] = lTriangles.mVertex0Y.mV; // 8283EF98
        v[8] = lTriangles.mVertex1X.mV; // 8283EFA0
        v[2] = __vspltw(v[10], 1); // 8283EFAC
        v[9] = __vspltw(v[0], 0); // 8283EFB0
        v[127] = lTriangles.mVertex0X.mV; // 8283EFB4
        v[0] = __vspltw(v[11], 3); // 8283EFB8
        v[6] = lTriangles.mVertex1Z.mV; // 8283EFBC
        v[11] = __vspltw(v[10], 0); // 8283EFC0
        v[5] = lTriangles.mVertex2X.mV; // 8283EFC4
        v[10] = __vspltw(v[10], 2); // 8283EFC8
        v[125] = lTriangles.mVertex0Z.mV; // 8283EFCC
        v[7] = lTriangles.mVertex1Y.mV; // 8283EFD0
        v[12] = __vsubfp(v[6], v[125]); // 8283EFD4
        v[50] = __vspltw(v[0], 0); // 8283EFD8
        v[0] = __vsubfp(v[8], v[127]); // 8283EFDC
        v[8] = __vsubfp(v[5], v[8]); // 8283EFE0
        v[3] = lTriangles.mVertex2Z.mV; // 8283EFE4
        v[122] = __vmulfp(v[10], v[9]); // 8283EFE8
        v[13] = __vsubfp(v[7], v[126]); // 8283EFF0
        v[10] = __vsubfp(v[3], v[6]); // 8283EFF4
        v[124] = __vmulfp(v[11], v[9]); // 8283EFF8
        v[11] = __vspltisw(1); // 8283EFFC
        v[123] = __vmulfp(v[2], v[9]); // 8283F000
        v[4] = lTriangles.mVertex2Y.mV; // 8283F004
        v[9] = __vsubfp(v[4], v[7]); // 8283F008
        v[56] = __vcfsx(v[11], 1); // 8283F00C
        v[57] = __vcfsx(v[11], 0); // 8283F010
        v[6] = v[8]; // 8283F014
        v[8] = __vmulfp(v[12], v[8]); // 8283F018
        v[30] = __vmulfp(v[13], v[10]); // 8283F01C
        v[1] = __vor(v[57], v[57]); // 8283F020
        v[31] = __vor(v[57], v[57]); // 8283F024
        v[10] = __vnmsubfp(v[0], v[10], v[8]); // 8283F028
        v[8] = __vmulfp(v[0], v[9]); // 8283F02C
        v[9] = __vnmsubfp(v[12], v[9], v[30]); // 8283F030
        v[8] = __vnmsubfp(v[13], v[6], v[8]); // 8283F034
        v[11] = __vmulfp(v[9], v[9]); // 8283F038
        v[11] = __vmaddfp(v[10], v[10], v[11]); // 8283F03C
        v[11] = __vmaddfp(v[8], v[8], v[11]); // 8283F040
        v[7] = v[11]; // 8283F044
        v[11] = __vrsqrtefp(v[11]); // 8283F048
        v[6] = __vmulfp(v[11], v[11]); // 8283F04C
        v[2] = __vmulfp(v[11], v[56]); // 8283F050
        v[1] = __vnmsubfp(v[7], v[6], v[1]); // 8283F054
        v[11] = __vmaddfp(v[1], v[2], v[11]); // 8283F058
        v[52] = __vmulfp(v[9], v[11]); // 8283F05C
        v[51] = __vmulfp(v[10], v[11]); // 8283F060
        v[53] = __vmulfp(v[8], v[11]); // 8283F064
        v[10] = __vmulfp(v[0], v[0]); // 8283F068
        v[11] = __vmulfp(v[52], v[127]); // 8283F06C
        v[7] = __vor(v[52], v[52]); // 8283F070
        v[9] = __vor(v[51], v[51]); // 8283F074
        v[8] = __vor(v[53], v[53]); // 8283F078
        v[10] = __vmaddfp(v[13], v[13], v[10]); // 8283F07C
        v[11] = __vmaddfp(v[51], v[126], v[11]); // 8283F080
        v[2] = v[11]; // 8283F084
        v[11] = __vmaddfp(v[12], v[12], v[10]); // 8283F088
        v[2] = __vmaddfp(v[53], v[125], v[2]); // 8283F08C
        v[10] = v[11]; // 8283F090
        v[11] = __vrsqrtefp(v[11]); // 8283F094
        v[6] = __vmulfp(v[11], v[11]); // 8283F098
        v[1] = __vmulfp(v[11], v[56]); // 8283F09C
        v[31] = __vnmsubfp(v[10], v[6], v[31]); // 8283F0A0
        v[11] = __vmaddfp(v[31], v[1], v[11]); // 8283F0A4
        v[49] = __vmulfp(v[13], v[11]); // 8283F0A8
        v[28] = __vor(v[57], v[57]); // 8283F0AC
        v[48] = __vmulfp(v[12], v[11]); // 8283F0B0
        v[58] = __vspltisw(0); // 8283F0B4
        v[47] = __vmulfp(v[0], v[11]); // 8283F0B8
        v[10] = __vmulfp(v[49], v[53]); // 8283F0BC
        v[1] = __vor(v[49], v[49]); // 8283F0C0
        v[11] = __vor(v[48], v[48]); // 8283F0C4
        v[31] = __vmulfp(v[48], v[52]); // 8283F0C8
        v[6] = __vor(v[47], v[47]); // 8283F0CC
        v[30] = __vmulfp(v[47], v[51]); // 8283F0D0
        v[10] = __vnmsubfp(v[11], v[9], v[10]); // 8283F0D4
        v[9] = __vnmsubfp(v[6], v[8], v[31]); // 8283F0D8
        v[8] = __vnmsubfp(v[1], v[7], v[30]); // 8283F0DC
        v[7] = __vspltisw(-1); // 8283F0E0
        v[31] = __vmulfp(v[124], v[52]); // 8283F0E4
        v[30] = __vmulfp(v[38], v[52]); // 8283F0E8
        v[46] = __vcfsx(v[7], 0); // 8283F0EC
        v[7] = __vslw(v[7], v[7]); // 8283F0F0
        v[11] = __vmulfp(v[10], v[10]); // 8283F0F4
        v[31] = __vmaddfp(v[123], v[51], v[31]); // 8283F0F8
        v[30] = __vmaddfp(v[37], v[51], v[30]); // 8283F0FC
        v[11] = __vmaddfp(v[9], v[9], v[11]); // 8283F100
        v[121] = v[31]; // 8283F104
        v[30] = __vmaddfp(v[36], v[53], v[30]); // 8283F108
        v[121] = __vmaddfp(v[122], v[53], v[121]); // 8283F10C
        v[6] = __vmaddfp(v[8], v[8], v[11]); // 8283F110
        v[35] = __vsubfp(v[30], v[2]); // 8283F114
        v[11] = __vrsqrtefp(v[6]); // 8283F118
        v[7] = __vandc(v[35], v[7]); // 8283F11C
        v[7] = __vcmpgefp(v[7], v[50]); // 8283F120
        v[29] = __vmulfp(v[11], v[11]); // 8283F124
        v[1] = __vmulfp(v[11], v[56]); // 8283F128
        v[28] = __vnmsubfp(v[6], v[29], v[28]); // 8283F12C
        v[11] = __vmaddfp(v[28], v[1], v[11]); // 8283F130
        v[1] = __vor(v[57], v[57]); // 8283F134
        v[120] = __vmulfp(v[10], v[11]); // 8283F138
        v[10] = __vmulfp(v[46], v[121]); // 8283F13C
        v[118] = __vmulfp(v[8], v[11]); // 8283F140
        v[119] = __vmulfp(v[9], v[11]); // 8283F144
        v[9] = __vsubfp(v[35], v[50]); // 8283F148
        v[11] = __vrefp(v[10]); // 8283F14C
        v[8] = v[10]; // 8283F150
        v[6] = v[10]; // 8283F154
        v[10] = v[11]; // 8283F158
        v[1] = __vnmsubfp(v[8], v[10], v[1]); // 8283F15C
        v[8] = __vor(v[57], v[57]); // 8283F160
        v[11] = __vmaddfp(v[11], v[1], v[11]); // 8283F164
        v[1] = __vmulfp(v[0], v[47]); // 8283F168
        v[0] = __vmulfp(v[0], v[120]); // 8283F16C
        v[10] = v[11]; // 8283F170
        v[1] = __vmaddfp(v[13], v[49], v[1]); // 8283F174
        v[0] = __vmaddfp(v[13], v[119], v[0]); // 8283F178
        v[8] = __vnmsubfp(v[6], v[10], v[8]); // 8283F17C
        v[6] = __vor(v[57], v[57]); // 8283F180
        v[34] = v[1]; // 8283F184
        v[33] = v[0]; // 8283F188
        v[11] = __vmaddfp(v[11], v[8], v[11]); // 8283F18C
        v[8] = __vsubfp(v[5], v[127]); // 8283F190
        v[5] = __vor(v[58], v[58]); // 8283F194
        v[34] = __vmaddfp(v[12], v[48], v[34]); // 8283F198
        v[33] = __vmaddfp(v[12], v[118], v[33]); // 8283F19C
        v[10] = __vmulfp(v[9], v[11]); // 8283F1A0
        v[9] = __vaddfp(v[35], v[50]); // 8283F1A4
        v[9] = __vmulfp(v[9], v[11]); // 8283F1A8
        v[11] = __vcmpeqfp(v[121], v[58]); // 8283F1AC
        v[13] = __vminfp(v[10], v[9]); // 8283F1B0
        v[10] = __vmaxfp(v[10], v[9]); // 8283F1B4
        v[45] = v[11]; // 8283F1B8
        v[12] = __vnor(v[11], v[11]); // 8283F1BC
        v[0] = v[13]; // 8283F1C0
        v[13] = __vminfp(v[10], v[57]); // 8283F1C4
        v[10] = __vand(v[7], v[11]); // 8283F1C8
        v[0] = __vmaxfp(v[0], v[58]); // 8283F1CC
        v[13] = __vsel(v[13], v[6], v[11]); // 8283F1D0
        v[45] = __vsel(v[0], v[5], v[45]); // 8283F1D4
        v[13] = __vcmpgefp(v[58], v[13]); // 8283F1D8
        v[0] = __vcmpgefp(v[45], v[57]); // 8283F1DC
        v[0] = __vor(v[0], v[13]); // 8283F1E0
        v[0] = __vand(v[0], v[12]); // 8283F1E4
        v[115] = __vor(v[10], v[0]); // 8283F1E8
        v[0] = __vsubfp(v[4], v[126]); // 8283F1EC
        v[27] = __vor(v[57], v[57]); // 8283F1F0
        v[7] = __vmulfp(v[8], v[47]); // 8283F1F4
        v[6] = __vmulfp(v[8], v[120]); // 8283F1FC
        v[10] = __vsubfp(v[38], v[127]); // 8283F204
        v[9] = __vsubfp(v[37], v[126]); // 8283F208
        v[12] = __vsubfp(v[34], v[58]); // 8283F20C
        v[13] = __vsubfp(v[3], v[125]); // 8283F210
        v[11] = __vsubfp(v[33], v[58]); // 8283F214
        v[31] = __vmulfp(v[124], v[47]); // 8283F218
        v[3] = __vsubfp(v[36], v[125]); // 8283F21C
        v[112] = __vmulfp(v[50], v[50]); // 8283F220
        v[7] = __vmaddfp(v[0], v[49], v[7]); // 8283F224
        v[6] = __vmaddfp(v[0], v[119], v[6]); // 8283F228
        v[0] = __vmulfp(v[10], v[47]); // 8283F22C
        v[1] = __vmulfp(v[10], v[120]); // 8283F230
        v[31] = __vmaddfp(v[123], v[49], v[31]); // 8283F234
        v[8] = v[7]; // 8283F238
        v[32] = v[6]; // 8283F23C
        v[0] = __vmaddfp(v[9], v[49], v[0]); // 8283F240
        v[1] = __vmaddfp(v[9], v[119], v[1]); // 8283F244
        v[8] = __vmaddfp(v[13], v[48], v[8]); // 8283F248
        v[32] = __vmaddfp(v[13], v[118], v[32]); // 8283F24C
        v[55] = v[31]; // 8283F250
        v[55] = __vmaddfp(v[122], v[48], v[55]); // 8283F254
        v[117] = v[0]; // 8283F258
        v[0] = __vmulfp(v[12], v[12]); // 8283F25C
        v[116] = v[1]; // 8283F260
        v[10] = __vsubfp(v[8], v[34]); // 8283F264
        v[9] = __vsubfp(v[32], v[33]); // 8283F268
        v[117] = __vmaddfp(v[3], v[48], v[117]); // 8283F26C
        v[116] = __vmaddfp(v[3], v[118], v[116]); // 8283F270
        v[7] = __vsubfp(v[58], v[8]); // 8283F274
        v[6] = __vsubfp(v[58], v[32]); // 8283F278
        v[5] = __vmaddfp(v[11], v[11], v[0]); // 8283F27C
        v[0] = __vmulfp(v[124], v[120]); // 8283F280
        v[13] = __vmulfp(v[10], v[10]); // 8283F284
        v[3] = v[5]; // 8283F288
        v[0] = __vmaddfp(v[123], v[119], v[0]); // 8283F28C
        v[4] = __vmaddfp(v[9], v[9], v[13]); // 8283F290
        v[54] = v[0]; // 8283F294
        v[0] = __vrsqrtefp(v[5]); // 8283F298
        v[13] = __vrsqrtefp(v[4]); // 8283F29C
        v[1] = v[4]; // 8283F2A0
        v[54] = __vmaddfp(v[122], v[118], v[54]); // 8283F2A4
        v[31] = __vmulfp(v[0], v[0]); // 8283F2A8
        v[30] = __vmulfp(v[0], v[56]); // 8283F2AC
        v[29] = __vmulfp(v[13], v[13]); // 8283F2B0
        v[28] = __vmulfp(v[13], v[56]); // 8283F2B4
        v[27] = __vnmsubfp(v[3], v[31], v[27]); // 8283F2B8
        v[3] = __vor(v[57], v[57]); // 8283F2BC
        v[3] = __vnmsubfp(v[1], v[29], v[3]); // 8283F2C0
        v[0] = __vmaddfp(v[27], v[30], v[0]); // 8283F2C4
        v[13] = __vmaddfp(v[3], v[28], v[13]); // 8283F2C8
        v[44] = __vmulfp(v[12], v[0]); // 8283F2CC
        v[12] = __vor(v[38], v[38]); // 8283F2D0
        v[114] = __vmulfp(v[5], v[0]); // 8283F2D4
        v[43] = __vmulfp(v[11], v[0]); // 8283F2D8
        v[0] = __vmulfp(v[7], v[7]); // 8283F2DC
        v[12] = __vmaddfp(v[45], v[124], v[12]); // 8283F2E0
        v[42] = __vmulfp(v[10], v[13]); // 8283F2E4
        v[41] = __vmulfp(v[9], v[13]); // 8283F2E8
        v[113] = __vmulfp(v[4], v[13]); // 8283F2EC
        v[13] = __vor(v[37], v[37]); // 8283F2F0
        v[13] = __vmaddfp(v[45], v[123], v[13]); // 8283F2F4
        __vector4 stack256 = v[114]; // 8283F2FC
        v[11] = __vor(v[43], v[43]); // 8283F300
        v[10] = __vmaddfp(v[6], v[6], v[0]); // 8283F304
        v[0] = __vmulfp(v[12], v[52]); // 8283F308
        v[12] = __vor(v[36], v[36]); // 8283F30C
        __vector4 stack272 = v[113]; // 8283F314
        v[12] = __vmaddfp(v[45], v[122], v[12]); // 8283F318
        v[5] = v[10]; // 8283F31C
        v[0] = __vmaddfp(v[13], v[51], v[0]); // 8283F320
        v[13] = __vor(v[41], v[41]); // 8283F324
        v[0] = __vmaddfp(v[12], v[53], v[0]); // 8283F328
        v[12] = __vor(v[42], v[42]); // 8283F32C
        v[9] = __vsubfp(v[0], v[2]); // 8283F330
        v[0] = __vrsqrtefp(v[10]); // 8283F334
        v[4] = __vmulfp(v[0], v[0]); // 8283F338
        v[2] = __vor(v[57], v[57]); // 8283F33C
        v[3] = __vmulfp(v[0], v[56]); // 8283F340
        v[1] = __vor(v[45], v[45]); // 8283F344
        v[1] = __vmaddfp(v[55], v[1], v[117]); // 8283F348
        v[2] = __vnmsubfp(v[5], v[4], v[2]); // 8283F34C
        v[5] = __vor(v[58], v[58]); // 8283F350
        v[4] = __vor(v[58], v[58]); // 8283F354
        v[0] = __vmaddfp(v[2], v[3], v[0]); // 8283F358
        v[2] = __vor(v[45], v[45]); // 8283F35C
        v[2] = __vmaddfp(v[54], v[2], v[116]); // 8283F360
        v[40] = __vmulfp(v[7], v[0]); // 8283F364
        v[7] = __vor(v[33], v[33]); // 8283F368
        v[39] = __vmulfp(v[6], v[0]); // 8283F36C
        v[6] = __vor(v[34], v[34]); // 8283F370
        v[111] = __vmulfp(v[10], v[0]); // 8283F374
        v[10] = __vor(v[44], v[44]); // 8283F378
        v[0] = __vmulfp(v[9], v[9]); // 8283F37C
        v[9] = __vor(v[32], v[32]); // 8283F380
        __vector4 stack224 = v[40]; // 8283F388
        __vector4 stack240 = v[39]; // 8283F390
        __vector4 stack288 = v[111]; // 8283F398
        v[3] = __vsubfp(v[112], v[0]); // 8283F39C
        __vector4 stack320;
        __vector4 stack336;
        v[1] = Intersect2DCircleWithTriangleSOA(v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10], v[11], v[12], v[13], stack224, stack240, stack256, stack272, stack288, stack320, stack336);
        v[0] = __vspltisw(2); // 8283F3A4
        v[12] = __vsubfp(v[117], v[58]); // 8283F3A8
        v[11] = __vsubfp(v[116], v[58]); // 8283F3AC
        v[22] = __vor(v[57], v[57]); // 8283F3B0
        v[13] = __vmulfp(v[55], v[55]); // 8283F3B4
        v[5] = __vmulfp(v[121], v[121]); // 8283F3B8
        v[0] = __vcfsx(v[0], 0); // 8283F3BC
        v[10] = __vmulfp(v[35], v[35]); // 8283F3C0
        v[6] = __vmulfp(v[12], v[55]); // 8283F3C4
        v[13] = __vmaddfp(v[54], v[54], v[13]); // 8283F3C8
        v[7] = __vmulfp(v[0], v[35]); // 8283F3CC
        v[10] = __vsubfp(v[10], v[112]); // 8283F3D0
        v[9] = __vmulfp(v[0], v[0]); // 8283F3D4
        v[6] = __vmaddfp(v[11], v[54], v[6]); // 8283F3D8
        v[13] = __vaddfp(v[13], v[5]); // 8283F3DC
        v[7] = __vmulfp(v[7], v[121]); // 8283F3E0
        v[2] = __vmulfp(v[9], v[13]); // 8283F3E4
        v[4] = __vmulfp(v[0], v[13]); // 8283F3E8
        v[3] = __vmaddfp(v[6], v[0], v[7]); // 8283F3EC
        v[6] = __vmulfp(v[12], v[12]); // 8283F3F0
        v[31] = __vcmpeqfp(v[13], v[58]); // 8283F3F4
        v[28] = v[4]; // 8283F3F8
        v[30] = __vmulfp(v[3], v[3]); // 8283F3FC
        v[27] = v[4]; // 8283F400
        v[6] = __vmaddfp(v[11], v[11], v[6]); // 8283F404
        v[6] = __vaddfp(v[6], v[10]); // 8283F408
        v[6] = __vmulfp(v[2], v[6]); // 8283F40C
        v[2] = __vrefp(v[4]); // 8283F410
        v[6] = __vsubfp(v[30], v[6]); // 8283F414
        v[30] = __vor(v[58], v[58]); // 8283F418
        v[23] = v[2]; // 8283F41C
        v[22] = __vnmsubfp(v[23], v[28], v[22]); // 8283F420
        v[4] = __vrsqrtefp(v[6]); // 8283F424
        v[26] = v[6]; // 8283F428
        v[25] = v[6]; // 8283F42C
        v[24] = __vcmpeqfp(v[58], v[6]); // 8283F430
        v[29] = __vcmpgefp(v[6], v[58]); // 8283F434
        v[2] = __vmaddfp(v[2], v[22], v[2]); // 8283F438
        v[22] = __vor(v[57], v[57]); // 8283F43C
        v[28] = __vmulfp(v[4], v[4]); // 8283F440
        v[23] = __vmulfp(v[4], v[56]); // 8283F444
        v[22] = __vnmsubfp(v[26], v[28], v[22]); // 8283F448
        v[28] = v[2]; // 8283F44C
        v[26] = __vor(v[57], v[57]); // 8283F450
        v[26] = __vnmsubfp(v[28], v[27], v[26]); // 8283F454
        v[4] = __vmaddfp(v[23], v[22], v[4]); // 8283F458
        v[2] = __vmaddfp(v[2], v[26], v[2]); // 8283F45C
        v[26] = __vor(v[57], v[57]); // 8283F460
        v[28] = __vmulfp(v[4], v[4]); // 8283F464
        v[27] = __vmulfp(v[4], v[56]); // 8283F468
        v[26] = __vnmsubfp(v[25], v[28], v[26]); // 8283F46C
        v[28] = __vmulfp(v[55], v[44]); // 8283F470
        v[4] = __vmaddfp(v[27], v[26], v[4]); // 8283F474
        v[6] = __vmulfp(v[6], v[4]); // 8283F478
        v[4] = __vmulfp(v[46], v[3]); // 8283F47C
        v[6] = __vsel(v[6], v[30], v[24]); // 8283F480
        v[30] = __vmulfp(v[12], v[44]); // 8283F484
        v[31] = __vnor(v[31], v[31]); // 8283F488
        v[30] = __vmaddfp(v[11], v[43], v[30]); // 8283F48C
        v[28] = __vmaddfp(v[54], v[43], v[28]); // 8283F490
        v[26] = __vor(v[44], v[44]); // 8283F494
        v[25] = __vor(v[43], v[43]); // 8283F498
        v[27] = __vor(v[44], v[44]); // 8283F49C
        v[29] = __vand(v[31], v[29]); // 8283F4A0
        v[31] = __vsubfp(v[4], v[6]); // 8283F4A4
        v[6] = __vaddfp(v[4], v[6]); // 8283F4A8
        v[21] = __vor(v[57], v[57]); // 8283F4AC
        v[3] = v[30]; // 8283F4B0
        v[30] = v[28]; // 8283F4B4
        v[28] = __vor(v[43], v[43]); // 8283F4B8
        v[4] = __vnmsubfp(v[3], v[25], v[11]); // 8283F4BC
        v[24] = __vmulfp(v[6], v[2]); // 8283F4C0
        v[6] = __vnmsubfp(v[3], v[26], v[12]); // 8283F4C4
        v[3] = __vor(v[55], v[55]); // 8283F4C8
        v[31] = __vmulfp(v[31], v[2]); // 8283F4CC
        v[2] = v[30]; // 8283F4D0
        v[26] = __vor(v[58], v[58]); // 8283F4D4
        v[3] = __vnmsubfp(v[2], v[27], v[3]); // 8283F4D8
        v[2] = __vor(v[54], v[54]); // 8283F4DC
        v[2] = __vnmsubfp(v[30], v[28], v[2]); // 8283F4E0
        v[31] = __vminfp(v[24], v[31]); // 8283F4E4
        v[30] = __vmulfp(v[6], v[3]); // 8283F4E8
        v[28] = __vmulfp(v[3], v[3]); // 8283F4EC
        v[3] = __vcmpgefp(v[57], v[31]); // 8283F4F0
        v[27] = __vcmpgefp(v[31], v[58]); // 8283F4F4
        v[30] = __vmaddfp(v[4], v[2], v[30]); // 8283F4F8
        v[2] = __vmaddfp(v[2], v[2], v[28]); // 8283F4FC
        v[27] = __vand(v[27], v[3]); // 8283F500
        v[3] = __vmaddfp(v[30], v[0], v[7]); // 8283F504
        v[30] = __vand(v[29], v[27]); // 8283F508
        v[29] = __vmulfp(v[6], v[6]); // 8283F50C
        v[6] = __vaddfp(v[2], v[5]); // 8283F510
        v[28] = __vmulfp(v[3], v[3]); // 8283F514
        v[4] = __vmaddfp(v[4], v[4], v[29]); // 8283F518
        v[27] = __vmulfp(v[9], v[6]); // 8283F51C
        v[29] = __vcmpeqfp(v[6], v[58]); // 8283F520
        v[3] = __vmulfp(v[46], v[3]); // 8283F524
        v[2] = __vaddfp(v[4], v[10]); // 8283F528
        v[4] = __vmulfp(v[0], v[6]); // 8283F52C
        v[29] = __vnor(v[29], v[29]); // 8283F530
        v[6] = __vmulfp(v[27], v[2]); // 8283F534
        v[2] = __vrefp(v[4]); // 8283F538
        v[27] = v[4]; // 8283F53C
        v[25] = v[4]; // 8283F540
        v[6] = __vsubfp(v[28], v[6]); // 8283F544
        v[22] = v[2]; // 8283F548
        v[21] = __vnmsubfp(v[22], v[27], v[21]); // 8283F54C
        v[4] = __vrsqrtefp(v[6]); // 8283F550
        v[24] = v[6]; // 8283F554
        v[28] = __vcmpgefp(v[6], v[58]); // 8283F558
        v[23] = __vcmpeqfp(v[58], v[6]); // 8283F55C
        v[2] = __vmaddfp(v[2], v[21], v[2]); // 8283F560
        v[21] = __vor(v[57], v[57]); // 8283F564
        v[27] = __vmulfp(v[4], v[4]); // 8283F568
        v[22] = __vmulfp(v[4], v[56]); // 8283F56C
        v[29] = __vand(v[29], v[28]); // 8283F570
        v[21] = __vnmsubfp(v[24], v[27], v[21]); // 8283F574
        v[27] = v[2]; // 8283F578
        v[24] = __vor(v[57], v[57]); // 8283F57C
        v[24] = __vnmsubfp(v[27], v[25], v[24]); // 8283F580
        v[25] = v[6]; // 8283F584
        v[4] = __vmaddfp(v[22], v[21], v[4]); // 8283F588
        v[2] = __vmaddfp(v[2], v[24], v[2]); // 8283F58C
        v[24] = __vor(v[57], v[57]); // 8283F590
        v[27] = __vmulfp(v[4], v[4]); // 8283F594
        v[24] = __vnmsubfp(v[25], v[27], v[24]); // 8283F598
        v[27] = __vmulfp(v[4], v[56]); // 8283F59C
        v[4] = __vmaddfp(v[27], v[24], v[4]); // 8283F5A0
        v[27] = __vsel(v[0], v[31], v[30]); // 8283F5A4
        v[6] = __vmulfp(v[6], v[4]); // 8283F5A8
        v[6] = __vsel(v[6], v[26], v[23]); // 8283F5AC
        v[4] = __vsubfp(v[3], v[6]); // 8283F5B0
        v[6] = __vaddfp(v[3], v[6]); // 8283F5B4
        v[4] = __vmulfp(v[4], v[2]); // 8283F5B8
        v[6] = __vmulfp(v[6], v[2]); // 8283F5BC
        v[6] = __vminfp(v[6], v[4]); // 8283F5C0
        v[12] = __vmaddfp(v[55], v[6], v[12]); // 8283F5C4
        v[3] = v[6]; // 8283F5C8
        v[11] = __vmaddfp(v[54], v[6], v[11]); // 8283F5CC
        v[24] = __vor(v[57], v[57]); // 8283F5D0
        v[12] = __vmulfp(v[12], v[44]); // 8283F5D4
        v[21] = __vor(v[57], v[57]); // 8283F5D8
        v[4] = __vcmpgefp(v[57], v[6]); // 8283F5DC
        v[14] = __vor(v[57], v[57]); // 8283F5E0
        v[6] = __vcmpgefp(v[6], v[58]); // 8283F5E4
        v[20] = __vor(v[58], v[58]); // 8283F5E8
        v[18] = __vor(v[42], v[42]); // 8283F5EC
        v[12] = __vmaddfp(v[11], v[43], v[12]); // 8283F5F0
        v[6] = __vand(v[6], v[4]); // 8283F5F4
        v[6] = __vand(v[29], v[6]); // 8283F5F8
        v[29] = __vcmpeqfp(v[13], v[58]); // 8283F5FC
        v[4] = v[12]; // 8283F600
        v[11] = __vcmpgefp(v[4], v[58]); // 8283F604
        v[12] = __vcmpgefp(v[114], v[4]); // 8283F608
        v[29] = __vnor(v[29], v[29]); // 8283F60C
        v[12] = __vand(v[11], v[12]); // 8283F610
        v[11] = __vsubfp(v[116], v[33]); // 8283F614
        v[12] = __vand(v[6], v[12]); // 8283F618
        v[6] = __vmulfp(v[0], v[13]); // 8283F61C
        v[26] = __vsel(v[0], v[3], v[12]); // 8283F620
        v[3] = __vmulfp(v[9], v[13]); // 8283F624
        v[25] = __vor(v[30], v[12]); // 8283F628
        v[12] = __vsubfp(v[117], v[34]); // 8283F62C
        v[2] = __vcmpgefp(v[26], v[27]); // 8283F630
        v[28] = v[6]; // 8283F634
        v[23] = v[6]; // 8283F638
        v[30] = __vmulfp(v[12], v[12]); // 8283F63C
        v[31] = __vmulfp(v[12], v[55]); // 8283F640
        v[30] = __vmaddfp(v[11], v[11], v[30]); // 8283F644
        v[31] = __vmaddfp(v[11], v[54], v[31]); // 8283F648
        v[30] = __vaddfp(v[30], v[10]); // 8283F64C
        v[31] = __vmaddfp(v[31], v[0], v[7]); // 8283F650
        v[30] = __vmulfp(v[3], v[30]); // 8283F654
        v[3] = __vrefp(v[6]); // 8283F658
        v[6] = __vmulfp(v[31], v[31]); // 8283F65C
        v[6] = __vsubfp(v[6], v[30]); // 8283F660
        v[30] = v[3]; // 8283F664
        v[24] = __vnmsubfp(v[30], v[28], v[24]); // 8283F668
        v[28] = v[6]; // 8283F66C
        v[19] = __vcmpeqfp(v[58], v[6]); // 8283F670
        v[30] = __vmaddfp(v[3], v[24], v[3]); // 8283F674
        v[3] = __vrsqrtefp(v[6]); // 8283F678
        v[24] = __vcmpgefp(v[6], v[58]); // 8283F67C
        v[22] = v[30]; // 8283F680
        v[17] = __vand(v[29], v[24]); // 8283F684
        v[21] = __vnmsubfp(v[22], v[23], v[21]); // 8283F688
        v[22] = __vor(v[57], v[57]); // 8283F68C
        v[23] = __vmulfp(v[3], v[3]); // 8283F690
        v[30] = __vmaddfp(v[30], v[21], v[30]); // 8283F694
        v[22] = __vnmsubfp(v[28], v[23], v[22]); // 8283F698
        v[23] = v[6]; // 8283F69C
        v[21] = __vmulfp(v[3], v[56]); // 8283F6A0
        v[28] = __vmulfp(v[46], v[31]); // 8283F6A4
        v[31] = __vmulfp(v[55], v[42]); // 8283F6A8
        v[3] = __vmaddfp(v[21], v[22], v[3]); // 8283F6AC
        v[21] = __vor(v[41], v[41]); // 8283F6B0
        v[22] = __vmulfp(v[12], v[42]); // 8283F6B4
        v[31] = __vmaddfp(v[54], v[41], v[31]); // 8283F6B8
        v[29] = __vmulfp(v[3], v[3]); // 8283F6BC
        v[22] = __vmaddfp(v[11], v[41], v[22]); // 8283F6C0
        v[15] = v[31]; // 8283F6C4
        v[16] = v[31]; // 8283F6C8
        v[31] = __vor(v[54], v[54]); // 8283F6CC
        v[31] = __vnmsubfp(v[16], v[21], v[31]); // 8283F6D0
        v[14] = __vnmsubfp(v[23], v[29], v[14]); // 8283F6D4
        v[29] = __vor(v[55], v[55]); // 8283F6D8
        v[24] = v[22]; // 8283F6DC
        v[22] = __vmulfp(v[3], v[56]); // 8283F6E0
        v[29] = __vnmsubfp(v[15], v[18], v[29]); // 8283F6E4
        v[3] = __vmaddfp(v[22], v[14], v[3]); // 8283F6E8
        v[6] = __vmulfp(v[6], v[3]); // 8283F6EC
        v[6] = __vsel(v[6], v[20], v[19]); // 8283F6F0
        v[3] = __vsubfp(v[28], v[6]); // 8283F6F4
        v[6] = __vaddfp(v[28], v[6]); // 8283F6F8
        v[3] = __vmulfp(v[3], v[30]); // 8283F6FC
        v[6] = __vmulfp(v[6], v[30]); // 8283F700
        v[30] = __vminfp(v[6], v[3]); // 8283F704
        v[6] = __vcmpgefp(v[57], v[30]); // 8283F708
        v[3] = __vcmpgefp(v[30], v[58]); // 8283F70C
        v[6] = __vand(v[3], v[6]); // 8283F710
        v[23] = __vor(v[41], v[41]); // 8283F714
        v[20] = __vor(v[57], v[57]); // 8283F718
        v[16] = __vor(v[57], v[57]); // 8283F71C
        v[28] = __vand(v[17], v[6]); // 8283F720
        v[6] = __vor(v[42], v[42]); // 8283F724
        v[3] = __vnmsubfp(v[24], v[23], v[11]); // 8283F728
        v[6] = __vnmsubfp(v[24], v[6], v[12]); // 8283F72C
        v[24] = __vmulfp(v[29], v[29]); // 8283F730
        v[29] = __vmulfp(v[6], v[29]); // 8283F734
        v[24] = __vmaddfp(v[31], v[31], v[24]); // 8283F738
        v[23] = __vmulfp(v[6], v[6]); // 8283F73C
        v[31] = __vmaddfp(v[3], v[31], v[29]); // 8283F740
        v[6] = __vaddfp(v[24], v[5]); // 8283F744
        v[3] = __vmaddfp(v[3], v[3], v[23]); // 8283F748
        v[31] = __vmaddfp(v[31], v[0], v[7]); // 8283F74C
        v[23] = __vmulfp(v[9], v[6]); // 8283F750
        v[24] = __vaddfp(v[3], v[10]); // 8283F754
        v[3] = __vmulfp(v[0], v[6]); // 8283F758
        v[29] = __vcmpeqfp(v[6], v[58]); // 8283F75C
        v[6] = __vmulfp(v[23], v[24]); // 8283F760
        v[24] = __vmulfp(v[31], v[31]); // 8283F764
        v[22] = v[3]; // 8283F768
        v[31] = __vmulfp(v[46], v[31]); // 8283F76C
        v[29] = __vnor(v[29], v[29]); // 8283F770
        v[6] = __vsubfp(v[24], v[6]); // 8283F774
        v[24] = v[3]; // 8283F778
        v[3] = __vrefp(v[3]); // 8283F77C
        v[23] = __vcmpgefp(v[6], v[58]); // 8283F780
        v[21] = v[3]; // 8283F784
        v[19] = __vcmpeqfp(v[58], v[6]); // 8283F788
        v[20] = __vnmsubfp(v[21], v[24], v[20]); // 8283F78C
        v[21] = __vor(v[57], v[57]); // 8283F790
        v[29] = __vand(v[29], v[23]); // 8283F794
        v[23] = __vcmpeqfp(v[13], v[58]); // 8283F798
        v[3] = __vmaddfp(v[3], v[20], v[3]); // 8283F79C
        v[20] = __vor(v[58], v[58]); // 8283F7A0
        v[24] = v[3]; // 8283F7A4
        v[21] = __vnmsubfp(v[24], v[22], v[21]); // 8283F7A8
        v[22] = v[6]; // 8283F7AC
        v[24] = __vmaddfp(v[3], v[21], v[3]); // 8283F7B0
        v[21] = v[6]; // 8283F7B4
        v[3] = __vrsqrtefp(v[6]); // 8283F7B8
        v[18] = __vmulfp(v[3], v[3]); // 8283F7BC
        v[17] = __vmulfp(v[3], v[56]); // 8283F7C0
        v[16] = __vnmsubfp(v[22], v[18], v[16]); // 8283F7C4
        v[3] = __vmaddfp(v[17], v[16], v[3]); // 8283F7C8
        v[17] = __vor(v[57], v[57]); // 8283F7CC
        v[22] = __vmulfp(v[3], v[3]); // 8283F7D0
        v[18] = __vmulfp(v[3], v[56]); // 8283F7D4
        v[17] = __vnmsubfp(v[21], v[22], v[17]); // 8283F7D8
        v[3] = __vmaddfp(v[18], v[17], v[3]); // 8283F7DC
        v[6] = __vmulfp(v[6], v[3]); // 8283F7E0
        v[6] = __vsel(v[6], v[20], v[19]); // 8283F7E4
        v[20] = __vmulfp(v[9], v[13]); // 8283F7E8
        v[3] = __vsubfp(v[31], v[6]); // 8283F7EC
        v[6] = __vaddfp(v[31], v[6]); // 8283F7F0
        v[3] = __vmulfp(v[3], v[24]); // 8283F7F4
        v[6] = __vmulfp(v[6], v[24]); // 8283F7F8
        v[6] = __vminfp(v[6], v[3]); // 8283F7FC
        v[12] = __vmaddfp(v[55], v[6], v[12]); // 8283F800
        v[22] = v[6]; // 8283F804
        v[11] = __vmaddfp(v[54], v[6], v[11]); // 8283F808
        v[3] = __vcmpgefp(v[57], v[6]); // 8283F80C
        v[31] = __vcmpgefp(v[6], v[58]); // 8283F810
        v[12] = __vmulfp(v[12], v[42]); // 8283F814
        v[31] = __vand(v[31], v[3]); // 8283F818
        v[6] = __vand(v[29], v[31]); // 8283F81C
        v[31] = __vmulfp(v[0], v[13]); // 8283F820
        v[12] = __vmaddfp(v[11], v[41], v[12]); // 8283F824
        v[3] = v[12]; // 8283F828
        v[12] = __vcmpgefp(v[113], v[3]); // 8283F82C
        v[11] = __vcmpgefp(v[3], v[58]); // 8283F830
        v[24] = __vand(v[11], v[12]); // 8283F834
        v[12] = __vsubfp(v[117], v[8]); // 8283F838
        v[11] = __vsubfp(v[116], v[32]); // 8283F83C
        v[6] = __vand(v[6], v[24]); // 8283F840
        v[24] = __vsel(v[0], v[30], v[28]); // 8283F844
        v[13] = __vmulfp(v[12], v[55]); // 8283F848
        v[30] = __vmulfp(v[12], v[12]); // 8283F84C
        v[13] = __vmaddfp(v[11], v[54], v[13]); // 8283F850
        v[29] = __vmaddfp(v[11], v[11], v[30]); // 8283F854
        v[22] = __vsel(v[0], v[22], v[6]); // 8283F858
        v[30] = __vmaddfp(v[13], v[0], v[7]); // 8283F85C
        v[21] = __vor(v[28], v[6]); // 8283F860
        v[18] = v[31]; // 8283F864
        v[15] = __vor(v[57], v[57]); // 8283F868
        v[14] = __vor(v[57], v[57]); // 8283F86C
        v[28] = __vcmpgefp(v[22], v[24]); // 8283F870
        v[13] = __vaddfp(v[29], v[10]); // 8283F874
        v[6] = __vmulfp(v[30], v[30]); // 8283F878
        v[29] = __vrefp(v[31]); // 8283F87C
        v[30] = __vmulfp(v[46], v[30]); // 8283F880
        v[13] = __vmulfp(v[20], v[13]); // 8283F884
        v[17] = v[29]; // 8283F888
        v[15] = __vnmsubfp(v[17], v[18], v[15]); // 8283F88C
        v[13] = __vsubfp(v[6], v[13]); // 8283F890
        v[31] = __vmaddfp(v[29], v[15], v[29]); // 8283F894
        v[29] = __vnor(v[23], v[23]); // 8283F898
        v[23] = __vor(v[58], v[58]); // 8283F89C
        v[6] = __vrsqrtefp(v[13]); // 8283F8A0
        v[19] = v[13]; // 8283F8A4
        v[16] = v[13]; // 8283F8A8
        v[20] = __vcmpgefp(v[13], v[58]); // 8283F8AC
        v[17] = __vmulfp(v[6], v[6]); // 8283F8B0
        v[15] = __vmulfp(v[6], v[56]); // 8283F8B4
        v[29] = __vand(v[29], v[20]); // 8283F8B8
        v[20] = __vcmpeqfp(v[58], v[13]); // 8283F8BC
        v[14] = __vnmsubfp(v[19], v[17], v[14]); // 8283F8C0
        v[19] = v[31]; // 8283F8C4
        v[17] = __vor(v[57], v[57]); // 8283F8C8
        v[17] = __vnmsubfp(v[19], v[18], v[17]); // 8283F8CC
        v[6] = __vmaddfp(v[15], v[14], v[6]); // 8283F8D0
        v[31] = __vmaddfp(v[31], v[17], v[31]); // 8283F8D4
        v[17] = __vor(v[57], v[57]); // 8283F8D8
        v[19] = __vmulfp(v[6], v[6]); // 8283F8DC
        v[18] = __vmulfp(v[6], v[56]); // 8283F8E0
        v[17] = __vnmsubfp(v[16], v[19], v[17]); // 8283F8E4
        v[19] = __vor(v[39], v[39]); // 8283F8E8
        v[6] = __vmaddfp(v[18], v[17], v[6]); // 8283F8EC
        v[18] = __vor(v[40], v[40]); // 8283F8F0
        v[17] = __vor(v[39], v[39]); // 8283F8F4
        v[13] = __vmulfp(v[13], v[6]); // 8283F8F8
        v[13] = __vsel(v[13], v[23], v[20]); // 8283F8FC
        v[20] = __vor(v[40], v[40]); // 8283F900
        v[6] = __vsubfp(v[30], v[13]); // 8283F904
        v[13] = __vaddfp(v[30], v[13]); // 8283F908
        v[6] = __vmulfp(v[6], v[31]); // 8283F90C
        v[13] = __vmulfp(v[13], v[31]); // 8283F910
        v[6] = __vminfp(v[13], v[6]); // 8283F914
        v[13] = __vcmpgefp(v[57], v[6]); // 8283F918
        v[31] = __vcmpgefp(v[6], v[58]); // 8283F91C
        v[30] = __vand(v[31], v[13]); // 8283F920
        v[13] = __vmulfp(v[55], v[40]); // 8283F924
        v[31] = __vmulfp(v[12], v[40]); // 8283F928
        v[23] = __vand(v[29], v[30]); // 8283F92C
        v[13] = __vmaddfp(v[54], v[39], v[13]); // 8283F930
        v[31] = __vmaddfp(v[11], v[39], v[31]); // 8283F934
        v[29] = v[13]; // 8283F938
        v[30] = v[13]; // 8283F93C
        v[13] = __vor(v[55], v[55]); // 8283F940
        v[16] = v[31]; // 8283F944
        v[15] = v[31]; // 8283F948
        v[31] = __vor(v[54], v[54]); // 8283F94C
        v[13] = __vnmsubfp(v[30], v[20], v[13]); // 8283F950
        v[30] = __vnmsubfp(v[16], v[18], v[12]); // 8283F954
        v[31] = __vnmsubfp(v[29], v[19], v[31]); // 8283F958
        v[29] = __vnmsubfp(v[15], v[17], v[11]); // 8283F95C
        v[20] = __vmulfp(v[13], v[13]); // 8283F960
        v[13] = __vmulfp(v[30], v[13]); // 8283F964
        v[30] = __vmulfp(v[30], v[30]); // 8283F968
        v[20] = __vmaddfp(v[31], v[31], v[20]); // 8283F96C
        v[13] = __vmaddfp(v[29], v[31], v[13]); // 8283F970
        v[31] = __vmaddfp(v[29], v[29], v[30]); // 8283F974
        v[5] = __vaddfp(v[20], v[5]); // 8283F978
        v[7] = __vmaddfp(v[13], v[0], v[7]); // 8283F97C
        v[13] = __vaddfp(v[31], v[10]); // 8283F980
        v[10] = __vmulfp(v[9], v[5]); // 8283F984
        v[9] = __vmulfp(v[7], v[7]); // 8283F988
        v[31] = __vcmpeqfp(v[5], v[58]); // 8283F98C
        v[13] = __vmulfp(v[10], v[13]); // 8283F990
        v[13] = __vsubfp(v[9], v[13]); // 8283F994
        v[10] = __vrsqrtefp(v[13]); // 8283F998
        v[29] = v[13]; // 8283F99C
        v[9] = __vmulfp(v[0], v[5]); // 8283F9A0
        v[14] = __vor(v[57], v[57]); // 8283F9A4
        v[5] = __vmulfp(v[46], v[7]); // 8283F9A8
        v[20] = v[13]; // 8283F9AC
        v[30] = __vcmpgefp(v[13], v[58]); // 8283F9B0
        v[19] = __vor(v[58], v[58]); // 8283F9B4
        v[18] = __vcmpeqfp(v[58], v[13]); // 8283F9B8
        v[16] = __vmulfp(v[10], v[10]); // 8283F9C0
        v[17] = v[9]; // 8283F9C4
        v[7] = __vrefp(v[9]); // 8283F9C8
        v[15] = v[9]; // 8283F9CC
        v[9] = __vmulfp(v[10], v[56]); // 8283F9D0
        v[14] = __vnmsubfp(v[29], v[16], v[14]); // 8283F9D4
        v[29] = __vor(v[57], v[57]); // 8283F9D8
        v[10] = __vmaddfp(v[9], v[14], v[10]); // 8283F9DC
        v[9] = v[7]; // 8283F9E0
        v[29] = __vnmsubfp(v[9], v[17], v[29]); // 8283F9E4
        v[17] = __vor(v[57], v[57]); // 8283F9E8
        v[9] = __vmaddfp(v[7], v[29], v[7]); // 8283F9EC
        v[7] = __vmulfp(v[10], v[10]); // 8283F9F0
        v[29] = __vmulfp(v[10], v[56]); // 8283F9F4
        v[17] = __vnmsubfp(v[20], v[7], v[17]); // 8283F9F8
        v[7] = __vor(v[57], v[57]); // 8283F9FC
        v[10] = __vmaddfp(v[29], v[17], v[10]); // 8283FA00
        v[13] = __vmulfp(v[13], v[10]); // 8283FA04
        v[10] = v[9]; // 8283FA08
        v[7] = __vnmsubfp(v[10], v[15], v[7]); // 8283FA0C
        v[13] = __vsel(v[13], v[19], v[18]); // 8283FA10
        v[10] = __vmaddfp(v[9], v[7], v[9]); // 8283FA14
        v[9] = __vsubfp(v[5], v[13]); // 8283FA18
        v[13] = __vaddfp(v[5], v[13]); // 8283FA1C
        v[9] = __vmulfp(v[9], v[10]); // 8283FA20
        v[13] = __vmulfp(v[13], v[10]); // 8283FA24
        v[13] = __vminfp(v[13], v[9]); // 8283FA28
        v[12] = __vmaddfp(v[55], v[13], v[12]); // 8283FA2C
        v[5] = v[13]; // 8283FA30
        v[11] = __vmaddfp(v[54], v[13], v[11]); // 8283FA34
        v[9] = __vcmpgefp(v[13], v[58]); // 8283FA38
        v[10] = __vcmpgefp(v[57], v[13]); // 8283FA3C
        v[13] = __vnor(v[31], v[31]); // 8283FA40
        v[31] = __vor(v[34], v[34]); // 8283FA44
        v[30] = __vand(v[13], v[30]); // 8283FA48
        v[7] = __vmulfp(v[12], v[40]); // 8283FA4C
        v[12] = __vsel(v[0], v[6], v[23]); // 8283FA50
        v[10] = __vand(v[9], v[10]); // 8283FA54
        v[9] = __vsel(v[22], v[24], v[28]); // 8283FA58
        v[29] = v[12]; // 8283FA5C
        v[10] = __vand(v[30], v[10]); // 8283FA60
        v[30] = __vsel(v[26], v[27], v[2]); // 8283FA64
        v[27] = __vor(v[45], v[45]); // 8283FA68
        v[7] = __vmaddfp(v[11], v[39], v[7]); // 8283FA6C
        v[11] = __vsel(v[0], v[9], v[21]); // 8283FA70
        v[13] = v[7]; // 8283FA74
        v[7] = __vor(v[34], v[34]); // 8283FA78
        v[6] = __vcmpgefp(v[13], v[58]); // 8283FA7C
        v[9] = __vcmpgefp(v[111], v[13]); // 8283FA80
        v[7] = __vmaddfp(v[3], v[42], v[7]); // 8283FA84
        v[9] = __vand(v[6], v[9]); // 8283FA88
        v[7] = __vsel(v[7], v[31], v[28]); // 8283FA8C
        v[10] = __vand(v[10], v[9]); // 8283FA90
        v[9] = __vsel(v[0], v[5], v[10]); // 8283FA94
        v[6] = __vor(v[23], v[10]); // 8283FA98
        v[5] = __vor(v[58], v[58]); // 8283FA9C
        v[12] = __vcmpgefp(v[9], v[12]); // 8283FAA0
        v[5] = __vmaddfp(v[4], v[44], v[5]); // 8283FAA4
        v[10] = __vsel(v[9], v[29], v[12]); // 8283FAA8
        v[29] = __vor(v[58], v[58]); // 8283FAAC
        v[10] = __vsel(v[0], v[10], v[6]); // 8283FAB0
        v[0] = __vsel(v[0], v[30], v[25]); // 8283FAB4
        v[30] = v[8]; // 8283FAB8
        v[5] = __vsel(v[5], v[29], v[2]); // 8283FABC
        v[29] = __vor(v[32], v[32]); // 8283FAC0
        v[30] = __vmaddfp(v[13], v[40], v[30]); // 8283FAC4
        v[29] = __vmaddfp(v[13], v[39], v[29]); // 8283FAC8
        v[13] = __vor(v[32], v[32]); // 8283FACC
        v[8] = __vsel(v[30], v[8], v[12]); // 8283FAD0
        v[30] = stack320; // 8283FAD4
        v[29] = __vsel(v[29], v[13], v[12]); // 8283FADC
        v[26] = stack336; // 8283FAE0
        v[13] = __vor(v[33], v[33]); // 8283FAE4
        v[24] = __vor(v[33], v[33]); // 8283FAE8
        v[23] = __vor(v[57], v[57]); // 8283FAEC
        v[13] = __vmaddfp(v[3], v[41], v[13]); // 8283FAF0
        v[3] = v[13]; // 8283FAF4
        v[13] = __vcmpgefp(v[0], v[11]); // 8283FAF8
        v[9] = __vsel(v[3], v[24], v[28]); // 8283FAFC
        v[0] = __vsel(v[0], v[11], v[13]); // 8283FB00
        v[11] = __vsel(v[5], v[7], v[13]); // 8283FB04
        v[12] = __vcmpgefp(v[0], v[10]); // 8283FB08
        v[11] = __vsel(v[11], v[8], v[12]); // 8283FB0C
        v[8] = __vor(v[58], v[58]); // 8283FB10
        v[0] = __vsel(v[0], v[10], v[12]); // 8283FB14
        v[10] = __vor(v[58], v[58]); // 8283FB18
        v[11] = __vsel(v[11], v[30], v[1]); // 8283FB1C
        v[8] = __vmaddfp(v[4], v[43], v[8]); // 8283FB20
        v[30] = v[122]; // 8283FB24
        v[0] = __vsel(v[0], v[27], v[1]); // 8283FB28
        v[10] = __vsel(v[8], v[10], v[2]); // 8283FB2C
        v[2] = __vor(v[57], v[57]); // 8283FB30
        v[13] = __vsel(v[10], v[9], v[13]); // 8283FB34
        v[13] = __vsel(v[13], v[29], v[12]); // 8283FB38
        v[13] = __vsel(v[13], v[26], v[1]); // 8283FB3C
        v[12] = __vmulfp(v[13], v[120]); // 8283FB40
        v[10] = __vmulfp(v[13], v[119]); // 8283FB44
        v[13] = __vmulfp(v[13], v[118]); // 8283FB48
        v[12] = __vmaddfp(v[11], v[47], v[12]); // 8283FB4C
        v[10] = __vmaddfp(v[11], v[49], v[10]); // 8283FB50
        v[13] = __vmaddfp(v[11], v[48], v[13]); // 8283FB54
        v[12] = __vaddfp(v[12], v[127]); // 8283FB58
        v[11] = __vaddfp(v[10], v[126]); // 8283FB5C
        v[10] = __vaddfp(v[13], v[125]); // 8283FB60
        v[9] = __vsubfp(v[12], v[38]); // 8283FB64
        v[8] = __vsubfp(v[11], v[37]); // 8283FB68
        v[7] = __vsubfp(v[10], v[36]); // 8283FB6C
        v[13] = __vmulfp(v[9], v[9]); // 8283FB70
        v[13] = __vmaddfp(v[8], v[8], v[13]); // 8283FB74
        v[13] = __vmaddfp(v[7], v[7], v[13]); // 8283FB78
        v[5] = v[13]; // 8283FB7C
        v[13] = __vrsqrtefp(v[13]); // 8283FB80
        v[4] = __vmulfp(v[13], v[13]); // 8283FB84
        v[3] = __vmulfp(v[13], v[56]); // 8283FB88
        v[2] = __vnmsubfp(v[5], v[4], v[2]); // 8283FB8C
        v[4] = v[124]; // 8283FB90
        v[5] = __vnmsubfp(v[4], v[0], v[12]); // 8283FB94
        v[13] = __vmaddfp(v[2], v[3], v[13]); // 8283FB98
        v[2] = v[123]; // 8283FB9C
        v[3] = __vnmsubfp(v[30], v[0], v[10]); // 8283FBA0
        v[4] = __vnmsubfp(v[2], v[0], v[11]); // 8283FBA4
        v[2] = __vmulfp(v[9], v[13]); // 8283FBA8
        v[9] = __vsubfp(v[5], v[38]); // 8283FBAC
        v[31] = __vmulfp(v[8], v[13]); // 8283FBB0
        v[30] = __vmulfp(v[7], v[13]); // 8283FBB4
        v[8] = __vsubfp(v[4], v[37]); // 8283FBB8
        v[7] = __vsubfp(v[3], v[36]); // 8283FBBC
        v[38] = __vmaddfp(v[2], v[50], v[38]); // 8283FBC0
        v[13] = __vmulfp(v[9], v[9]); // 8283FBC4
        v[37] = __vmaddfp(v[31], v[50], v[37]); // 8283FBC8
        v[36] = __vmaddfp(v[30], v[50], v[36]); // 8283FBCC
        v[13] = __vmaddfp(v[8], v[8], v[13]); // 8283FBD0
        v[13] = __vmaddfp(v[7], v[7], v[13]); // 8283FBD4
        v[27] = v[13]; // 8283FBD8
        v[13] = __vrsqrtefp(v[13]); // 8283FBDC
        v[26] = __vmulfp(v[13], v[13]); // 8283FBE0
        v[24] = __vmulfp(v[13], v[56]); // 8283FBE4
        v[23] = __vnmsubfp(v[27], v[26], v[23]); // 8283FBE8
        v[13] = __vmaddfp(v[23], v[24], v[13]); // 8283FBEC
        v[9] = __vmulfp(v[9], v[13]); // 8283FBF0
        v[8] = __vmulfp(v[8], v[13]); // 8283FBF4
        v[13] = __vmulfp(v[7], v[13]); // 8283FBF8
        v[7] = __vor(v[38], v[38]); // 8283FBFC
        v[7] = __vsel(v[5], v[7], v[1]); // 8283FC00
        v[9] = __vsel(v[9], v[2], v[1]); // 8283FC04
        v[2] = __vor(v[37], v[37]); // 8283FC08
        v[8] = __vsel(v[8], v[31], v[1]); // 8283FC0C
        v[13] = __vsel(v[13], v[30], v[1]); // 8283FC10
        v[31] = __vor(v[36], v[36]); // 8283FC14
        v[30] = v[3]; // 8283FC18
        v[5] = __vsel(v[4], v[2], v[1]); // 8283FC1C
        v[4] = __vsel(v[30], v[31], v[1]); // 8283FC20
        v[31] = __vmrghw(v[9], v[13]); // 8283FC24
        v[30] = __vmrghw(v[8], v[13]); // 8283FC28
        v[29] = __vmrglw(v[9], v[13]); // 8283FC30
        v[28] = __vmrglw(v[8], v[13]); // 8283FC34
        v[3] = __vmrghw(v[52], v[53]); // 8283FC38
        v[27] = __vmrghw(v[31], v[30]); // 8283FC3C
        v[31] = __vmrglw(v[31], v[30]); // 8283FC40
        v[30] = __vmrghw(v[29], v[28]); // 8283FC44
        v[29] = __vmrglw(v[29], v[28]); // 8283FC48
        v[2] = __vmrghw(v[51], v[53]); // 8283FC4C
        v[28] = __vmrghw(v[5], v[0]); // 8283FC50
        v[5] = __vmrglw(v[5], v[0]); // 8283FC54
        lContactNormal0.mV = v[27]; // 8283FC58
        v[27] = __vmrghw(v[11], v[0]); // 8283FC5C
        lContactNormal1.mV = v[31]; // 8283FC60
        v[0] = __vmrglw(v[11], v[0]); // 8283FC64
        lContactNormal2.mV = v[30]; // 8283FC68
        v[11] = __vmrghw(v[3], v[2]); // 8283FC70
        v[31] = __vmrglw(v[52], v[53]); // 8283FC74
        v[30] = __vmrglw(v[51], v[53]); // 8283FC78
        lContactNormal3.mV = v[29]; // 8283FC7C
        v[29] = __vmrghw(v[7], v[4]); // 8283FC80
        v[7] = __vmrglw(v[7], v[4]); // 8283FC84
        v[4] = __vmrghw(v[12], v[10]); // 8283FC8C
        v[12] = __vmrglw(v[12], v[10]); // 8283FC90
        lTriangleNormal0.mV = v[11]; // 8283FC94
        v[10] = __vmrglw(v[3], v[2]); // 8283FC98
        v[11] = __vmrghw(v[31], v[30]); // 8283FC9C
        v[3] = __vmrghw(v[29], v[28]); // 8283FCA0
        v[2] = __vmrglw(v[29], v[28]); // 8283FCA4
        lTriangleNormal1.mV = v[10]; // 8283FCA8
        v[10] = __vmrglw(v[31], v[30]); // 8283FCAC
        lTriangleNormal2.mV = v[11]; // 8283FCB0
        v[11] = __vmrghw(v[7], v[5]); // 8283FCB8
        lTriangleNormal3.mV = v[10]; // 8283FCC0
        lSphereContactPoint0.mV = v[3]; // 8283FCC8
        v[10] = __vmrglw(v[7], v[5]); // 8283FCCC
        v[7] = __vmrghw(v[4], v[27]); // 8283FCD0
        v[5] = __vmrglw(v[4], v[27]); // 8283FCD4
        lSphereContactPoint1.mV = v[2]; // 8283FCD8
        lSphereContactPoint2.mV = v[11]; // 8283FCE0
        v[11] = __vmrghw(v[12], v[0]); // 8283FCE8
        v[0] = __vmrglw(v[12], v[0]); // 8283FCEC
        lSphereContactPoint3.mV = v[10]; // 8283FCF0
        lTriangleContactPoint0.mV = v[7]; // 8283FCF8
        v[10] = __vor(v[1], v[25]); // 8283FCFC
        lTriangleContactPoint1.mV = v[5]; // 8283FD00
        v[10] = __vor(v[10], v[21]); // 8283FD04
        lTriangleContactPoint2.mV = v[11]; // 8283FD08
        v[11] = __vmulfp(v[9], v[52]); // 8283FD0C
        v[9] = __vnor(v[115], v[115]); // 8283FD10
        v[10] = __vor(v[10], v[6]); // 8283FD1C
        lTriangleContactPoint3.mV = v[0]; // 8283FD20
        v[12] = __vand(v[9], v[10]); // 8283FD24
        v[0] = lTriangles.mValidMasks.mV; // 8283FD28
        v[11] = __vmaddfp(v[8], v[51], v[11]); // 8283FD2C
        v[11] = __vmaddfp(v[13], v[53], v[11]); // 8283FD30
        v[13] = __vcmpgtfp(v[58], v[11]); // 8283FD34
        v[13] = __vand(v[13], v[12]); // 8283FD38
        v[1] = __vand(v[13], v[0]); // 8283FD3C
        return Triangle4::Mask4(v[1]);
    }
}

// =============================================================================================
// The SINGLE-triangle swept kernel:
//   CgsGeometric::IntersectTriangleSweptSphere   (1171 instructions; the last 780 follow its twelve IsValid asserts)
//   CgsGeometric::Intersect2DCircleWithTriangle  (150, straight-line VMX)
// One caller: TriangleCollisionDebugComponent::TestPolySoupSweptSphereCollision.
//
// The algorithm is the four-wide kernel's (the banner above), written per triangle in AoS
// form, and the instruction stream decides every difference from that kernel:
//   * the 2D circle test DOES take vertex C as a candidate (the SOA function above does not),
//     and its compares are `!(d > best)`;
//   * no facing test on the contact normal: the result is the slab clip and the hit masks;
//   * the edge sweeps keep the vertex unless it is LATER than the edge (`!(v > e)`), and of
//     the three pairs a later one wins a tie;
//   * the triangle normal output is the unit normal on all four lanes; the contact normal is
//     computed from the contact point carrying the w lane the caller's TriangleContactPoint
//     held on entry (the console writes the point back with that w, then derives both
//     normals from the stored vector, and only then puts the contact time in the w lanes).
// Local names are the declaration's; the inlined helpers SolveSweptCircleEquation,
// IsBetween0And1, Intersect2DSweptCircleWithVertexAndEdge, Transform3DTo2D /
// Transform3DVectorTo2D and NormalizeReturnMagnitude are file-local functions below.
//
// HOW IT WAS CHECKED: every VMX128 operand was read from its raw instruction word (39 printed
// vA fields corrected, the fused forms' operand roles per their encodings), the corrected
// stream was executed on float32 lanes by a small interpreter, and this lowering was compared
// with it on 3,500 random sweeps (aimed, planar, parallel to the plane, exactly parallel,
// zero length, collinear triangles, non-zero w lanes): no hit-mask differences; the only value
// differences are rejected sweeps parallel to the plane to within a float ulp, where the
// slab times divide by a dot product of ~1e-8 whose sign the rounding decides.
// Native continuation: the single-triangle/circle family below now uses actual
// VMX estimates, refinements, fused and separate arithmetic, comparisons and
// word merges. Whole emitted functions and all output lanes are compared with
// ARTIST. The older four-wide sections above retain their separate legacy notes.
// =============================================================================================

namespace CgsGeometric
{
    namespace
    {
        // vminfp / vmaxfp: a NaN operand gives a NaN (the first operand's when both are), and
        // equal operands of opposite sign order -0 below +0.
        inline f32 SweptVmxMin(f32 lfA, f32 lfB)
        {
            return __vminfp(XMVectorReplicate(lfA), XMVectorReplicate(lfB)).x;
        }
        inline f32 SweptVmxMax(f32 lfA, f32 lfB)
        {
            return __vmaxfp(XMVectorReplicate(lfA), XMVectorReplicate(lfB)).x;
        }

        __forceinline f32 SweptMul(f32 a, f32 b) { return __vmulfp(XMVectorReplicate(a), XMVectorReplicate(b)).x; }
        __forceinline f32 SweptAdd(f32 a, f32 b) { return __vaddfp(XMVectorReplicate(a), XMVectorReplicate(b)).x; }
        __forceinline f32 SweptSub(f32 a, f32 b) { return __vsubfp(XMVectorReplicate(a), XMVectorReplicate(b)).x; }
        __forceinline f32 SweptMadd(f32 a, f32 b, f32 c) { return __vmaddfp(XMVectorReplicate(a), XMVectorReplicate(b), XMVectorReplicate(c)).x; }
        __forceinline bool SweptMaskBool(__vector4 mask)
        {
            u32 bits;
            std::memcpy(&bits, &mask, sizeof(bits));
            return bits != 0;
        }
        __forceinline bool SweptGE(f32 a, f32 b) { return SweptMaskBool(__vcmpgefp(XMVectorReplicate(a), XMVectorReplicate(b))); }
        __forceinline bool SweptGT(f32 a, f32 b) { return SweptMaskBool(__vcmpgtfp(XMVectorReplicate(a), XMVectorReplicate(b))); }
        __forceinline bool SweptEQ(f32 a, f32 b) { return SweptMaskBool(__vcmpeqfp(XMVectorReplicate(a), XMVectorReplicate(b))); }
        __forceinline __vector4 SweptReciprocal(__vector4 d)
        {
            const __vector4 one = XMVectorReplicate(1.0f);
            __vector4 x = __vrefp(d);
            for (int i = 0; i != 2; ++i) x = __vmaddfp(x, __vnmsubfp(x, d, one), x);
            return x;
        }
        __forceinline __vector4 SweptReciprocalSqrt(__vector4 d)
        {
            const __vector4 one = XMVectorReplicate(1.0f), half = XMVectorReplicate(0.5f);
            __vector4 x = __vrsqrtefp(d);
            for (int i = 0; i != 2; ++i)
            {
                const __vector4 square = __vmulfp(x, x), halfX = __vmulfp(x, half);
                x = __vmaddfp(halfX, __vnmsubfp(d, square, one), x);
            }
            return x;
        }

        inline rw::math::vpu::MaskScalar SweptMask(bool lbValue)
        {
            return rw::math::vpu::MaskScalar(lbValue ? __vspltisw(-1) : __vspltisw(0));
        }

        // The original two-dimensional lane packing is (x, y, x, x).
        inline Vector2 SweptPlanePoint(f32 lfX, f32 lfY)
        {
            Vector2 lPoint(lfX, lfY);
            lPoint.mV = __vrlimi(lPoint.mV, __vspltw(lPoint.mV, 0), 3, 0);
            return lPoint;
        }

        // The x+y dot of two 2D vectors (vmulfp128, then splat x + splat y).
        inline f32 SweptDot2(const Vector2& lrA, const Vector2& lrB)
        {
            const __vector4 product = __vmulfp(__lvx(&lrA, 0), __lvx(&lrB, 0));
            return __vaddfp(__vspltw(product, 0), __vspltw(product, 1)).x;
        }

        inline f32 SweptDot3(const Vector3& lrA, const Vector3& lrB)
        {
            return __vmsum3fp(__lvx(&lrA, 0), __lvx(&lrB, 0)).x;
        }

        // Normalize on all four lanes: vmsum3fp128 + vrsqrtefp + two Newton-Raphson steps.
        inline Vector3 SweptNormalize(const Vector3& lrV)
        {
            const __vector4 v = __lvx(&lrV, 0);
            const __vector4 inverse = SweptReciprocalSqrt(__vmsum3fp(v, v));
            Vector3 lResult;
            __stvx(__vmulfp(v, inverse), &lResult, 0);
            return lResult;
        }

        // a x b by the yzx-permute idiom, on all four lanes (the permute keeps w in w): one
        // rounded product (vmulfp128), then the other subtracted FUSED (vnmsubfp), which is
        // retained as the native instruction because it decides the normal of a nearly
        // degenerate triangle, where the two products cancel.
        inline Vector3 SweptCross(const Vector3& lrA, const Vector3& lrB)
        {
            Vector3 lResult;
            const __vector4 a = __lvx(&lrA, 0), b = __lvx(&lrB, 0);
            const __vector4 first = __vmulfp(a, __vpermwi(b, 0x63));
            __stvx(__vpermwi(__vnmsubfp(__vpermwi(a, 0x63), b, first), 0x63), &lResult, 0);
            return lResult;
        }

        // Transform3DTo2D / Transform3DVectorTo2D (the declaration's names; both inlined).
        inline Vector2 SweptTransform3DTo2D(const Vector3& lrBasisOrigin, const Vector3& lrBasisX,
                                            const Vector3& lrBasisY, const Vector3& lrPoint)
        {
            Vector3 lOffset;
            __stvx(__vsubfp(__lvx(&lrPoint, 0), __lvx(&lrBasisOrigin, 0)), &lOffset, 0);
            return SweptPlanePoint(SweptDot3(lOffset, lrBasisX), SweptDot3(lOffset, lrBasisY));
        }
        inline Vector2 SweptTransform3DVectorTo2D(const Vector3& lrBasisX, const Vector3& lrBasisY,
                                                  const Vector3& lrVector)
        {
            return SweptPlanePoint(SweptDot3(lrVector, lrBasisX), SweptDot3(lrVector, lrBasisY));
        }

        // NormalizeReturnMagnitude(Vector2): direction = v * rsqrt(|v|^2) on all lanes (no zero
        // guard), magnitude = |v|^2 * rsqrt(|v|^2), selected to 0 when |v|^2 == 0.
        inline f32 SweptNormalizeReturnMagnitude(const Vector2& lrV, Vector2& lrDirection)
        {
            const __vector4 lengthSq = XMVectorReplicate(SweptDot2(lrV, lrV));
            const __vector4 inverse = SweptReciprocalSqrt(lengthSq);
            __stvx(__vmulfp(__lvx(&lrV, 0), inverse), &lrDirection, 0);
            return __vsel(__vmulfp(lengthSq, inverse), __vzero(), __vcmpeqfp(lengthSq, __vzero())).x;
        }

        // SolveSweptCircleEquation: a t^2 + b t + c = 0 with
        // a = |V|^2 + R1, b = 2 (P.V) + R2, c = |P|^2 + R3. Returns the smaller root
        // (`vminfp` of the + root and the - root, in that order) and reports a and the
        // discriminant for the caller's validity mask. The square root is disc * rsqrt(disc),
        // selected to 0 when disc == 0.
        inline f32 SweptSolveCircleEquation(const Vector2& lrP, const Vector2& lrV,
                                            f32 lfR1, f32 lfR2, f32 lfR3,
                                            f32& lrfA, f32& lrfDiscriminant)
        {
            const f32 lfA = SweptAdd(SweptDot2(lrV, lrV), lfR1);
            const f32 lfB = SweptMadd(2.0f, SweptDot2(lrP, lrV), lfR2);
            const f32 lfC = SweptAdd(SweptDot2(lrP, lrP), lfR3);

            lrfA            = lfA;
            lrfDiscriminant = SweptSub(SweptMul(lfB, lfB), SweptMul(SweptMul(4.0f, lfA), lfC));

            const f32 lfInverseTwoA = SweptReciprocal(XMVectorReplicate(SweptMul(2.0f, lfA))).x;
            const f32 lfRoot = SweptEQ(lrfDiscriminant, 0.0f)
                             ? 0.0f
                             : SweptMul(lrfDiscriminant, SweptReciprocalSqrt(XMVectorReplicate(lrfDiscriminant)).x);
            const f32 lfMinusB = SweptMul(-1.0f, lfB);
            return SweptVmxMin(SweptMul(SweptAdd(lfMinusB, lfRoot), lfInverseTwoA), SweptMul(SweptSub(lfMinusB, lfRoot), lfInverseTwoA));
        }

        // IsBetween0And1: (t >= 0) & !(t > 1).
        inline bool SweptIsBetween0And1(f32 lfValue)
        {
            return SweptGE(lfValue, 0.0f) && !SweptGT(lfValue, 1.0f);
        }

        // Intersect2DSweptCircleWithVertexAndEdge, inlined three times. The vertex sweep solves for P = circle - vertex; the edge sweep for
        // the components of P and of the translation perpendicular to the edge, and is kept only
        // when the contact's projection on the edge lies in [0, |edge|]. Each solution is carried
        // as 2.0 when it is not valid; the vertex wins unless it is later than the edge.
        // Returns lGotVertexSolution | lGotEdgeSolution.
        inline bool SweptCircleWithVertexAndEdge(const Vector2& lrCirclePos, const Vector2& lrCircleTranslation,
                                                 f32 lfR1, f32 lfR2, f32 lfR3,
                                                 const Vector2& lrVertex, const Vector2& lrEdgeDir,
                                                 f32 lfEdgeMagnitude, f32& lrfOutT, Vector2& lrOutPosition)
        {
            const f32 KF_NO_SOLUTION = 2.0f;

            Vector2 lOffset;
            __stvx(__vsubfp(__lvx(&lrCirclePos, 0), __lvx(&lrVertex, 0)), &lOffset, 0);

            f32 lfVertexA = 0.0f, lfVertexDiscriminant = 0.0f;
            const f32 lfVertexT = SweptSolveCircleEquation(lOffset, lrCircleTranslation, lfR1, lfR2, lfR3,
                                                           lfVertexA, lfVertexDiscriminant);
            const bool lbGotVertexSolution = !SweptEQ(lfVertexA, 0.0f) && SweptGE(lfVertexDiscriminant, 0.0f)
                                          && SweptIsBetween0And1(lfVertexT);

            const f32 lfOffsetAlong      = SweptDot2(lOffset, lrEdgeDir);
            const f32 lfTranslationAlong = SweptDot2(lrCircleTranslation, lrEdgeDir);
            Vector2 lOffsetPerp, lTranslationPerp;
            __stvx(__vsubfp(__lvx(&lOffset, 0), __vmulfp(__lvx(&lrEdgeDir, 0), XMVectorReplicate(lfOffsetAlong))), &lOffsetPerp, 0);
            __stvx(__vsubfp(__lvx(&lrCircleTranslation, 0), __vmulfp(__lvx(&lrEdgeDir, 0), XMVectorReplicate(lfTranslationAlong))), &lTranslationPerp, 0);

            f32 lfEdgeA = 0.0f, lfEdgeDiscriminant = 0.0f;
            const f32 lfEdgeT = SweptSolveCircleEquation(lOffsetPerp, lTranslationPerp, lfR1, lfR2, lfR3,
                                                         lfEdgeA, lfEdgeDiscriminant);

            Vector2 lEdgeSolutionOffset;
            __stvx(__vmaddfp(__lvx(&lrCircleTranslation, 0), XMVectorReplicate(lfEdgeT), __lvx(&lOffset, 0)), &lEdgeSolutionOffset, 0);
            const f32 lfEdgeSolutionParam = SweptDot2(lEdgeSolutionOffset, lrEdgeDir);

            const bool lbEdgeSolutionIR = SweptGE(lfEdgeSolutionParam, 0.0f) && !SweptGT(lfEdgeSolutionParam, lfEdgeMagnitude);
            const bool lbGotEdgeSolution = !SweptEQ(lfEdgeA, 0.0f) && SweptGE(lfEdgeDiscriminant, 0.0f)
                                        && SweptIsBetween0And1(lfEdgeT) && lbEdgeSolutionIR;

            const f32 lfVertexSolution = lbGotVertexSolution ? lfVertexT : KF_NO_SOLUTION;
            const f32 lfEdgeSolution   = lbGotEdgeSolution   ? lfEdgeT   : KF_NO_SOLUTION;
            const bool lbPickVertex    = !SweptGT(lfVertexSolution, lfEdgeSolution);

            lrfOutT = lbPickVertex ? lfVertexSolution : lfEdgeSolution;
            if (lbPickVertex)
            {
                lrOutPosition = lrVertex;
            }
            else
            {
                __stvx(__vmaddfp(__lvx(&lrEdgeDir, 0), XMVectorReplicate(lfEdgeSolutionParam), __lvx(&lrVertex, 0)), &lrOutPosition, 0);
            }
            return lbGotVertexSolution || lbGotEdgeSolution;
        }
    }

    rw::math::vpu::MaskScalar Intersect2DCircleWithTriangle(Vector2 lCentre, VecFloat lRadiusSquared, Vector2 lVert0, Vector2 lVert1, Vector2 lVert2, Vector2 lEdge01Dir, Vector2 lEdge12Dir, Vector2 lEdge20Dir, VecFloat lfEdge01Magnitude, VecFloat lfEdge12Magnitude, VecFloat lfEdge20Magnitude, Vector2& lIntersectPoint)
    {
        __vector4 v[128];
        v[1] = lCentre.mV;
        v[2] = lRadiusSquared.mV;
        v[3] = lVert0.mV;
        v[4] = lVert1.mV;
        v[5] = lVert2.mV;
        v[6] = lEdge01Dir.mV;
        v[7] = lEdge12Dir.mV;
        v[8] = lEdge20Dir.mV;
        v[9] = lfEdge01Magnitude.mV;
        v[10] = lfEdge12Magnitude.mV;
        v[11] = lfEdge20Magnitude.mV;
        v[30] = __vsubfp(v[3], v[1]); // 8283DAA8
        v[25] = __vspltw(v[6], 1); // 8283DAAC
        v[29] = __vsubfp(v[4], v[1]); // 8283DAB0
        v[26] = __vspltw(v[7], 0); // 8283DAB4
        v[28] = __vsubfp(v[5], v[1]); // 8283DAB8
        v[24] = __vspltw(v[6], 0); // 8283DABC
        v[27] = __vspltw(v[7], 1); // 8283DAC0
        v[13] = __vsubfp(v[1], v[3]); // 8283DAC4
        v[23] = __vspltw(v[8], 1); // 8283DAC8
        v[12] = __vsubfp(v[1], v[4]); // 8283DACC
        v[22] = __vspltw(v[8], 0); // 8283DAD0
        v[31] = __vsubfp(v[1], v[5]); // 8283DAD4
        v[0] = __vspltisw(0); // 8283DAD8
        v[18] = v[2]; // 8283DADC
        v[21] = __vspltw(v[30], 0); // 8283DAE0
        v[20] = __vspltw(v[29], 0); // 8283DAE4
        v[29] = __vspltw(v[29], 1); // 8283DAE8
        v[30] = __vspltw(v[30], 1); // 8283DAEC
        v[25] = __vmulfp(v[25], v[21]); // 8283DAF0
        v[21] = __vspltw(v[28], 0); // 8283DAF4
        v[27] = __vmulfp(v[27], v[20]); // 8283DAF8
        v[28] = __vspltw(v[28], 1); // 8283DAFC
        v[29] = __vmulfp(v[26], v[29]); // 8283DB00
        v[30] = __vmulfp(v[24], v[30]); // 8283DB04
        v[26] = __vmulfp(v[23], v[21]); // 8283DB08
        v[28] = __vmulfp(v[22], v[28]); // 8283DB0C
        v[24] = __vmulfp(v[12], v[12]); // 8283DB10
        v[20] = __vmulfp(v[31], v[31]); // 8283DB14
        v[29] = __vsubfp(v[29], v[27]); // 8283DB18
        v[30] = __vsubfp(v[30], v[25]); // 8283DB1C
        v[28] = __vsubfp(v[28], v[26]); // 8283DB20
        v[21] = __vspltw(v[24], 1); // 8283DB24
        v[24] = __vspltw(v[24], 0); // 8283DB28
        v[29] = __vcmpgefp(v[29], v[0]); // 8283DB2C
        v[27] = __vcmpgefp(v[30], v[0]); // 8283DB30
        v[30] = __vmulfp(v[13], v[6]); // 8283DB34
        v[28] = __vcmpgefp(v[28], v[0]); // 8283DB38
        v[27] = __vand(v[27], v[29]); // 8283DB3C
        v[29] = __vmulfp(v[12], v[7]); // 8283DB40
        v[28] = __vand(v[27], v[28]); // 8283DB44
        v[27] = __vspltw(v[30], 1); // 8283DB48
        v[30] = __vspltw(v[30], 0); // 8283DB4C
        v[30] = __vaddfp(v[30], v[27]); // 8283DB50
        v[27] = __vmulfp(v[31], v[8]); // 8283DB54
        v[25] = __vspltw(v[29], 1); // 8283DB58
        v[29] = __vspltw(v[29], 0); // 8283DB5C
        v[29] = __vaddfp(v[29], v[25]); // 8283DB60
        v[25] = __vmulfp(v[13], v[13]); // 8283DB64
        v[23] = __vcmpgefp(v[30], v[9]); // 8283DB68
        v[26] = __vcmpgtfp(v[30], v[0]); // 8283DB6C
        v[9] = __vspltw(v[27], 1); // 8283DB70
        v[27] = __vspltw(v[27], 0); // 8283DB74
        v[9] = __vaddfp(v[27], v[9]); // 8283DB78
        v[22] = __vcmpgefp(v[29], v[10]); // 8283DB7C
        v[10] = __vspltw(v[25], 1); // 8283DB80
        v[7] = __vmulfp(v[7], v[29]); // 8283DB84
        v[25] = __vspltw(v[25], 0); // 8283DB88
        v[27] = __vnor(v[26], v[26]); // 8283DB8C
        v[26] = __vcmpgtfp(v[29], v[0]); // 8283DB90
        v[10] = __vaddfp(v[25], v[10]); // 8283DB94
        v[25] = __vaddfp(v[24], v[21]); // 8283DB98
        v[24] = __vor(v[27], v[23]); // 8283DB9C
        v[23] = __vcmpgtfp(v[9], v[0]); // 8283DBA0
        v[11] = __vcmpgefp(v[9], v[11]); // 8283DBA4
        v[9] = __vmulfp(v[8], v[9]); // 8283DBA8
        v[8] = __vspltw(v[20], 1); // 8283DBAC
        v[26] = __vnor(v[26], v[26]); // 8283DBB0
        v[19] = v[10]; // 8283DBB4
        v[22] = __vor(v[26], v[22]); // 8283DBB8
        v[23] = __vnor(v[23], v[23]); // 8283DBBC
        v[21] = __vor(v[23], v[11]); // 8283DBC0
        v[11] = __vcmpgtfp(v[10], v[2]); // 8283DBC4
        v[10] = __vmulfp(v[6], v[30]); // 8283DBC8
        v[6] = __vspltw(v[20], 0); // 8283DBCC
        v[8] = __vaddfp(v[6], v[8]); // 8283DBD0
        v[11] = __vnor(v[11], v[11]); // 8283DBD4
        v[13] = __vsubfp(v[13], v[10]); // 8283DBD8
        v[11] = __vand(v[11], v[27]); // 8283DBDC
        v[2] = v[11]; // 8283DBE0
        v[30] = v[11]; // 8283DBE4
        v[11] = __vsel(v[18], v[19], v[11]); // 8283DBE8
        v[6] = __vmulfp(v[13], v[13]); // 8283DBEC
        v[24] = __vnor(v[24], v[24]); // 8283DBF0
        v[13] = __vcmpgtfp(v[25], v[11]); // 8283DBF4
        v[0] = __vor(v[0], v[2]); // 8283DBF8
        v[12] = __vsubfp(v[12], v[7]); // 8283DBFC
        v[10] = __vaddfp(v[10], v[3]); // 8283DC00
        v[7] = __vaddfp(v[7], v[4]); // 8283DC04
        v[13] = __vnor(v[13], v[13]); // 8283DC08
        v[12] = __vmulfp(v[12], v[12]); // 8283DC0C
        v[13] = __vand(v[13], v[26]); // 8283DC10
        v[29] = v[13]; // 8283DC14
        v[27] = v[13]; // 8283DC18
        v[13] = __vsel(v[11], v[25], v[13]); // 8283DC1C
        v[0] = __vor(v[0], v[29]); // 8283DC20
        v[11] = v[13]; // 8283DC24
        v[13] = __vcmpgtfp(v[8], v[13]); // 8283DC28
        v[13] = __vnor(v[13], v[13]); // 8283DC2C
        v[13] = __vand(v[13], v[23]); // 8283DC30
        v[23] = __vnor(v[21], v[21]); // 8283DC34
        v[26] = v[13]; // 8283DC38
        v[25] = v[13]; // 8283DC3C
        v[13] = __vsel(v[11], v[8], v[13]); // 8283DC40
        v[8] = __vspltw(v[6], 0); // 8283DC44
        v[11] = __vspltw(v[6], 1); // 8283DC48
        v[0] = __vor(v[0], v[26]); // 8283DC4C
        v[6] = v[13]; // 8283DC50
        v[11] = __vaddfp(v[8], v[11]); // 8283DC54
        v[8] = __vsubfp(v[31], v[9]); // 8283DC58
        v[31] = __vnor(v[22], v[22]); // 8283DC5C
        v[9] = __vaddfp(v[9], v[5]); // 8283DC60
        v[13] = __vcmpgtfp(v[11], v[13]); // 8283DC64
        v[22] = v[11]; // 8283DC68
        v[11] = __vmulfp(v[8], v[8]); // 8283DC6C
        v[8] = __vspltw(v[12], 1); // 8283DC70
        v[12] = __vspltw(v[12], 0); // 8283DC74
        v[12] = __vaddfp(v[12], v[8]); // 8283DC78
        v[13] = __vnor(v[13], v[13]); // 8283DC7C
        v[8] = __vspltw(v[11], 1); // 8283DC80
        v[11] = __vspltw(v[11], 0); // 8283DC84
        v[13] = __vand(v[13], v[24]); // 8283DC88
        v[24] = __vaddfp(v[11], v[8]); // 8283DC8C
        v[11] = v[13]; // 8283DC90
        v[8] = v[13]; // 8283DC94
        v[13] = __vsel(v[6], v[22], v[13]); // 8283DC98
        v[22] = v[12]; // 8283DC9C
        v[0] = __vor(v[0], v[11]); // 8283DCA0
        v[21] = v[13]; // 8283DCA4
        v[13] = __vcmpgtfp(v[12], v[13]); // 8283DCA8
        v[13] = __vnor(v[13], v[13]); // 8283DCAC
        v[13] = __vand(v[13], v[31]); // 8283DCB0
        v[31] = __vsel(v[1], v[3], v[30]); // 8283DCB4
        v[12] = v[13]; // 8283DCB8
        v[6] = v[13]; // 8283DCBC
        v[13] = __vsel(v[21], v[22], v[13]); // 8283DCC0
        v[3] = __vsel(v[31], v[4], v[27]); // 8283DCC4
        v[0] = __vor(v[0], v[12]); // 8283DCC8
        v[13] = __vcmpgtfp(v[24], v[13]); // 8283DCCC
        v[4] = __vsel(v[3], v[5], v[25]); // 8283DCD0
        v[10] = __vsel(v[4], v[10], v[8]); // 8283DCD4
        v[10] = __vsel(v[10], v[7], v[6]); // 8283DCD8
        v[13] = __vnor(v[13], v[13]); // 8283DCDC
        v[13] = __vand(v[13], v[23]); // 8283DCE0
        v[12] = __vsel(v[10], v[9], v[13]); // 8283DCE4
        v[0] = __vor(v[0], v[13]); // 8283DCE8
        v[13] = __vsel(v[12], v[1], v[28]); // 8283DCEC
        v[1] = __vor(v[0], v[28]); // 8283DCF0
        lIntersectPoint.mV = v[13]; // 8283DCF4
        return rw::math::vpu::MaskScalar(v[1]);
    }

    const rw::math::vpu::MaskScalar IntersectTriangleSweptSphere(const SweptSphere& lSphere,
                                                                 const Triangle&    lTriangle,
                                                                 Vector3&           lOutContactNormal,
                                                                 Vector3&           lOutTriangleNormal,
                                                                 Vector3Plus&       lOutSphereContactPoint,
                                                                 Vector3Plus&       lOutTriangleContactPoint)
    {
        const f32 KF_NO_SOLUTION = 2.0f;

        // ---- unpack ----------------------------------------------------------------------------
        const Vector3 lTriVert0 = lTriangle.Get(0);
        const Vector3 lTriVert1 = lTriangle.Get(1);
        const Vector3 lTriVert2 = lTriangle.Get(2);

        const Vector3Plus lPositionAndRadius  = lSphere.GetPositionAndRadius();
        const Vector3Plus lDirectionAndLength = lSphere.GetDirectionAndLength();
        const Vector3 lSpherePos(lPositionAndRadius.mV);
        const Vector3 lSphereDir(lDirectionAndLength.mV);
        const f32 lfSphereRad = lPositionAndRadius.mV.w;
        const f32 lfSphereLength = lDirectionAndLength.mV.w;
        Vector3 lSphereTranslation;
        __stvx(__vmulfp(__lvx(&lDirectionAndLength, 0), XMVectorReplicate(lfSphereLength)), &lSphereTranslation, 0);

        CGS_ASSERT(rw::math::vpu::IsValid(lTriVert0), "IsValid(lTriVert0)");
        CGS_ASSERT(rw::math::vpu::IsValid(lTriVert1), "IsValid(lTriVert1)");
        CGS_ASSERT(rw::math::vpu::IsValid(lTriVert2), "IsValid(lTriVert2)");
        CGS_ASSERT(rw::math::vpu::IsValid(lSpherePos), "IsValid(lSpherePos)");
        CGS_ASSERT(lfSphereRad == lfSphereRad, "IsValid(lSphereRad)");
        CGS_ASSERT(rw::math::vpu::IsValid(lSphereDir), "IsValid(lSphereDir)");
        CGS_ASSERT(lfSphereLength == lfSphereLength, "IsValid(lSphereLength)");
        CGS_ASSERT(rw::math::vpu::IsValid(lSphereTranslation), "IsValid(lSphereTranslation)");

        // ---- the triangle's plane and the in-plane basis ----------------------------------------
        Vector3 lEdge01, lEdge12;
        __stvx(__vsubfp(__lvx(&lTriVert1, 0), __lvx(&lTriVert0, 0)), &lEdge01, 0);
        __stvx(__vsubfp(__lvx(&lTriVert2, 0), __lvx(&lTriVert1, 0)), &lEdge12, 0);
        CGS_ASSERT(rw::math::vpu::IsValid(lEdge01), "IsValid(lEdge01)");
        CGS_ASSERT(rw::math::vpu::IsValid(lEdge12), "IsValid(lEdge12)");

        const Vector3 lTriangleNormal = SweptNormalize(SweptCross(lEdge01, lEdge12));
        CGS_ASSERT(rw::math::vpu::IsValid(lTriangleNormal), "IsValid(lTriangleNormal)");

        const f32 lfPlaneOffset = SweptDot3(lTriangleNormal, lTriVert0);
        CGS_ASSERT(lfPlaneOffset == lfPlaneOffset, "IsValid(lfPlaneOffset)");

        const Vector3& lBasisOrigin = lTriVert0;
        const Vector3  lBasisX      = SweptNormalize(lEdge01);
        const Vector3  lBasisY      = SweptNormalize(SweptCross(lBasisX, lTriangleNormal));

        // ---- clip the sweep to the slab of half-thickness r about the plane ---------------------
        const f32 lfProjectTranslationOntoPlaneNormal = SweptDot3(lSphereTranslation, lTriangleNormal);
        const f32 lfPosPlaneOffset = SweptSub(SweptDot3(lSpherePos, lTriangleNormal), lfPlaneOffset);

        // Both times divide by MINUS the translation's normal component (the `vxor` with the sign
        // splat, then `vrefp` + two Newton-Raphson steps).
        const f32 lfInverseApproach = SweptReciprocal(__vxor(XMVectorReplicate(lfProjectTranslationOntoPlaneNormal), XMVectorReplicate(-0.0f))).x;
        const f32 lfFaceCollisionT0 = SweptMul(lfInverseApproach, SweptSub(lfPosPlaneOffset, lfSphereRad));
        const f32 lfFaceCollisionT1 = SweptMul(lfInverseApproach, SweptAdd(lfPosPlaneOffset, lfSphereRad));

        const bool lbMovingParallel = SweptEQ(lfProjectTranslationOntoPlaneNormal, 0.0f);
        const f32  lfT0 = lbMovingParallel ? 0.0f
                                           : SweptVmxMax(SweptVmxMin(lfFaceCollisionT0, lfFaceCollisionT1), 0.0f);
        const f32  lfT1 = lbMovingParallel ? 1.0f
                                           : SweptVmxMin(SweptVmxMax(lfFaceCollisionT0, lfFaceCollisionT1), 1.0f);

        const f32  lfAbsPosPlaneOffset = __vandc(XMVectorReplicate(lfPosPlaneOffset), XMVectorReplicate(-0.0f)).x;
        const bool lbFailedParallel    = SweptGE(lfAbsPosPlaneOffset, lfSphereRad) && lbMovingParallel;
        const bool lbFailedNoneParallel = (SweptGE(lfT0, 1.0f) || !SweptGT(lfT1, 0.0f)) && !lbMovingParallel;
        const bool lbFailed            = lbFailedParallel || lbFailedNoneParallel;

        // ---- everything in the plane's 2D frame, origin at vertex 0 -----------------------------
        const Vector2 l2DTriangleV0 = SweptPlanePoint(0.0f, 0.0f);
        const Vector2 l2DTriangleV1 = SweptTransform3DTo2D(lBasisOrigin, lBasisX, lBasisY, lTriVert1);
        const Vector2 l2DTriangleV2 = SweptTransform3DTo2D(lBasisOrigin, lBasisX, lBasisY, lTriVert2);
        const Vector2 l2DCirclePos  = SweptTransform3DTo2D(lBasisOrigin, lBasisX, lBasisY, lSpherePos);
        const Vector2 l2DCircleTranslation = SweptTransform3DVectorTo2D(lBasisX, lBasisY, lSphereTranslation);

        Vector2 l2DEdge01, l2DEdge12, l2DEdge20;
        __stvx(__vsubfp(__lvx(&l2DTriangleV1, 0), __lvx(&l2DTriangleV0, 0)), &l2DEdge01, 0);
        __stvx(__vsubfp(__lvx(&l2DTriangleV2, 0), __lvx(&l2DTriangleV1, 0)), &l2DEdge12, 0);
        __stvx(__vsubfp(__lvx(&l2DTriangleV0, 0), __lvx(&l2DTriangleV2, 0)), &l2DEdge20, 0);

        Vector2 l2DEdge01Dir, l2DEdge12Dir, l2DEdge20Dir;
        const f32 lfEdge01Magnitude = SweptNormalizeReturnMagnitude(l2DEdge01, l2DEdge01Dir);
        const f32 lfEdge12Magnitude = SweptNormalizeReturnMagnitude(l2DEdge12, l2DEdge12Dir);
        const f32 lfEdge20Magnitude = SweptNormalizeReturnMagnitude(l2DEdge20, l2DEdge20Dir);

        // ---- the FACE test at the slab entry time --------------------------------------------------
        Vector3 lSpherePosAtT0;
        __stvx(__vmaddfp(__lvx(&lSphereTranslation, 0), XMVectorReplicate(lfT0), __lvx(&lPositionAndRadius, 0)), &lSpherePosAtT0, 0);

        const f32 lfHeightAtT0 = SweptSub(SweptDot3(lSpherePosAtT0, lTriangleNormal), lfPlaneOffset);
        const f32 lfRadiusSquaredAtT0 = SweptSub(SweptMul(lfSphereRad, lfSphereRad), SweptMul(lfHeightAtT0, lfHeightAtT0));
        const VecFloat lRadiusSquaredAtT0(lfRadiusSquaredAtT0);
        const Vector2 l2DCirclePosAtT0 = SweptTransform3DTo2D(lBasisOrigin, lBasisX, lBasisY, lSpherePosAtT0);
        const VecFloat lEdge01Magnitude(lfEdge01Magnitude), lEdge12Magnitude(lfEdge12Magnitude), lEdge20Magnitude(lfEdge20Magnitude);

        Vector2 l2DResultAtT0;
        const bool lbResultValidAtT0 = Intersect2DCircleWithTriangle(
            l2DCirclePosAtT0, lRadiusSquaredAtT0,
            l2DTriangleV0, l2DTriangleV1, l2DTriangleV2,
            l2DEdge01Dir, l2DEdge12Dir, l2DEdge20Dir,
            lEdge01Magnitude, lEdge12Magnitude, lEdge20Magnitude,
            l2DResultAtT0).GetBool();

        // ---- the three (vertex, edge) sweeps ------------------------------------------------------
        const f32 lfVDotN       = lfProjectTranslationOntoPlaneNormal;
        const f32 lfPDotNMinusK = lfPosPlaneOffset;
        const f32 lfR1 = SweptMul(lfVDotN, lfVDotN);
        const f32 lfR2 = SweptMul(SweptMul(2.0f, lfPDotNMinusK), lfVDotN);
        const f32 lfR3 = SweptSub(SweptMul(lfPDotNMinusK, lfPDotNMinusK), SweptMul(lfSphereRad, lfSphereRad));

        f32 lfEdge01T, lfEdge12T, lfEdge20T;
        Vector2 lEdge01P, lEdge12P, lEdge20P;
        const bool lbEdge01Mask = SweptCircleWithVertexAndEdge(l2DCirclePos, l2DCircleTranslation, lfR1, lfR2, lfR3,
                                                               l2DTriangleV0, l2DEdge01Dir, lfEdge01Magnitude,
                                                               lfEdge01T, lEdge01P);
        const bool lbEdge12Mask = SweptCircleWithVertexAndEdge(l2DCirclePos, l2DCircleTranslation, lfR1, lfR2, lfR3,
                                                               l2DTriangleV1, l2DEdge12Dir, lfEdge12Magnitude,
                                                               lfEdge12T, lEdge12P);
        const bool lbEdge20Mask = SweptCircleWithVertexAndEdge(l2DCirclePos, l2DCircleTranslation, lfR1, lfR2, lfR3,
                                                               l2DTriangleV2, l2DEdge20Dir, lfEdge20Magnitude,
                                                               lfEdge20T, lEdge20P);

        // The earliest of the three; a later pair wins a tie (`!(later > earlier)`).
        const f32 lfMaskedEdge01T = lbEdge01Mask ? lfEdge01T : KF_NO_SOLUTION;
        const f32 lfMaskedEdge12T = lbEdge12Mask ? lfEdge12T : KF_NO_SOLUTION;
        const f32 lfMaskedEdge20T = lbEdge20Mask ? lfEdge20T : KF_NO_SOLUTION;

        const bool lPickEdge12 = !SweptGT(lfMaskedEdge12T, lfMaskedEdge01T);
        f32     lfEdgeResultT = lPickEdge12 ? lfMaskedEdge12T : lfMaskedEdge01T;
        Vector2 lEdgeResultP  = lPickEdge12 ? lEdge12P : lEdge01P;

        const bool lPickEdge20 = !SweptGT(lfMaskedEdge20T, lfEdgeResultT);
        lfEdgeResultT = lPickEdge20 ? lfMaskedEdge20T : lfEdgeResultT;
        lEdgeResultP  = lPickEdge20 ? lEdge20P : lEdgeResultP;

        // The face contact wins outright when it is valid.
        const f32     lfResultT = lbResultValidAtT0 ? lfT0 : lfEdgeResultT;
        const Vector2 lResultP  = lbResultValidAtT0 ? l2DResultAtT0 : lEdgeResultP;

        const bool lPassed = !lbFailed && (lbResultValidAtT0 || lbEdge01Mask || lbEdge12Mask || lbEdge20Mask);

        // ---- outputs -------------------------------------------------------------------------------
        lOutTriangleNormal = lTriangleNormal;

        // Transform2DTo3D: (BasisX * p.x + BasisY * p.y) + origin. The console stores it with the
        // w lane the caller's TriangleContactPoint already held, and computes both normals from
        // that vector before the w lane is replaced by the contact time.
        Vector3 lTriangleContactPoint;
        const __vector4 lAlongY = __vmulfp(__lvx(&lBasisY, 0), XMVectorReplicate(lResultP.mV.y));
        __vector4 lContact = __vaddfp(__vmaddfp(__lvx(&lBasisX, 0), XMVectorReplicate(lResultP.mV.x), lAlongY), __lvx(&lBasisOrigin, 0));
        lContact = __vrlimi(lContact, __lvx(&lOutTriangleContactPoint, 0), 1, 0);
        __stvx(lContact, &lTriangleContactPoint, 0);

        Vector3 lToContact;
        __stvx(__vsubfp(__lvx(&lTriangleContactPoint, 0), __lvx(&lPositionAndRadius, 0)), &lToContact, 0);
        const Vector3 lSphereNormalIfPenetration = SweptNormalize(lToContact);

        Vector3 lSphereContactPointIfPenetration;
        __stvx(__vmaddfp(__lvx(&lSphereNormalIfPenetration, 0), XMVectorReplicate(lfSphereRad), __lvx(&lPositionAndRadius, 0)), &lSphereContactPointIfPenetration, 0);

        Vector3 lSphereContactPointIfFuture;
        __stvx(__vsubfp(__lvx(&lTriangleContactPoint, 0), __vmulfp(__lvx(&lSphereTranslation, 0), XMVectorReplicate(lfResultT))), &lSphereContactPointIfFuture, 0);

        Vector3 lToFuture;
        __stvx(__vsubfp(__lvx(&lSphereContactPointIfFuture, 0), __lvx(&lPositionAndRadius, 0)), &lToFuture, 0);
        const Vector3 lSphereNormalIfFuture = SweptNormalize(lToFuture);

        lOutContactNormal = lbResultValidAtT0 ? lSphereNormalIfPenetration : lSphereNormalIfFuture;

        const Vector3& lrSphereContactPoint = lbResultValidAtT0 ? lSphereContactPointIfPenetration
                                                                : lSphereContactPointIfFuture;
        __stvx(__vrlimi(__lvx(&lrSphereContactPoint, 0), XMVectorReplicate(lfResultT), 1, 0), &lOutSphereContactPoint, 0);

        __stvx(__vrlimi(__lvx(&lTriangleContactPoint, 0), XMVectorReplicate(lfResultT), 1, 0), &lOutTriangleContactPoint, 0);

        return SweptMask(lPassed);
    }
}
