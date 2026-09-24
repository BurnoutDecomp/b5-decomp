// FX-VOICEPOOL (crash parity 2026-09-24), third piece: CgsSound::Utils::Curve::GetOutput
// @0x82689698 (DWARF CgsSoundUtils.cpp:159) -- the equal-power curve the sound fades, clutch,
// shift, in-air, road-noise, sweetener, reverb and music / stream / emitter fall-offs go through.
//
// The console (ARTIST asm 0x82689698..0x826897EC):
//   * E_POWER            fmuls x * -511 (flt_820AD414), fctiwz, `slwi 2 ; subf` off the table
//                        base 0x82F2D920: entry trunc(round_s(511 x));
//   * E_ONE_MINUS_EQPWR  its OWN lookup: fmsubs x * 511 - 511 (flt_820AA7B0) in one rounding,
//                        fctiwz, `slwi 2 ; subf`: entry trunc(round_s(511 (1 - x))), then 1 - entry;
//   * E_EQ_PWR_SQ / E_ONE_MINUS_EQPWR_SQ square those two; E_LINEAR returns x; any other curve
//     returns 0 (flt_82001CC0);
//   * the assert fires on `bgt 1.0` / `!bge 0.0` only -- a NaN passes it -- and a NaN index
//     (fctiwz 0x80000000) wraps to byte offset 0: entry 0.
// The table is gafArraySinTable[513] (DWARF CgsSoundUtils.cpp:85), .data 0x82F2D920, carried below
// as the raw image words (tools/re/x360rd.py) -- independent of the production file's decimal
// literals. The PC used to derive sin(i * pi / 1022) (max error 1.1e-3 at entry 277, equal at 2 of
// 512 entries) and ran E_ONE_MINUS_EQPWR as 1 - GetOutput(1 - x, E_POWER) (a second rounding:
// 16928 floats in [0, 1] read a neighbouring entry).
//
// run_fxvoicepool_curve.py extracts the PRODUCTION GetOutput (plus the table, its constant and
// the read helper where the revision has them) into fxvoicepool_curve_bodies.inc.
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

static unsigned guAsserts = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++guAsserts; return 0; }
void* EndAssert() { return nullptr; }
} }

namespace CgsSound { namespace Utils {
#include "fxvoicepool_curve_bodies.inc"
} }

using CgsSound::Utils::Curve;

