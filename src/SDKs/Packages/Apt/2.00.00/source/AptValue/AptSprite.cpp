#include "SDKs/Packages/Apt/2.00.00/source/AptValue/AptSprite.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   SpriteMembersIndex::in_word_set @ 0x82AD6800
//   SpriteMembersIndex::hash        @ 0x82AD6798
// (gperf perfect-hash recognizer for the Apt "Sprite" value object's member /
//  method names; called by AptCIH::objectMemberLookup / objectMemberSet and
//  AptNativeHash::UpdateObjectMethods).
//
// Faithful to the X360 pseudocode:
//   - length gate:  (len - 2) > 0x12  =>  reject   (2 <= len <= 20)
//   - hash:         len + asso[str0] + asso[str1] (+asso[str5] if len>=6)
//                                                 (+asso[str7] if len>=8)
//   - hash gate:    hash > 0x119 (281)  =>  reject
//   - entry:        &wordlist[hash]      (DIRECT hash index -- no lookup[] table,
//                                         no duplicate-key resolution)
//   - first-char compare, then strcmp(str+1, name+1); return Entry* or 0.

const SpriteMembersIndex::Entry*
SpriteMembersIndex::in_word_set(const char* lpcStr, unsigned int luLen)
{
    // Length gate: only words of length [MIN_WORD_LENGTH .. MAX_WORD_LENGTH].
    if (luLen - MIN_WORD_LENGTH > MAX_WORD_LENGTH - MIN_WORD_LENGTH)   // (a2 - 2) > 0x12
        return 0;

    const unsigned int luKey = hash(lpcStr, luLen);                   // v3
    if (luKey > MAX_HASH_VALUE)                                       // v3 > 0x119
        return 0;

    // Direct hash index into the wordlist (X360: &off_82F78FD8 + 2 * v3).
    const Entry* lpEntry = &wordlist[luKey];                          // result
    const char*  lpcName = lpEntry->mpcName;                          // *result

    if ((unsigned char)lpcStr[0] != (unsigned char)lpcName[0])        // *a1 != **result
        return 0;

    const char* lpcN = lpcName + 1;                                   // v5 = *result + 1
    const char* lpcS = lpcStr  + 1;                                   // v6 = a1 + 1
    int liDiff;                                                       // v7
    do
    {
        liDiff = (unsigned char)*lpcS - (unsigned char)*lpcN;         // v7 = *v6 - *v5
        if (*lpcS == '\0')                                            // !*v6
            break;
        ++lpcS;
        ++lpcN;
    }
    while (liDiff == 0);                                              // while ( !v7 )

    if (liDiff != 0)                                                  // if ( v7 )
        return 0;
    return lpEntry;                                                   // return result
}

// --- gperf hash (asso_values[] @ 0x82F725B0) -----------------------------
// h  = len
// h += asso_values[str[0]]                  (always)
// h += asso_values[str[1]]                  (len >= 2 -- always true past the gate)
// h += asso_values[str[5]]                  (len >= 6)
// h += asso_values[str[7]]                  (len >= 8)
// The X360 control flow (LABEL_5 / LABEL_7 fall-through with the len 1/5/7
// branch points) is preserved structurally below.
unsigned int
SpriteMembersIndex::hash(const char* lpcStr, unsigned int luLen)
{
    const unsigned char* lpucStr = (const unsigned char*)lpcStr;     // a1
    unsigned int luHashVal = luLen;                                  // a2

    if (luLen != 1)
    {
        if (luLen > 1)
        {
            if (luLen <= 5)
            {
                // len in [2..5]: positions 1 then 0 only.
                luHashVal += asso_values[lpucStr[1]];
                return asso_values[lpucStr[0]] + luHashVal;
            }
            if (luLen > 7)
                luHashVal += asso_values[lpucStr[7]];                // len >= 8
            luHashVal += asso_values[lpucStr[5]];                    // len >= 6
        }
        // (len <= 1 with len != 1 is unreachable past the length gate; it would
        //  fall through to add position 7 -- harmless, omitted.)
        luHashVal += asso_values[lpucStr[1]];
        return asso_values[lpucStr[0]] + luHashVal;
    }
    return asso_values[lpucStr[0]] + luHashVal;                      // len == 1
}

