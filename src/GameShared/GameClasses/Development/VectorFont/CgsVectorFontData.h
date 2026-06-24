#pragma once

#include "types.hpp"

// CgsDev VectorFont glyph data - CARVED from BURNOUT_X360_ARTIST (decrypted XEX) by
// tools/assets/fonts/carve_vectorfont.py. KA_CHARSET 0x82F31FF0 / KAN_LINECOUNT 0x82F32190 /
// KAN_CHARWIDTH 0x82F321F8. Each glyph is a list of CharLine strokes (start/end in a
// 0..KF_CHARWIDTH x 0..KF_CHARHEIGHT cell). Chars KI_FIRST_CHAR(32)..KI_LAST_CHAR(135).
namespace CompressedFontData
{
    struct CharLine { u8 miStartX; u8 miStartY; u8 miEndX; u8 miEndY; };

    static const CharLine KA_CHARDATA_0[1] = { {0,0,0,0} };
    static const CharLine KA_CHARDATA_1[2] = { {0,0,0,3}, {0,4,0,5} };
    static const CharLine KA_CHARDATA_2[2] = { {0,1,0,2}, {1,1,1,2} };
    static const CharLine KA_CHARDATA_3[4] = { {1,2,1,5}, {2,2,2,5}, {0,3,3,3}, {0,4,3,4} };
    static const CharLine KA_CHARDATA_4[4] = { {2,2,0,3}, {0,3,2,4}, {2,4,0,5}, {1,1,1,6} };
    static const CharLine KA_CHARDATA_5[5] = { {3,1,0,5}, {0,1,1,2}, {1,1,0,2}, {2,4,3,5}, {3,4,2,5} };
    static const CharLine KA_CHARDATA_6[8] = { {2,3,1,5}, {1,5,0,5}, {0,5,0,4}, {0,4,1,2}, {1,2,1,1}, {1,1,0,1}, {0,1,0,2}, {0,2,2,5} };
    static const CharLine KA_CHARDATA_7[1] = { {1,1,1,2} };
    static const CharLine KA_CHARDATA_8[3] = { {1,0,0,1}, {0,1,0,5}, {0,5,1,6} };
    static const CharLine KA_CHARDATA_9[3] = { {0,0,1,1}, {1,1,1,5}, {1,5,0,6} };
    static const CharLine KA_CHARDATA_10[3] = { {1,2,1,4}, {2,2,0,4}, {0,2,2,4} };
    static const CharLine KA_CHARDATA_11[4] = { {0,4,2,4}, {1,3,1,5}, {1,5,1,3}, {2,4,0,4} };
    static const CharLine KA_CHARDATA_12[1] = { {1,5,0,6} };
    static const CharLine KA_CHARDATA_13[1] = { {0,4,2,4} };
    static const CharLine KA_CHARDATA_14[1] = { {0,5,1,5} };
    static const CharLine KA_CHARDATA_15[1] = { {2,0,0,6} };
    static const CharLine KA_CHARDATA_16[5] = { {0,1,0,5}, {0,5,2,5}, {2,5,2,1}, {2,1,0,1}, {2,1,0,5} };
    static const CharLine KA_CHARDATA_17[3] = { {1,1,1,5}, {1,1,0,2}, {0,5,2,5} };
    static const CharLine KA_CHARDATA_18[6] = { {2,5,0,5}, {0,5,0,4}, {0,4,2,2}, {2,2,2,1}, {2,1,0,1}, {0,1,0,2} };
    static const CharLine KA_CHARDATA_19[5] = { {0,1,2,1}, {2,1,2,3}, {0,5,2,5}, {2,5,2,3}, {1,3,2,3} };
    static const CharLine KA_CHARDATA_20[3] = { {0,1,0,4}, {0,4,2,4}, {1,5,1,2} };
    static const CharLine KA_CHARDATA_21[7] = { {2,1,0,1}, {0,1,0,2}, {0,2,2,3}, {2,3,2,4}, {2,4,1,5}, {1,5,0,5}, {0,5,0,4} };
    static const CharLine KA_CHARDATA_22[8] = { {2,1,1,1}, {1,1,0,2}, {0,2,0,5}, {0,5,1,5}, {1,5,2,4}, {2,4,2,3}, {2,3,0,3}, {2,1,2,2} };
    static const CharLine KA_CHARDATA_23[2] = { {0,5,2,1}, {0,1,2,1} };
    static const CharLine KA_CHARDATA_24[5] = { {0,1,0,5}, {0,5,2,5}, {2,5,2,1}, {2,1,0,1}, {0,3,2,3} };
    static const CharLine KA_CHARDATA_25[5] = { {0,5,2,4}, {2,4,2,1}, {2,1,0,1}, {0,1,0,3}, {0,3,2,3} };
    static const CharLine KA_CHARDATA_26[2] = { {0,3,1,3}, {0,5,1,5} };
    static const CharLine KA_CHARDATA_27[3] = { {0,3,1,3}, {0,5,1,5}, {1,5,0,7} };
    static const CharLine KA_CHARDATA_28[2] = { {0,3,2,1}, {0,3,2,5} };
    static const CharLine KA_CHARDATA_29[2] = { {0,3,2,3}, {0,5,2,5} };
    static const CharLine KA_CHARDATA_30[2] = { {2,3,0,1}, {2,3,0,5} };
    static const CharLine KA_CHARDATA_31[5] = { {1,5,1,4}, {0,0,2,0}, {2,0,2,2}, {2,2,1,2}, {1,2,1,3} };
    static const CharLine KA_CHARDATA_32[8] = { {2,2,1,3}, {1,3,2,4}, {2,2,2,4}, {2,4,3,3}, {3,3,2,1}, {2,1,0,3}, {0,3,2,5}, {2,5,3,5} };
    static const CharLine KA_CHARDATA_33[5] = { {0,5,1,1}, {1,1,2,5}, {2,5,1,1}, {1,3,0,3}, {1,3,2,3} };
    static const CharLine KA_CHARDATA_34[8] = { {0,5,0,1}, {0,1,1,1}, {1,1,2,2}, {2,2,1,3}, {1,3,2,4}, {2,4,1,5}, {1,5,0,5}, {1,3,0,3} };
    static const CharLine KA_CHARDATA_35[3] = { {2,1,0,1}, {0,1,0,5}, {2,5,0,5} };
    static const CharLine KA_CHARDATA_36[6] = { {0,1,0,5}, {0,1,1,1}, {1,1,2,2}, {2,2,2,4}, {2,4,1,5}, {0,5,1,5} };
    static const CharLine KA_CHARDATA_37[4] = { {2,1,0,1}, {2,5,0,5}, {0,1,0,5}, {1,3,0,3} };
    static const CharLine KA_CHARDATA_38[3] = { {2,1,0,1}, {0,5,0,1}, {1,3,0,3} };
    static const CharLine KA_CHARDATA_39[5] = { {2,1,0,1}, {0,1,0,5}, {0,5,2,5}, {2,3,1,3}, {2,5,2,3} };
    static const CharLine KA_CHARDATA_40[6] = { {0,5,0,3}, {0,1,0,3}, {2,1,2,3}, {2,5,2,3}, {2,3,0,3}, {0,3,2,3} };
    static const CharLine KA_CHARDATA_41[3] = { {0,1,2,1}, {0,5,2,5}, {1,1,1,5} };
    static const CharLine KA_CHARDATA_42[3] = { {2,1,2,5}, {2,5,0,5}, {0,5,0,4} };
    static const CharLine KA_CHARDATA_43[4] = { {0,1,0,3}, {0,5,0,3}, {0,3,2,1}, {2,5,0,3} };
    static const CharLine KA_CHARDATA_44[2] = { {0,1,0,5}, {0,5,2,5} };
    static const CharLine KA_CHARDATA_45[5] = { {2,5,2,1}, {0,5,0,1}, {2,1,1,3}, {0,1,1,3}, {1,3,2,1} };
    static const CharLine KA_CHARDATA_46[4] = { {0,5,0,1}, {2,5,0,1}, {2,1,2,5}, {0,1,2,5} };
    static const CharLine KA_CHARDATA_47[4] = { {0,1,2,1}, {2,1,2,5}, {2,5,0,5}, {0,5,0,1} };
    static const CharLine KA_CHARDATA_48[4] = { {0,5,0,1}, {0,1,2,1}, {2,1,2,3}, {2,3,0,3} };
    static const CharLine KA_CHARDATA_49[5] = { {0,1,0,5}, {0,5,2,5}, {2,5,2,1}, {2,1,0,1}, {1,4,2,6} };
    static const CharLine KA_CHARDATA_50[6] = { {0,5,0,1}, {1,3,0,3}, {0,1,1,1}, {1,1,2,2}, {2,2,1,3}, {2,5,1,3} };
    static const CharLine KA_CHARDATA_51[7] = { {2,1,1,1}, {1,1,0,2}, {0,2,0,3}, {0,3,2,3}, {2,3,2,4}, {2,4,1,5}, {1,5,0,5} };
    static const CharLine KA_CHARDATA_52[3] = { {0,1,2,1}, {1,5,1,1}, {2,1,0,1} };
    static const CharLine KA_CHARDATA_53[3] = { {0,1,0,5}, {0,5,2,5}, {2,1,2,5} };
    static const CharLine KA_CHARDATA_54[3] = { {0,1,1,5}, {2,1,1,5}, {1,5,2,1} };
    static const CharLine KA_CHARDATA_55[5] = { {0,1,1,5}, {1,5,2,3}, {3,5,2,3}, {4,1,3,5}, {2,3,3,5} };
    static const CharLine KA_CHARDATA_56[4] = { {0,1,2,5}, {2,5,1,3}, {2,1,1,3}, {0,5,1,3} };
    static const CharLine KA_CHARDATA_57[4] = { {0,1,1,3}, {2,1,1,3}, {1,5,1,3}, {1,3,1,5} };
    static const CharLine KA_CHARDATA_58[3] = { {0,1,2,1}, {2,1,0,5}, {0,5,2,5} };
    static const CharLine KA_CHARDATA_59[3] = { {1,0,0,0}, {0,0,0,6}, {0,6,1,6} };
    static const CharLine KA_CHARDATA_60[1] = { {0,0,2,6} };
    static const CharLine KA_CHARDATA_61[3] = { {0,0,1,0}, {1,0,1,6}, {1,6,0,6} };
    static const CharLine KA_CHARDATA_62[3] = { {0,2,1,1}, {2,2,1,1}, {1,1,2,2} };
    static const CharLine KA_CHARDATA_63[1] = { {0,6,3,6} };
    static const CharLine KA_CHARDATA_64[1] = { {0,0,1,1} };
    static const CharLine KA_CHARDATA_65[5] = { {0,3,2,3}, {2,3,2,5}, {2,5,0,5}, {0,5,0,4}, {0,4,2,4} };
    static const CharLine KA_CHARDATA_66[4] = { {0,1,0,5}, {0,5,2,5}, {2,5,2,3}, {2,3,0,3} };
    static const CharLine KA_CHARDATA_67[3] = { {2,3,0,3}, {0,3,0,5}, {0,5,2,5} };
    static const CharLine KA_CHARDATA_68[4] = { {2,1,2,5}, {2,5,0,5}, {0,5,0,3}, {0,3,2,3} };
    static const CharLine KA_CHARDATA_69[5] = { {2,5,0,5}, {0,5,0,3}, {0,3,2,3}, {2,3,2,4}, {2,4,0,4} };
    static const CharLine KA_CHARDATA_70[3] = { {0,5,0,2}, {0,2,1,1}, {0,3,1,3} };
    static const CharLine KA_CHARDATA_71[5] = { {2,5,0,5}, {0,5,0,3}, {0,3,2,3}, {2,3,2,7}, {2,7,0,7} };
    static const CharLine KA_CHARDATA_72[4] = { {0,1,0,3}, {0,5,0,3}, {0,3,2,3}, {2,5,2,3} };
    static const CharLine KA_CHARDATA_73[2] = { {0,5,0,3}, {0,1,0,2} };
    static const CharLine KA_CHARDATA_74[3] = { {1,3,1,6}, {1,6,0,6}, {1,1,1,2} };
    static const CharLine KA_CHARDATA_75[4] = { {0,1,0,4}, {0,5,0,4}, {0,4,2,3}, {2,5,1,4} };
    static const CharLine KA_CHARDATA_76[2] = { {0,1,0,5}, {0,5,1,5} };
    static const CharLine KA_CHARDATA_77[5] = { {0,5,0,3}, {3,3,0,3}, {4,4,3,3}, {4,5,4,4}, {2,5,2,3} };
    static const CharLine KA_CHARDATA_78[4] = { {0,5,0,3}, {2,5,2,4}, {2,4,1,3}, {1,3,0,3} };
    static const CharLine KA_CHARDATA_79[4] = { {0,5,0,3}, {0,3,2,3}, {2,3,2,5}, {2,5,0,5} };
    static const CharLine KA_CHARDATA_80[4] = { {0,7,0,3}, {0,3,2,3}, {2,3,2,5}, {2,5,0,5} };
    static const CharLine KA_CHARDATA_81[4] = { {2,7,2,3}, {2,3,0,3}, {0,3,0,5}, {0,5,2,5} };
    static const CharLine KA_CHARDATA_82[2] = { {0,5,0,3}, {0,3,2,3} };
    static const CharLine KA_CHARDATA_83[6] = { {2,3,0,3}, {0,3,0,4}, {0,4,2,4}, {2,4,2,5}, {2,5,0,5}, {0,5,2,5} };
    static const CharLine KA_CHARDATA_84[3] = { {0,1,0,5}, {0,5,1,5}, {0,2,1,2} };
    static const CharLine KA_CHARDATA_85[3] = { {0,3,0,5}, {0,5,2,5}, {2,3,2,5} };
    static const CharLine KA_CHARDATA_86[3] = { {0,3,1,5}, {2,3,1,5}, {1,5,2,3} };
    static const CharLine KA_CHARDATA_87[4] = { {0,3,0,5}, {0,5,1,4}, {2,3,2,5}, {2,5,1,4} };
    static const CharLine KA_CHARDATA_88[4] = { {0,3,2,5}, {2,5,1,4}, {2,3,1,4}, {0,5,1,4} };
    static const CharLine KA_CHARDATA_89[2] = { {0,3,1,5}, {2,3,0,7} };
    static const CharLine KA_CHARDATA_90[3] = { {0,3,2,3}, {2,3,0,5}, {0,5,2,5} };
    static const CharLine KA_CHARDATA_91[4] = { {2,0,1,1}, {2,6,1,5}, {1,5,1,1}, {0,3,1,3} };
    static const CharLine KA_CHARDATA_92[1] = { {1,0,1,6} };
    static const CharLine KA_CHARDATA_93[4] = { {0,0,1,1}, {1,1,1,5}, {1,5,0,6}, {2,3,1,3} };
    static const CharLine KA_CHARDATA_94[3] = { {0,1,1,0}, {1,0,1,1}, {1,1,2,0} };
    static const CharLine KA_CHARDATA_95[3] = { {0,5,4,5}, {4,5,2,1}, {2,1,0,5} };
    static const CharLine KA_CHARDATA_96[4] = { {0,5,4,5}, {4,5,4,1}, {4,1,0,1}, {0,1,0,5} };
    static const CharLine KA_CHARDATA_97[2] = { {0,5,4,1}, {0,1,4,5} };
    static const CharLine KA_CHARDATA_98[8] = { {1,1,3,1}, {3,1,4,2}, {4,2,4,4}, {4,4,3,5}, {3,5,1,5}, {1,5,0,4}, {0,4,0,2}, {0,2,1,1} };
    static const CharLine KA_CHARDATA_99[3] = { {2,5,2,1}, {2,1,0,3}, {2,1,4,3} };
    static const CharLine KA_CHARDATA_100[3] = { {2,1,2,5}, {2,5,4,3}, {2,5,0,3} };
    static const CharLine KA_CHARDATA_101[3] = { {4,3,0,3}, {0,3,2,1}, {0,3,2,5} };
    static const CharLine KA_CHARDATA_102[3] = { {0,3,4,3}, {4,3,2,1}, {4,3,2,5} };
    static const CharLine KA_CHARDATA_103[6] = { {3,3,4,1}, {4,1,2,4}, {3,3,4,3}, {2,4,3,4}, {3,4,2,6}, {4,3,2,6} };