// gafArraySinTable @0x82F2D920..0x82F2E124, the image's words.
static const uint32_t kau32ImageSinTable[513] =
{
    0x00000000u, 0x3B49320Eu, 0x3BC9320Eu, 0x3C16BB99u, 0x3C49081Cu, 0x3C7B54A0u, 0x3C96D091u, 0x3CAFE1DAu,
    0x3CC9081Cu, 0x3CE22E5Eu, 0x3CFB3FA7u, 0x3D0A32F4u, 0x3D16C615u, 0x3D234EBAu, 0x3D2FE1DAu, 0x3D3C6A7Fu,
    0x3D48FDA0u, 0x3D558644u, 0x3D621965u, 0x3D6EA20Au, 0x3D7B2AAEu, 0x3D83D9A9u, 0x3D8A1DFCu, 0x3D90624Eu,
    0x3D96A6A0u, 0x3D9CEAF2u, 0x3DA32F45u, 0x3DA97397u, 0x3DAFB7E9u, 0x3DB5F6FDu, 0x3DBC3B4Fu, 0x3DC27A63u,
    0x3DC8BEB6u, 0x3DCEFDCAu, 0x3DD53CDEu, 0x3DDB7BF2u, 0x3DE1BB06u, 0x3DE7FA1Au, 0x3DEE392Eu, 0x3DF47842u,
    0x3DFAB218u, 0x3E0075F7u, 0x3E039581u, 0x3E06B26Cu, 0x3E09CF57u, 0x3E0CEC42u, 0x3E10092Du, 0x3E132379u,
    0x3E164064u, 0x3E195AAFu, 0x3E1C779Au, 0x3E1F91E6u, 0x3E22AC32u, 0x3E25C67Eu, 0x3E28DE2Bu, 0x3E2BF877u,
    0x3E2F1023u, 0x3E3227D0u, 0x3E353F7Du, 0x3E38572Au, 0x3E3B6ED6u, 0x3E3E83E4u, 0x3E419B91u, 0x3E44B09Fu,
    0x3E47C5ACu, 0x3E4ADABAu, 0x3E4DED29u, 0x3E510236u, 0x3E5414A5u, 0x3E572713u, 0x3E5A3982u, 0x3E5D4BF1u,
    0x3E605BC0u, 0x3E636B90u, 0x3E667B5Fu, 0x3E698B2Fu, 0x3E6C9AFEu, 0x3E6FA82Fu, 0x3E72B55Fu, 0x3E75C28Fu,
    0x3E78CFC0u, 0x3E7BDA51u, 0x3E7EE782u, 0x3E80F909u, 0x3E827E52u, 0x3E84024Bu, 0x3E858644u, 0x3E870A3Du,
    0x3E888E37u, 0x3E8A1230u, 0x3E8B94D9u, 0x3E8D1783u, 0x3E8E9A2Cu, 0x3E901B86u, 0x3E919E30u, 0x3E931F8Au,
    0x3E949F95u, 0x3E9620EFu, 0x3E97A0F9u, 0x3E992104u, 0x3E9AA10Eu, 0x3E9C1FC9u, 0x3E9D9E84u, 0x3E9F1D3Fu,
    0x3EA09AAAu, 0x3EA21816u, 0x3EA39581u, 0x3EA512ECu, 0x3EA68F08u, 0x3EA80B24u, 0x3EA98740u, 0x3EAB020Cu,
    0x3EAC7CD9u, 0x3EADF7A5u, 0x3EAF7122u, 0x3EB0EA9Eu, 0x3EB2641Bu, 0x3EB3DC48u, 0x3EB55476u, 0x3EB6CCA3u,
    0x3EB84381u, 0x3EB9BBAEu, 0x3EBB313Cu, 0x3EBCA81Au, 0x3EBE1DA8u, 0x3EBF91E6u, 0x3EC10774u, 0x3EC27BB3u,
    0x3EC3EEA2u, 0x3EC562E1u, 0x3EC6D480u, 0x3EC8476Fu, 0x3EC9B90Fu, 0x3ECB2AAEu, 0x3ECC9AFEu, 0x3ECE0B4Eu,
    0x3ECF7B9Eu, 0x3ED0EA9Eu, 0x3ED2599Fu, 0x3ED3C89Fu, 0x3ED53650u, 0x3ED6A401u, 0x3ED81062u, 0x3ED97CC4u,
    0x3EDAE7D5u, 0x3EDC5437u, 0x3EDDBDF9u, 0x3EDF290Bu, 0x3EE092CDu, 0x3EE1FB40u, 0x3EE363B2u, 0x3EE4CC25u,
    0x3EE63348u, 0x3EE79A6Bu, 0x3EE9003Fu, 0x3EEA6613u, 0x3EEBCBE6u, 0x3EED306Au, 0x3EEE94EEu, 0x3EEFF823u,
    0x3EF15B57u, 0x3EF2BD3Cu, 0x3EF41F21u, 0x3EF57FB7u, 0x3EF6E04Cu, 0x3EF840E1u, 0x3EF9A027u, 0x3EFAFF6Du,
    0x3EFC5D64u, 0x3EFDBA0Au, 0x3EFF1801u, 0x3F0039ACu, 0x3F00E7FFu, 0x3F0195ABu, 0x3F0242AFu, 0x3F02EFB3u,
    0x3F039C0Fu, 0x3F04486Bu, 0x3F04F4C7u, 0x3F059FD3u, 0x3F064B88u, 0x3F06F694u, 0x3F07A0F9u, 0x3F084B5Eu,
    0x3F08F5C3u, 0x3F099F80u, 0x3F0A4895u, 0x3F0AF1AAu, 0x3F0B9A17u, 0x3F0C4285u, 0x3F0CEAF2u, 0x3F0D92B8u,
    0x3F0E39D6u, 0x3F0EE0F4u, 0x3F0F876Au, 0x3F102DE0u, 0x3F10D3AEu, 0x3F11797Du, 0x3F121EA3u, 0x3F12C3CAu,
    0x3F136849u, 0x3F140C20u, 0x3F14AFF7u, 0x3F1553CEu, 0x3F15F6FDu, 0x3F169985u, 0x3F173C0Cu, 0x3F17DDECu,
    0x3F187FCCu, 0x3F192104u, 0x3F19C23Bu, 0x3F1A62CCu, 0x3F1B02B4u, 0x3F1BA29Cu, 0x3F1C41DDu, 0x3F1CE11Eu,
    0x3F1D7FB7u, 0x3F1E1E4Fu, 0x3F1EBC41u, 0x3F1F598Au, 0x3F1FF6D3u, 0x3F209375u, 0x3F213016u, 0x3F21CC10u,
    0x3F226762u, 0x3F2302B4u, 0x3F239D5Eu, 0x3F243809u, 0x3F24D20Bu, 0x3F256C0Du, 0x3F2604C0u, 0x3F269E1Bu,
    0x3F273626u, 0x3F27CE31u, 0x3F28663Cu, 0x3F28FCF8u, 0x3F29945Bu, 0x3F2A2A6Fu, 0x3F2AC083u, 0x3F2B55EFu,
    0x3F2BEB5Bu, 0x3F2C801Fu, 0x3F2D143Cu, 0x3F2DA858u, 0x3F2E3BCDu, 0x3F2ECF42u, 0x3F2F6167u, 0x3F2FF38Cu,
    0x3F3085B2u, 0x3F31172Fu, 0x3F31A805u, 0x3F3238DAu, 0x3F32C908u, 0x3F33588Eu, 0x3F33E76Du, 0x3F34764Bu,
    0x3F350529u, 0x3F3592B8u, 0x3F362047u, 0x3F36AD2Eu, 0x3F373A15u, 0x3F37C654u, 0x3F3851ECu, 0x3F38DD83u,
    0x3F396873u, 0x3F39F2BBu, 0x3F3A7C5Bu, 0x3F3B05FBu, 0x3F3B8EF3u, 0x3F3C17ECu, 0x3F3CA03Cu, 0x3F3D27E5u,
    0x3F3DAEE6u, 0x3F3E35E7u, 0x3F3EBC41u, 0x3F3F41F2u, 0x3F3FC7A4u, 0x3F404C06u, 0x3F40D10Fu, 0x3F4154CAu,
    0x3F41D884u, 0x3F425B96u, 0x3F42DE01u, 0x3F43606Bu, 0x3F43E22Eu, 0x3F446349u, 0x3F44E3BDu, 0x3F456430u,
    0x3F45E3FCu, 0x3F466320u, 0x3F46E243u, 0x3F4760BFu, 0x3F47DE94u, 0x3F485BC0u, 0x3F48D8EDu, 0x3F4954CAu,
    0x3F49D14Eu, 0x3F4A4C83u, 0x3F4AC7B9u, 0x3F4B4246u, 0x3F4BBC2Cu, 0x3F4C3569u, 0x3F4CAEA7u, 0x3F4D273Du,
    0x3F4D9F2Cu, 0x3F4E1672u, 0x3F4E8DB9u, 0x3F4F0457u, 0x3F4F7A4Eu, 0x3F4FEF9Eu, 0x3F5064EDu, 0x3F50D8EDu,
    0x3F514CECu, 0x3F51C0ECu, 0x3F52339Cu, 0x3F52A64Cu, 0x3F531855u, 0x3F5389B5u, 0x3F53FA6Eu, 0x3F546B27u,
    0x3F54DB38u, 0x3F554AA1u, 0x3F55B963u, 0x3F562824u, 0x3F569596u, 0x3F570308u, 0x3F576FD2u, 0x3F57DC9Cu,
    0x3F584817u, 0x3F58B392u, 0x3F591E64u, 0x3F598890u, 0x3F59F213u, 0x3F5A5B96u, 0x3F5AC472u, 0x3F5B2CA5u,
    0x3F5B9431u, 0x3F5BFB16u, 0x3F5C61FAu, 0x3F5CC78Fu, 0x3F5D2D23u, 0x3F5D9210u, 0x3F5DF6FDu, 0x3F5E5A9Bu,
    0x3F5EBE38u, 0x3F5F2086u, 0x3F5F82D4u, 0x3F5FE521u, 0x3F604620u, 0x3F60A676u, 0x3F6106CDu, 0x3F61667Bu,
    0x3F61C582u, 0x3F6223E2u, 0x3F628241u, 0x3F62DF50u, 0x3F633C60u, 0x3F6398C8u, 0x3F63F488u, 0x3F644FA0u,
    0x3F64AA11u, 0x3F650481u, 0x3F655E4Au, 0x3F65B6C3u, 0x3F660F3Du, 0x3F6667B6u, 0x3F66BEE0u, 0x3F671562u,
    0x3F676BE3u, 0x3F67C1BEu, 0x3F6816F0u, 0x3F686B7Bu, 0x3F68BF5Du, 0x3F691299u, 0x3F6965D4u, 0x3F69B7BFu,
    0x3F6A09ABu, 0x3F6A5AEEu, 0x3F6AAB8Au, 0x3F6AFB7Fu, 0x3F6B4ACBu, 0x3F6B9A17u, 0x3F6BE814u, 0x3F6C3611u,
    0x3F6C8366u, 0x3F6CD014u, 0x3F6D1C19u, 0x3F6D6777u, 0x3F6DB2D5u, 0x3F6DFCE3u, 0x3F6E46F1u, 0x3F6E8FB0u,
    0x3F6ED86Fu, 0x3F6F2086u, 0x3F6F67F5u, 0x3F6FAF64u, 0x3F6FF584u, 0x3F703AFBu, 0x3F708073u, 0x3F70C543u,
    0x3F7108C4u, 0x3F714C44u, 0x3F718F1Du, 0x3F71D14Eu, 0x3F72137Fu, 0x3F725461u, 0x3F729542u, 0x3F72D4D4u,
    0x3F731466u, 0x3F735350u, 0x3F7390EBu, 0x3F73CE85u, 0x3F740C20u, 0x3F74486Bu, 0x3F74840Eu, 0x3F74BF0Au,
    0x3F74FA05u, 0x3F753459u, 0x3F756D5Du, 0x3F75A661u, 0x3F75DEBEu, 0x3F761672u, 0x3F764D7Fu, 0x3F7683E4u,
    0x3F76BA49u, 0x3F76EF5Fu, 0x3F7723CDu, 0x3F77583Au, 0x3F778C00u, 0x3F77BE77u, 0x3F77F0EDu, 0x3F7822BCu,
    0x3F7853E3u, 0x3F788462u, 0x3F78B439u, 0x3F78E411u, 0x3F791299u, 0x3F794079u, 0x3F796E59u, 0x3F799B91u,
    0x3F79C77Au, 0x3F79F362u, 0x3F7A1EA3u, 0x3F7A493Du, 0x3F7A732Eu, 0x3F7A9C78u, 0x3F7AC519u, 0x3F7AED14u,
    0x3F7B150Eu, 0x3F7B3BB8u, 0x3F7B61BBu, 0x3F7B87BEu, 0x3F7BAD19u, 0x3F7BD124u, 0x3F7BF530u, 0x3F7C1893u,
    0x3F7C3B4Fu, 0x3F7C5D64u, 0x3F7C7ED0u, 0x3F7C9F95u, 0x3F7CBFB1u, 0x3F7CDF26u, 0x3F7CFE9Bu, 0x3F7D1CC1u,
    0x3F7D3AE7u, 0x3F7D57BCu, 0x3F7D7492u, 0x3F7D9019u, 0x3F7DAB9Fu, 0x3F7DC67Eu, 0x3F7DE0B5u, 0x3F7DFA44u,
    0x3F7E132Bu, 0x3F7E2B6Bu, 0x3F7E4303u, 0x3F7E59F3u, 0x3F7E70E3u, 0x3F7E8683u, 0x3F7E9B7Cu, 0x3F7EB075u,
    0x3F7EC41Eu, 0x3F7ED7C7u, 0x3F7EEAC8u, 0x3F7EFC7Au, 0x3F7F0E2Cu, 0x3F7F1F36u, 0x3F7F2F98u, 0x3F7F3F53u,
    0x3F7F4E66u, 0x3F7F5CD1u, 0x3F7F6A94u, 0x3F7F7857u, 0x3F7F84CBu, 0x3F7F9097u, 0x3F7F9C63u, 0x3F7FA6DFu,
    0x3F7FB15Bu, 0x3F7FBA88u, 0x3F7FC3B5u, 0x3F7FCB92u, 0x3F7FD36Fu, 0x3F7FDAA5u, 0x3F7FE133u, 0x3F7FE719u,
    0x3F7FEC57u, 0x3F7FF0EDu, 0x3F7FF4DCu, 0x3F7FF823u, 0x3F7FFAC2u, 0x3F7FFD61u, 0x3F7FFEB0u, 0x3F800000u,
    0x3F800000u
};