// ===========================================================================
// gperf static data tables -- EXTRACTED from the decrypted X360 ARTIST.XEX
// .rdata (file_off = 0x3000 + vaddr - 0x82000000, big-endian):
//     asso_values[256] @ dword_82F725B0   (u32: the hash reads it dword-indexed
//                                          via `b << 2` -- rotlwi r10,r10,2; lwzx)
//     wordlist[0x11A]  @ off_82F78FD8      (Entry{ const char* name; u32 id };
//                                          0x11A == MAX_HASH_VALUE + 1 slots,
//                                          indexed DIRECTLY by hash value)
// All 80 non-empty Sprite member/method names were verified to resolve back to
// their own slot through the reconstructed in_word_set above. Empty slots hold
// {"", 0}; their first char '\0' can never match a length-gated input string,
// exactly as the gperf-emitted empty entries behave.
// ===========================================================================

const u32 SpriteMembersIndex::asso_values[256] =
{
    282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282,
    282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282,
    282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282,
    282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282,
    282, 282,  15, 282,  45, 282,  15,  10, 282,   0, 282, 282,  25,  25, 282, 282,
     20, 282, 282,  40,   0,  40, 282,  20, 282, 282, 282, 282, 282, 282, 282,   0,
    282,  20,  10,  10,   5,   0,  40,  50, 100,  40, 282, 282,  35,  10,   0,  60,
     20,  20,   0,   0,  90, 110,   0,  25,  30,  75,   0, 282, 282, 282, 282, 282,
    282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282,
    282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282,
    282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282,
    282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282,
    282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282,
    282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282,
    282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282,
    282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282
};
const SpriteMembersIndex::Entry SpriteMembersIndex::wordlist[0x11A] =
{
    /*   0 */ { "", 0 },
    /*   1 */ { "", 0 },
    /*   2 */ { "_z",                  128 },
    /*   3 */ { "", 0 },
    /*   4 */ { "", 0 },
    /*   5 */ { "_name",               14 },
    /*   6 */ { "escape",              28 },
    /*   7 */ { "setMask",             122 },
    /*   8 */ { "", 0 },
    /*   9 */ { "", 0 },
    /*  10 */ { "", 0 },
    /*  11 */ { "", 0 },
    /*  12 */ { "", 0 },
    /*  13 */ { "", 0 },
    /*  14 */ { "", 0 },
    /*  15 */ { "removeTextField",     119 },
    /*  16 */ { "", 0 },
    /*  17 */ { "", 0 },
    /*  18 */ { "_visible",            8 },
    /*  19 */ { "nextFrame",           113 },
    /*  20 */ { "", 0 },
    /*  21 */ { "", 0 },
    /*  22 */ { "", 0 },
    /*  23 */ { "", 0 },
    /*  24 */ { "", 0 },
    /*  25 */ { "createTextField",     114 },
    /*  26 */ { "", 0 },
    /*  27 */ { "", 0 },
    /*  28 */ { "", 0 },
    /*  29 */ { "", 0 },
    /*  30 */ { "", 0 },
    /*  31 */ { "", 0 },
    /*  32 */ { "_x",                  1 },
    /*  33 */ { "", 0 },
    /*  34 */ { "", 0 },
    /*  35 */ { "", 0 },
    /*  36 */ { "extern",              23 },
    /*  37 */ { "_xmouse",             21 },
    /*  38 */ { "", 0 },
    /*  39 */ { "prevFrame",           108 },
    /*  40 */ { "createEmptyMovieClip", 116 },
    /*  41 */ { "", 0 },
    /*  42 */ { "", 0 },
    /*  43 */ { "", 0 },
    /*  44 */ { "", 0 },
    /*  45 */ { "isNaN",               26 },
    /*  46 */ { "_alpha",              7 },
    /*  47 */ { "", 0 },
    /*  48 */ { "", 0 },
    /*  49 */ { "", 0 },
    /*  50 */ { "_focusrect",          18 },
    /*  51 */ { "", 0 },
    /*  52 */ { "_renderflags",        131 },
    /*  53 */ { "", 0 },
    /*  54 */ { "", 0 },
    /*  55 */ { "", 0 },
    /*  56 */ { "", 0 },
    /*  57 */ { "", 0 },
    /*  58 */ { "setTextFormat",       125 },
    /*  59 */ { "play",                107 },
    /*  60 */ { "", 0 },
    /*  61 */ { "", 0 },
    /*  62 */ { "", 0 },
    /*  63 */ { "", 0 },
    /*  64 */ { "", 0 },
    /*  65 */ { "", 0 },
    /*  66 */ { "", 0 },
    /*  67 */ { "onPress",             211 },
    /*  68 */ { "", 0 },
    /*  69 */ { "onRelease",           212 },
    /*  70 */ { "", 0 },
    /*  71 */ { "onLoad",              207 },
    /*  72 */ { "_xscale",             3 },
    /*  73 */ { "", 0 },
    /*  74 */ { "", 0 },
    /*  75 */ { "removeMovieClip",     109 },
    /*  76 */ { "onReleaseOutside",    213 },
    /*  77 */ { "_y",                  2 },
    /*  78 */ { "", 0 },
    /*  79 */ { "", 0 },
    /*  80 */ { "", 0 },
    /*  81 */ { "getURL",              102 },
    /*  82 */ { "_ymouse",             22 },
    /*  83 */ { "", 0 },
    /*  84 */ { "", 0 },
    /*  85 */ { "", 0 },
    /*  86 */ { "onData",              200 },
    /*  87 */ { "onEnterFrame",        203 },
    /*  88 */ { "_framesloaded",       13 },
    /*  89 */ { "", 0 },
    /*  90 */ { "", 0 },
    /*  91 */ { "getNewTextFormat",    123 },
    /*  92 */ { "onMouseWheel",        218 },
    /*  93 */ { "", 0 },
    /*  94 */ { "stop",                110 },
    /*  95 */ { "onSetFocus",          216 },
    /*  96 */ { "onMouseMove",         209 },
    /*  97 */ { "_target",             12 },
    /*  98 */ { "", 0 },
    /*  99 */ { "", 0 },
    /* 100 */ { "_xrotation",          129 },
    /* 101 */ { "setInterval",         24 },
    /* 102 */ { "Boolean",             29 },
    /* 103 */ { "", 0 },
    /* 104 */ { "", 0 },
    /* 105 */ { "onRollOver",          215 },
    /* 106 */ { "_droptarget",         15 },
    /* 107 */ { "onKeyUp",             205 },
    /* 108 */ { "getTextFormat",       124 },
    /* 109 */ { "onMouseUp",           210 },
    /* 110 */ { "", 0 },
    /* 111 */ { "", 0 },
    /* 112 */ { "", 0 },
    /* 113 */ { "_currentframe",       5 },
    /* 114 */ { "_url",                16 },
    /* 115 */ { "", 0 },
    /* 116 */ { "onMouseDown",         208 },
    /* 117 */ { "_yscale",             4 },
    /* 118 */ { "localToGlobal",       127 },
    /* 119 */ { "", 0 },
    /* 120 */ { "onDragOver",          202 },
    /* 121 */ { "", 0 },
    /* 122 */ { "", 0 },
    /* 123 */ { "", 0 },
    /* 124 */ { "", 0 },
    /* 125 */ { "swapDepths",          120 },
    /* 126 */ { "", 0 },
    /* 127 */ { "", 0 },
    /* 128 */ { "_soundbuftime",       19 },
    /* 129 */ { "", 0 },
    /* 130 */ { "", 0 },
    /* 131 */ { "_width",              9 },
    /* 132 */ { "", 0 },
    /* 133 */ { "onUnload",            217 },
    /* 134 */ { "", 0 },
    /* 135 */ { "", 0 },
    /* 136 */ { "", 0 },
    /* 137 */ { "_totalframes",        6 },
    /* 138 */ { "unescape",            27 },
    /* 139 */ { "onKeyDown",           204 },
    /* 140 */ { "", 0 },
    /* 141 */ { "gotoAndPlay",         103 },
    /* 142 */ { "", 0 },
    /* 143 */ { "_quality",            20 },
    /* 144 */ { "", 0 },
    /* 145 */ { "_yrotation",          130 },
    /* 146 */ { "", 0 },
    /* 147 */ { "hitTest",             118 },
    /* 148 */ { "clearInterval",       25 },
    /* 149 */ { "", 0 },
    /* 150 */ { "", 0 },
    /* 151 */ { "", 0 },
    /* 152 */ { "_highquality",        17 },
    /* 153 */ { "getBytesTotal",       112 },
    /* 154 */ { "getBytesLoaded",      111 },
    /* 155 */ { "", 0 },
    /* 156 */ { "", 0 },
    /* 157 */ { "", 0 },
    /* 158 */ { "", 0 },
    /* 159 */ { "_rotation",           11 },
    /* 160 */ { "", 0 },
    /* 161 */ { "gotoAndStop",         104 },
    /* 162 */ { "", 0 },
    /* 163 */ { "", 0 },
    /* 164 */ { "startDrag",           126 },
    /* 165 */ { "", 0 },
    /* 166 */ { "onKillFocus",         206 },
    /* 167 */ { "", 0 },
    /* 168 */ { "loadVariables",       106 },
    /* 169 */ { "", 0 },
    /* 170 */ { "", 0 },
    /* 171 */ { "", 0 },
    /* 172 */ { "", 0 },
    /* 173 */ { "", 0 },
    /* 174 */ { "getBounds",           117 },
    /* 175 */ { "", 0 },
    /* 176 */ { "", 0 },
    /* 177 */ { "", 0 },
    /* 178 */ { "getDepth",            115 },
    /* 179 */ { "", 0 },
    /* 180 */ { "", 0 },
    /* 181 */ { "", 0 },
    /* 182 */ { "", 0 },
    /* 183 */ { "", 0 },
    /* 184 */ { "", 0 },
    /* 185 */ { "", 0 },
    /* 186 */ { "unloadMovie",         121 },
    /* 187 */ { "", 0 },
    /* 188 */ { "", 0 },
    /* 189 */ { "", 0 },
    /* 190 */ { "", 0 },
    /* 191 */ { "", 0 },
    /* 192 */ { "", 0 },
    /* 193 */ { "", 0 },
    /* 194 */ { "", 0 },
    /* 195 */ { "", 0 },
    /* 196 */ { "", 0 },
    /* 197 */ { "", 0 },
    /* 198 */ { "", 0 },
    /* 199 */ { "", 0 },
    /* 200 */ { "", 0 },
    /* 201 */ { "", 0 },
    /* 202 */ { "", 0 },
    /* 203 */ { "", 0 },
    /* 204 */ { "loadMovie",           105 },
    /* 205 */ { "", 0 },
    /* 206 */ { "", 0 },
    /* 207 */ { "_height",             10 },
    /* 208 */ { "", 0 },
    /* 209 */ { "", 0 },
    /* 210 */ { "", 0 },
    /* 211 */ { "", 0 },
    /* 212 */ { "", 0 },
    /* 213 */ { "", 0 },
    /* 214 */ { "onRollOut",           214 },
    /* 215 */ { "", 0 },
    /* 216 */ { "", 0 },
    /* 217 */ { "", 0 },
    /* 218 */ { "", 0 },
    /* 219 */ { "", 0 },
    /* 220 */ { "", 0 },
    /* 221 */ { "", 0 },
    /* 222 */ { "", 0 },
    /* 223 */ { "", 0 },
    /* 224 */ { "", 0 },
    /* 225 */ { "", 0 },
    /* 226 */ { "", 0 },
    /* 227 */ { "", 0 },
    /* 228 */ { "", 0 },
    /* 229 */ { "onDragOut",           201 },
    /* 230 */ { "", 0 },
    /* 231 */ { "", 0 },
    /* 232 */ { "", 0 },
    /* 233 */ { "duplicateMovieClip",  101 },
    /* 234 */ { "", 0 },
    /* 235 */ { "", 0 },
    /* 236 */ { "", 0 },
    /* 237 */ { "", 0 },
    /* 238 */ { "", 0 },
    /* 239 */ { "", 0 },
    /* 240 */ { "", 0 },
    /* 241 */ { "", 0 },
    /* 242 */ { "", 0 },
    /* 243 */ { "", 0 },
    /* 244 */ { "", 0 },
    /* 245 */ { "", 0 },
    /* 246 */ { "", 0 },
    /* 247 */ { "", 0 },
    /* 248 */ { "", 0 },
    /* 249 */ { "", 0 },
    /* 250 */ { "", 0 },
    /* 251 */ { "", 0 },
    /* 252 */ { "", 0 },
    /* 253 */ { "", 0 },
    /* 254 */ { "", 0 },
    /* 255 */ { "", 0 },
    /* 256 */ { "", 0 },
    /* 257 */ { "", 0 },
    /* 258 */ { "", 0 },
    /* 259 */ { "", 0 },
    /* 260 */ { "", 0 },
    /* 261 */ { "", 0 },
    /* 262 */ { "", 0 },
    /* 263 */ { "", 0 },
    /* 264 */ { "", 0 },
    /* 265 */ { "", 0 },
    /* 266 */ { "", 0 },
    /* 267 */ { "", 0 },
    /* 268 */ { "", 0 },
    /* 269 */ { "", 0 },
    /* 270 */ { "", 0 },
    /* 271 */ { "", 0 },
    /* 272 */ { "", 0 },
    /* 273 */ { "", 0 },
    /* 274 */ { "", 0 },
    /* 275 */ { "", 0 },
    /* 276 */ { "", 0 },
    /* 277 */ { "", 0 },
    /* 278 */ { "", 0 },
    /* 279 */ { "", 0 },
    /* 280 */ { "", 0 },
    /* 281 */ { "attachMovie",         100 },
};