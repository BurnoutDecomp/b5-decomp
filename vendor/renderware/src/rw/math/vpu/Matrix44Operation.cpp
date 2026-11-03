// ARTIST 0x825B2628: native VMX register sequence. The permutation constant
// 82CDA440 is 04..0F,00..03: rotate the register left by one word.
#include "rw/math/vpu/matrix44_operation.h"

namespace rw { namespace math { namespace vpu {

Matrix44 Inverse(const Matrix44& lrMatrix, Vector4& lrDeterminant)
{
    __vector4 v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13;
    __vector4 v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31;
    Matrix44 result;
    v31 = lrMatrix.xAxis.mV; // 825B262C
    v0 = __vspltisw(0); // 825B2634
    v13 = __vspltisw(-1); // 825B263C
    v10 = lrMatrix.zAxis.mV; // 825B2648
    v30 = __vslw(v13, v13); // 825B264C
    v11 = lrMatrix.yAxis.mV; // 825B2650
    v9 = lrMatrix.wAxis.mV; // 825B2658
    v11 = __vsldoi(v11, v11, 4); // 825B265C
    v10 = __vsldoi(v10, v10, 4); // 825B2660
    v30 = __vrlimi(v30, v0, 8, 0); // 825B2664
    v12 = __vsldoi(v31, v31, 4); // 825B2668
    v9 = __vsldoi(v9, v9, 4); // 825B266C
    v6 = __vsldoi(v11, v11, 4); // 825B2670
    v5 = __vsldoi(v10, v10, 4); // 825B2674
    v30 = __vrlimi(v30, v0, 2, 0); // 825B2678
    v8 = __vsldoi(v12, v12, 4); // 825B267C
    v4 = __vsldoi(v9, v9, 4); // 825B2680
    v2 = __vsldoi(v6, v6, 4); // 825B2684
    v21 = __vmaddfp(v12, v6, v0); // 825B2688
    v1 = __vsldoi(v5, v5, 4); // 825B268C
    v3 = __vsldoi(v8, v8, 4); // 825B2690
    v24 = __vmaddfp(v9, v8, v0); // 825B2694
    v7 = __vsldoi(v4, v4, 4); // 825B2698
    v23 = __vmaddfp(v1, v9, v0); // 825B269C
    v26 = __vmaddfp(v4, v3, v0); // 825B26A0
    v29 = __vmaddfp(v5, v7, v0); // 825B26A4
    v27 = __vmaddfp(v7, v12, v0); // 825B26A8
    v22 = __vmaddfp(v6, v1, v0); // 825B26AC
    v28 = __vnmsubfp(v10, v7, v23); // 825B26B0
    v26 = __vnmsubfp(v7, v8, v26); // 825B26B4
    v29 = __vnmsubfp(v1, v4, v29); // 825B26B8
    v23 = __vmaddfp(v10, v4, v0); // 825B26BC
    v27 = __vnmsubfp(v9, v3, v27); // 825B26C0
    v26 = __vmulfp(v10, v26); // 825B26C4
    v29 = __vmulfp(v11, v29); // 825B26C8
    v25 = __vnmsubfp(v5, v9, v23); // 825B26CC
    v27 = __vmaddfp(v5, v27, v26); // 825B26D0
    v26 = __vslw(v13, v13); // 825B26D4
    v29 = __vmaddfp(v6, v28, v29); // 825B26D8
    v28 = __vnmsubfp(v4, v12, v24); // 825B26DC
    v24 = __vslw(v13, v13); // 825B26E0
    v13 = __vslw(v13, v13); // 825B26E4
    v26 = __vrlimi(v26, v0, 4, 0); // 825B26E8
    v24 = __vrlimi(v24, v0, 8, 0); // 825B26EC
    v13 = __vrlimi(v13, v0, 4, 0); // 825B26F0
    v26 = __vrlimi(v26, v0, 1, 0); // 825B26F4
    v24 = __vrlimi(v24, v0, 2, 0); // 825B26F8
    v13 = __vrlimi(v13, v0, 1, 0); // 825B26FC
    v29 = __vmaddfp(v2, v25, v29); // 825B2700
    v28 = __vmaddfp(v1, v28, v27); // 825B2704
    v25 = __vmaddfp(v3, v11, v0); // 825B2708
    v30 = __vxor(v29, v30); // 825B270C
    v29 = __vxor(v28, v26); // 825B2710
    v26 = __vmaddfp(v8, v2, v0); // 825B2714
    v25 = __vnmsubfp(v12, v2, v25); // 825B2718
    v28 = __vnmsubfp(v3, v6, v26); // 825B271C
    v26 = v2; // 825B2720
    v2 = __vmaddfp(v2, v10, v0); // 825B2724
    v27 = __vnmsubfp(v26, v5, v22); // 825B2728
    v26 = __vnmsubfp(v8, v11, v21); // 825B272C
    v9 = __vmulfp(v9, v28); // 825B2730
    v28 = v11; // 825B2734
    v11 = __vmaddfp(v11, v5, v0); // 825B2738
    v0 = __vspltisw(1); // 825B273C
    v12 = __vmulfp(v12, v27); // 825B2740
    v5 = __vnmsubfp(v28, v1, v2); // 825B2744
    v9 = __vmaddfp(v4, v25, v9); // 825B2748
    v11 = __vnmsubfp(v6, v10, v11); // 825B274C
    v12 = __vmaddfp(v8, v5, v12); // 825B2750
    v10 = __vmaddfp(v7, v26, v9); // 825B2754
    v12 = __vmaddfp(v3, v11, v12); // 825B2758
    v11 = __vxor(v10, v24); // 825B275C
    v10 = __vxor(v12, v13); // 825B2760
    v12 = __vcfsx(v0, 0); // 825B2764
    v13 = __vmsum4fp(v31, v30); // 825B2768
    v0 = __vrefp(v13); // 825B276C
    v9 = __vmrghw(v30, v11); // 825B2770
    v8 = __vmrglw(v30, v11); // 825B2774
    lrDeterminant.mV = v13; // 825B2778
    v11 = __vmrghw(v29, v10); // 825B277C
    v7 = __vmrglw(v29, v10); // 825B2780
    v1 = __vmrghw(v9, v11); // 825B2784
    v9 = __vmrglw(v9, v11); // 825B2788
    v10 = __vnmsubfp(v0, v13, v12); // 825B278C
    v11 = __vnmsubfp(v0, v13, v12); // 825B2790
    v6 = __vnmsubfp(v0, v13, v12); // 825B2794
    v10 = __vmaddfp(v0, v10, v0); // 825B2798
    v11 = __vmaddfp(v0, v11, v0); // 825B279C
    v4 = __vnmsubfp(v10, v13, v12); // 825B27A0
    v5 = __vnmsubfp(v11, v13, v12); // 825B27A4
    v10 = __vmaddfp(v10, v4, v10); // 825B27A8
    v11 = __vmaddfp(v11, v5, v11); // 825B27AC
    v10 = __vmulfp(v10, v9); // 825B27B0
    v11 = __vmulfp(v11, v1); // 825B27B4
    result.yAxis.mV = v10; // 825B27B8
    v10 = __vnmsubfp(v0, v13, v12); // 825B27BC
    result.xAxis.mV = v11; // 825B27C0
    v11 = __vmaddfp(v0, v6, v0); // 825B27C4
    v6 = __vmrghw(v8, v7); // 825B27C8
    v8 = __vmrglw(v8, v7); // 825B27CC
    v0 = __vmaddfp(v0, v10, v0); // 825B27D0
    v10 = __vnmsubfp(v11, v13, v12); // 825B27D4
    v13 = __vnmsubfp(v0, v13, v12); // 825B27D8
    v11 = __vmaddfp(v11, v10, v11); // 825B27DC
    v0 = __vmaddfp(v0, v13, v0); // 825B27E0
    v11 = __vmulfp(v11, v6); // 825B27E4
    v0 = __vmulfp(v0, v8); // 825B27E8
    result.zAxis.mV = v11; // 825B27EC
    result.wAxis.mV = v0; // 825B27F0
    return result;
}

} } } // namespace rw::math::vpu