static float FromBits(uint32_t u) { float f; std::memcpy(&f, &u, 4); return f; }
static uint32_t Bits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }
static float Image(int i) { return FromBits(kau32ImageSinTable[i]); }

// The console's entries, derived independently of the production body: E_POWER's is
// -fctiwz(round_s(x * -511)) == trunc(round_s(511 x)); E_ONE_MINUS_EQPWR's is
// -fctiwz(round_s(x * 511 - 511)), and for x >= 2^-21 the double x * 511 - 511 is EXACT (at most
// 53 significant bits), so one conversion to float is fmsubs's single rounding.
static int ConsolePowerEntry(float x) { return static_cast<int>(x * 511.0f); }
static int ConsoleOneMinusEntry(float x)
{
    return -static_cast<int>(static_cast<float>(static_cast<double>(x) * 511.0 - 511.0));
}

// Calls go through volatiles so /O2 cannot fold a NaN or a witness at compile time.
static float Out(float x, int curve)
{
    volatile float vx = x;
    volatile int vc = curve;
    return Curve::GetOutput(vx, static_cast<Curve::ECurveType>(static_cast<int>(vc)));
}

static int giChecks = 0;
static int giFailures = 0;
static void Check(bool ok, const char* label)
{
    ++giChecks;
    if (!ok) ++giFailures;
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", label);
}
static void CheckBits(float got, float want, const char* label)
{
    char line[512];
    std::snprintf(line, sizeof(line), "%s (got %.9g 0x%08X, console %.9g 0x%08X)", label,
                  got, Bits(got), want, Bits(want));
    Check(Bits(got) == Bits(want), line);
}