    static const CharLine* const KA_CHARSET[104] =
    {
        KA_CHARDATA_0, KA_CHARDATA_1, KA_CHARDATA_2, KA_CHARDATA_3, KA_CHARDATA_4, KA_CHARDATA_5, KA_CHARDATA_6, KA_CHARDATA_7,
        KA_CHARDATA_8, KA_CHARDATA_9, KA_CHARDATA_10, KA_CHARDATA_11, KA_CHARDATA_12, KA_CHARDATA_13, KA_CHARDATA_14, KA_CHARDATA_15,
        KA_CHARDATA_16, KA_CHARDATA_17, KA_CHARDATA_18, KA_CHARDATA_19, KA_CHARDATA_20, KA_CHARDATA_21, KA_CHARDATA_22, KA_CHARDATA_23,
        KA_CHARDATA_24, KA_CHARDATA_25, KA_CHARDATA_26, KA_CHARDATA_27, KA_CHARDATA_28, KA_CHARDATA_29, KA_CHARDATA_30, KA_CHARDATA_31,
        KA_CHARDATA_32, KA_CHARDATA_33, KA_CHARDATA_34, KA_CHARDATA_35, KA_CHARDATA_36, KA_CHARDATA_37, KA_CHARDATA_38, KA_CHARDATA_39,
        KA_CHARDATA_40, KA_CHARDATA_41, KA_CHARDATA_42, KA_CHARDATA_43, KA_CHARDATA_44, KA_CHARDATA_45, KA_CHARDATA_46, KA_CHARDATA_47,
        KA_CHARDATA_48, KA_CHARDATA_49, KA_CHARDATA_50, KA_CHARDATA_51, KA_CHARDATA_52, KA_CHARDATA_53, KA_CHARDATA_54, KA_CHARDATA_55,
        KA_CHARDATA_56, KA_CHARDATA_57, KA_CHARDATA_58, KA_CHARDATA_59, KA_CHARDATA_60, KA_CHARDATA_61, KA_CHARDATA_62, KA_CHARDATA_63,
        KA_CHARDATA_64, KA_CHARDATA_65, KA_CHARDATA_66, KA_CHARDATA_67, KA_CHARDATA_68, KA_CHARDATA_69, KA_CHARDATA_70, KA_CHARDATA_71,
        KA_CHARDATA_72, KA_CHARDATA_73, KA_CHARDATA_74, KA_CHARDATA_75, KA_CHARDATA_76, KA_CHARDATA_77, KA_CHARDATA_78, KA_CHARDATA_79,
        KA_CHARDATA_80, KA_CHARDATA_81, KA_CHARDATA_82, KA_CHARDATA_83, KA_CHARDATA_84, KA_CHARDATA_85, KA_CHARDATA_86, KA_CHARDATA_87,
        KA_CHARDATA_88, KA_CHARDATA_89, KA_CHARDATA_90, KA_CHARDATA_91, KA_CHARDATA_92, KA_CHARDATA_93, KA_CHARDATA_94, KA_CHARDATA_95,
        KA_CHARDATA_96, KA_CHARDATA_97, KA_CHARDATA_98, KA_CHARDATA_99, KA_CHARDATA_100, KA_CHARDATA_101, KA_CHARDATA_102, KA_CHARDATA_103,
    };

