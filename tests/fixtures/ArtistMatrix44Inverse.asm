0x825B2628  lis       r11, unk_82CDA440@ha
0x825B262C  lvx128    v31, r0, r4
0x825B2630  li        r9, 0x20 # ' '
0x825B2634  vspltisw  v0, 0
0x825B2638  li        r10, 0x10
0x825B263C  vspltisw  v13, -1
0x825B2640  addi      r11, r11, unk_82CDA440@l
0x825B2644  li        r8, 0x30 # '0'
0x825B2648  lvx128    v10, r4, r9
0x825B264C  vslw      v30, v13, v13
0x825B2650  lvx128    v11, r4, r10
0x825B2654  lvx128    v7, r0, r11
0x825B2658  lvx128    v9, r4, r8
0x825B265C  vperm     v11, v11, v11, v7
0x825B2660  vperm     v10, v10, v10, v7
0x825B2664  vrlimi128 v30, v0, 8, 0
0x825B2668  vperm     v12, v31, v31, v7
0x825B266C  vperm     v9, v9, v9, v7
0x825B2670  vperm     v6, v11, v11, v7
0x825B2674  vperm     v5, v10, v10, v7
0x825B2678  vrlimi128 v30, v0, 2, 0
0x825B267C  vperm     v8, v12, v12, v7
0x825B2680  vperm     v4, v9, v9, v7
0x825B2684  vperm     v2, v6, v6, v7
0x825B2688  vmaddfp   v21, v12, v0, v6
0x825B268C  vperm     v1, v5, v5, v7
0x825B2690  vperm     v3, v8, v8, v7
0x825B2694  vmaddfp   v24, v9, v0, v8
0x825B2698  vperm     v7, v4, v4, v7
0x825B269C  vmaddfp   v23, v1, v0, v9
0x825B26A0  vmaddfp   v26, v4, v0, v3
0x825B26A4  vmaddfp   v29, v5, v0, v7
0x825B26A8  vmaddfp   v27, v7, v0, v12
0x825B26AC  vmaddfp   v22, v6, v0, v1
0x825B26B0  vnmsubfp  v28, v10, v23, v7
0x825B26B4  vnmsubfp  v26, v7, v26, v8
0x825B26B8  vnmsubfp  v29, v1, v29, v4
0x825B26BC  vmaddfp   v23, v10, v0, v4
0x825B26C0  vnmsubfp  v27, v9, v27, v3
0x825B26C4  vmulfp128 v26, v10, v26
0x825B26C8  vmulfp128 v29, v11, v29
0x825B26CC  vnmsubfp  v25, v5, v23, v9
0x825B26D0  vmaddfp   v27, v5, v26, v27
0x825B26D4  vslw      v26, v13, v13
0x825B26D8  vmaddfp   v29, v6, v29, v28
0x825B26DC  vnmsubfp  v28, v4, v24, v12
0x825B26E0  vslw      v24, v13, v13
0x825B26E4  vslw      v13, v13, v13
0x825B26E8  vrlimi128 v26, v0, 4, 0
0x825B26EC  vrlimi128 v24, v0, 8, 0
0x825B26F0  vrlimi128 v13, v0, 4, 0
0x825B26F4  vrlimi128 v26, v0, 1, 0
0x825B26F8  vrlimi128 v24, v0, 2, 0
0x825B26FC  vrlimi128 v13, v0, 1, 0
0x825B2700  vmaddfp   v29, v2, v29, v25
0x825B2704  vmaddfp   v28, v1, v27, v28
0x825B2708  vmaddfp   v25, v3, v0, v11
0x825B270C  vxor      v30, v29, v30
0x825B2710  vxor      v29, v28, v26
0x825B2714  vmaddfp   v26, v8, v0, v2
0x825B2718  vnmsubfp  v25, v12, v25, v2
0x825B271C  vnmsubfp  v28, v3, v26, v6
0x825B2720  vmr       v26, v2
0x825B2724  vmaddfp   v2, v2, v0, v10
0x825B2728  vnmsubfp  v27, v26, v22, v5
0x825B272C  vnmsubfp  v26, v8, v21, v11
0x825B2730  vmulfp128 v9, v9, v28
0x825B2734  vmr       v28, v11
0x825B2738  vmaddfp   v11, v11, v0, v5
0x825B273C  vspltisw  v0, 1
0x825B2740  vmulfp128 v12, v12, v27
0x825B2744  vnmsubfp  v5, v28, v2, v1
0x825B2748  vmaddfp   v9, v4, v9, v25
0x825B274C  vnmsubfp  v11, v6, v11, v10
0x825B2750  vmaddfp   v12, v8, v12, v5
0x825B2754  vmaddfp   v10, v7, v9, v26
0x825B2758  vmaddfp   v12, v3, v12, v11
0x825B275C  vxor      v11, v10, v24
0x825B2760  vxor      v10, v12, v13
0x825B2764  vcfsx     v12, v0, 0
0x825B2768  vmsum4fp128 v13, v31, v30
0x825B276C  vrefp     v0, v13
0x825B2770  vmrghw    v9, v30, v11
0x825B2774  vmrglw    v8, v30, v11
0x825B2778  stvx128   v13, r0, r5
0x825B277C  vmrghw    v11, v29, v10
0x825B2780  vmrglw    v7, v29, v10
0x825B2784  vmrghw    v1, v9, v11
0x825B2788  vmrglw    v9, v9, v11
0x825B278C  vnmsubfp  v10, v0, v12, v13
0x825B2790  vnmsubfp  v11, v0, v12, v13
0x825B2794  vnmsubfp  v6, v0, v12, v13
0x825B2798  vmaddfp   v10, v0, v0, v10
0x825B279C  vmaddfp   v11, v0, v0, v11
0x825B27A0  vnmsubfp  v4, v10, v12, v13
0x825B27A4  vnmsubfp  v5, v11, v12, v13
0x825B27A8  vmaddfp   v10, v10, v10, v4
0x825B27AC  vmaddfp   v11, v11, v11, v5
0x825B27B0  vmulfp128 v10, v10, v9
0x825B27B4  vmulfp128 v11, v11, v1
0x825B27B8  stvx128   v10, r3, r10
0x825B27BC  vnmsubfp  v10, v0, v12, v13
0x825B27C0  stvx128   v11, r0, r3
0x825B27C4  vmaddfp   v11, v0, v0, v6
0x825B27C8  vmrghw    v6, v8, v7
0x825B27CC  vmrglw    v8, v8, v7
0x825B27D0  vmaddfp   v0, v0, v0, v10
0x825B27D4  vnmsubfp  v10, v11, v12, v13
0x825B27D8  vnmsubfp  v13, v0, v12, v13
0x825B27DC  vmaddfp   v11, v11, v11, v10
0x825B27E0  vmaddfp   v0, v0, v0, v13
0x825B27E4  vmulfp128 v11, v11, v6
0x825B27E8  vmulfp128 v0, v0, v8
0x825B27EC  stvx128   v11, r3, r9
0x825B27F0  stvx128   v0, r3, r8
0x825B27F4  blr