int main()
{
    // ---- the table itself --------------------------------------------------------------------
#ifdef FXVP_HAS_SIN_TABLE
    {
        int lnMismatch = 0, lnFirst = -1;
        for (int i = 0; i < 513; ++i)
            if (Bits(CgsSound::Utils::gafArraySinTable[i]) != kau32ImageSinTable[i])
            {
                if (lnFirst < 0) lnFirst = i;
                ++lnMismatch;
            }
        char line[256];
        std::snprintf(line, sizeof(line),
                      "gafArraySinTable[513] is the image's, word for word (%d mismatches, first %d)",
                      lnMismatch, lnFirst);
        Check(sizeof(CgsSound::Utils::gafArraySinTable) == 513 * sizeof(float) && lnMismatch == 0, line);
    }
#else
    Check(false, "gafArraySinTable[513] is the image's, word for word (the revision carries no table)");
#endif

    const Curve::ECurveType kLinear = Curve::E_LINEAR, kPower = Curve::E_POWER,
                            kPowerSq = Curve::E_EQ_PWR_SQ, kOneMinus = Curve::E_ONE_MINUS_EQPWR,
                            kOneMinusSq = Curve::E_ONE_MINUS_EQPWR_SQ;

    // ---- E_POWER / E_EQ_PWR_SQ ---------------------------------------------------------------
    CheckBits(Out(0.5f, kPower), Image(255),
              "E_POWER(0.5) reads entry 255 (the old sin(255 pi / 1022) is 0.708)");
    CheckBits(Out(277.0f / 511.0f, kPower), Image(277),
              "E_POWER(277/511) reads entry 277 (the old body's worst entry, 1.1e-3 off)");
    CheckBits(Out(1.0f, kPower), Image(511), "E_POWER(1) reads entry 511 = 1.0");
    CheckBits(Out(0.0f, kPower), Image(0), "E_POWER(0) reads entry 0 = 0.0");
    CheckBits(Out(0.5f, kPowerSq), Image(255) * Image(255), "E_EQ_PWR_SQ(0.5) = entry 255 squared (fmuls)");

    // ---- E_ONE_MINUS_EQPWR: its own fused lookup ---------------------------------------------
    CheckBits(Out(FromBits(0x3B004081u), kOneMinus), 1.0f - Image(510),
              "E_ONE_MINUS_EQPWR(0x3B004081) = 1 - entry 510 (a call into E_POWER with 1 - x reads 509)");
    CheckBits(Out(FromBits(0x33000001u), kOneMinus), 1.0f - Image(511),
              "E_ONE_MINUS_EQPWR(0x33000001, 2.98e-8) = 1 - entry 511 = 0 (511 x <= 2^-16; the call reads 510)");
    CheckBits(Out(FromBits(0x3C004041u), kOneMinus), 1.0f - Image(506),
              "E_ONE_MINUS_EQPWR(0x3C004041) = 1 - entry 506 (the call into E_POWER reads 507)");
    CheckBits(Out(FromBits(0x3E024123u), kOneMinus), 1.0f - Image(445),
              "E_ONE_MINUS_EQPWR(0x3E024123) = 1 - entry 445 (an unfused x * 511 - 511 reads 446)");
    CheckBits(Out(FromBits(0x3F024121u), kOneMinus), 1.0f - Image(250),
              "E_ONE_MINUS_EQPWR(0x3F024121) = 1 - entry 250 (an unfused x * 511 - 511 reads 251)");
    CheckBits(Out(0.0f, kOneMinus), 0.0f, "E_ONE_MINUS_EQPWR(0) = 1 - entry 511 = 0");
    CheckBits(Out(1.0f, kOneMinus), 1.0f, "E_ONE_MINUS_EQPWR(1) = 1 - entry 0 = 1");
    {
        const float lfOne = 1.0f - Image(510);
        CheckBits(Out(FromBits(0x3B004081u), kOneMinusSq), lfOne * lfOne,
                  "E_ONE_MINUS_EQPWR_SQ(0x3B004081) = (1 - entry 510) squared");
    }

    // ---- every float in [2^-8, 1] against the console's entries --------------------------------
    {
        long long lnPower = 0, lnOneMinus = 0, lnSwept = 0;
        uint32_t luFirstPower = 0, luFirstOneMinus = 0;
        for (uint32_t b = 0x3B800000u; b <= 0x3F800000u; ++b)
        {
            const float x = FromBits(b);
            if (Bits(Curve::GetOutput(x, kPower)) != Bits(Image(ConsolePowerEntry(x))))
            {
                if (!lnPower) luFirstPower = b;
                ++lnPower;
            }
            if (Bits(Curve::GetOutput(x, kOneMinus)) != Bits(1.0f - Image(ConsoleOneMinusEntry(x))))
            {
                if (!lnOneMinus) luFirstOneMinus = b;
                ++lnOneMinus;
            }
            ++lnSwept;
        }
        char line[256];
        std::snprintf(line, sizeof(line),
                      "E_POWER matches the console for all %lld floats in [2^-8, 1] (%lld differ, first 0x%08X)",
                      lnSwept, lnPower, luFirstPower);
        Check(lnPower == 0, line);
        std::snprintf(line, sizeof(line),
                      "E_ONE_MINUS_EQPWR matches the console for all %lld floats in [2^-8, 1] (%lld differ, first 0x%08X)",
                      lnSwept, lnOneMinus, luFirstOneMinus);
        Check(lnOneMinus == 0, line);
    }

    // ---- E_LINEAR / default ------------------------------------------------------------------
    CheckBits(Out(0.3f, kLinear), 0.3f, "E_LINEAR returns its input");
    CheckBits(Out(0.3f, 5), 0.0f, "an unknown curve returns 0 (flt_82001CC0)");

    // ---- NaN: no assert, entry 0 ---------------------------------------------------------------
    {
        const float lfNaN = std::numeric_limits<float>::quiet_NaN();
        guAsserts = 0;
        const float lfPower = Out(lfNaN, kPower);
        const float lfOneMinus = Out(lfNaN, kOneMinus);
        const unsigned luFired = guAsserts;
        CheckBits(lfPower, Image(0), "E_POWER(NaN) reads entry 0 (fctiwz 0x80000000 << 2 wraps to offset 0)");
        CheckBits(lfOneMinus, 1.0f - Image(0), "E_ONE_MINUS_EQPWR(NaN) = 1 - entry 0 = 1");
        char line[160];
        std::snprintf(line, sizeof(line),
                      "a NaN input does not fire the assert (bgt / bge pass an unordered compare): %u fired",
                      luFired);
        Check(luFired == 0, line);
    }

    // ---- the assert's range ------------------------------------------------------------------
    {
        guAsserts = 0; (void)Out(1.5f, kLinear);
        Check(guAsserts == 1, "1.5 fires the range assert once");
        guAsserts = 0; (void)Out(-0.25f, kLinear);
        Check(guAsserts == 1, "-0.25 fires the range assert once");
        guAsserts = 0; (void)Out(1.0f, kLinear); (void)Out(0.0f, kLinear); (void)Out(-0.0f, kLinear);
        Check(guAsserts == 0, "1, 0 and -0 do not fire it");
    }

    std::printf("FxVoicepoolCurve: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