    static const u8 KAN_LINECOUNT[104] =
    {
        0, 2, 2, 4, 4, 5, 8, 1, 3, 3, 3, 4, 1, 1, 1, 1,
        5, 3, 6, 5, 3, 7, 8, 2, 5, 5, 2, 3, 2, 2, 2, 5,
        8, 5, 8, 3, 6, 4, 3, 5, 6, 3, 3, 4, 2, 5, 4, 4,
        4, 5, 6, 7, 3, 3, 3, 5, 4, 4, 3, 3, 1, 3, 3, 1,
        1, 5, 4, 3, 4, 5, 3, 5, 4, 2, 3, 4, 2, 5, 4, 4,
        4, 4, 2, 6, 3, 3, 3, 4, 4, 2, 3, 4, 1, 4, 3, 3,
        4, 2, 8, 3, 3, 3, 3, 6,
    };

    static const u8 KAN_CHARWIDTH[104] =
    {
        3, 1, 2, 4, 3, 4, 3, 2, 2, 2, 3, 3, 2, 3, 2, 3,
        3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 3, 3, 3, 3,
        4, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
        3, 3, 3, 3, 3, 3, 3, 5, 3, 3, 3, 2, 3, 2, 3, 4,
        2, 3, 3, 3, 3, 3, 2, 3, 3, 1, 2, 3, 2, 5, 3, 3,
        3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 5,
        5, 5, 5, 5, 5, 5, 5, 5,
    };

    static const f32 KF_CHARWIDTH  = 8.0f;   // glyph cell is 8x8 (DrawText scales coords by 1/8)
    static const f32 KF_CHARHEIGHT = 8.0f;
    static const s32 KI_FIRST_CHAR = 32;
    static const s32 KI_LAST_CHAR  = 135;
}
