#include "SDKs/EATech/include/NFSMix/NFSMixShape.hpp"

// ===========================================================================
//  NFSMixShape -- pitch-conversion bodies, reconstructed from BURNOUT_X360_ARTIST.XEX
//  with the rodata tables extracted from the decrypted XEX (file offset = 0x3000 +
//  (vaddr - 0x82000000)). The two tables decompose 2^(cents/1200) into a per-semitone
//  factor (2^(i/12)) and a per-cent-fraction factor (2^(i/1200)); recovery verified
//  against those closed forms to float precision. Other recovered consts:
//    flt_82001C98 = 1.0   flt_82001D9C = 2.0 (octave)   flt_821478DC = 4096.0 (fixed pt)
// ===========================================================================

namespace NFSMixShape
{

// flt_82F8778C -- 2^(i/12), indexed by (cents/100) within an octave; [12]=0 padding.
static const float kSemitone[13] = {
    1.0f, 1.0594631433486938f, 1.1224620342254639f, 1.1892070770263672f, 1.2599210739135742f, 1.3348398208618164f,
    1.4142135381698608f, 1.4983071088790894f, 1.587401032447815f, 1.6817928552627563f, 1.7817974090576172f, 1.8877485990524292f,
    0.0f,
};

// flt_82F877C0 -- 2^(i/1200), indexed by (cents%100).
static const float kCentsFrac[100] = {
    1.0f, 1.0005778074264526f, 1.0011558532714844f, 1.0017343759536743f, 1.0023131370544434f, 1.002892255783081f,
    1.0034717321395874f, 1.0040515661239624f, 1.0046316385269165f, 1.0052121877670288f, 1.0057929754257202f, 1.0063741207122803f,
    1.0069555044174194f, 1.0075373649597168f, 1.0081194639205933f, 1.0087019205093384f, 1.0092848539352417f, 1.0098679065704346f,
    1.0104514360427856f, 1.0110353231430054f, 1.0116194486618042f, 1.0122039318084717f, 1.0127887725830078f, 1.0133739709854126f,
    1.013959527015686f, 1.0145453214645386f, 1.0151314735412598f, 1.0157181024551392f, 1.0163049697875977f, 1.0168921947479248f,
    1.017479658126831f, 1.0180675983428955f, 1.018655776977539f, 1.0192444324493408f, 1.0198333263397217f, 1.0204225778579712f,
    1.0210121870040894f, 1.0216020345687866f, 1.022192358970642f, 1.0227829217910767f, 1.0233738422393799f, 1.0239652395248413f,
    1.0245568752288818f, 1.0251487493515015f, 1.0257411003112793f, 1.0263338088989258f, 1.0269267559051514f, 1.0275201797485352f,
    1.028113842010498f, 1.0287078619003296f, 1.0293022394180298f, 1.0298969745635986f, 1.0304920673370361f, 1.0310873985290527f,
    1.0316832065582275f, 1.0322792530059814f, 1.0328757762908936f, 1.0334725379943848f, 1.0340696573257446f, 1.0346671342849731f,
    1.0352649688720703f, 1.0358630418777466f, 1.036461591720581f, 1.0370604991912842f, 1.0376596450805664f, 1.0382592678070068f,
    1.0388591289520264f, 1.0394593477249146f, 1.0400599241256714f, 1.0406608581542969f, 1.041262149810791f, 1.0418637990951538f,
    1.0424658060073853f, 1.0430680513381958f, 1.0436707735061646f, 1.0442737340927124f, 1.0448771715164185f, 1.0454808473587036f,
    1.0460848808288574f, 1.0466893911361694f, 1.0472941398620605f, 1.0478992462158203f, 1.0485047101974487f, 1.0491105318069458f,
    1.0497167110443115f, 1.050323247909546f, 1.0509300231933594f, 1.051537275314331f, 1.0521448850631714f, 1.0527527332305908f,
    1.0533610582351685f, 1.0539696216583252f, 1.0545786619186401f, 1.0551879405975342f, 1.0557975769042969f, 1.0564076900482178f,
    1.0570180416107178f, 1.0576287508010864f, 1.0582398176193237f, 1.0588512420654297f,
};

// ---------------------------------------------------------------------------
// GetPitchMultFromCents @0x82B44F30 -- 2^(cents/1200).
//   octave = 1.0;                                            (flt_82001C98)
//   while (cents >= 1200) { octave *= 2.0; cents -= 1200; }  (flt_82001D9C; via /1200 magic)
//   while (cents <= -1200){ octave *= 2.0; cents += 1200; }
//   if (original cents >= 0) return octave * kSemitone[cents/100] * kCentsFrac[cents%100];
//   else { a = -cents; return (1/kCentsFrac[a%100]) * ((1/kSemitone[a/100]) / octave); }
// (the sign test is on the ORIGINAL cents; after reduction a negative input leaves
//  cents in (-1200,0], so -cents is in [0,1200) -> table index in range.)
// ---------------------------------------------------------------------------
float GetPitchMultFromCents(int liCents)
{
    float lfOctave = 1.0f;
    const bool lbNeg = (liCents < 0);

    while (liCents >= 1200) { lfOctave *= 2.0f; liCents -= 1200; }
    while (liCents <= -1200) { lfOctave *= 2.0f; liCents += 1200; }

    if (!lbNeg)
        return lfOctave * kSemitone[liCents / 100] * kCentsFrac[liCents % 100];

    const int liA = -liCents;                                   // [0,1200)
    const float lfR = (1.0f / kSemitone[liA / 100]) / lfOctave; // f0 = f13/octave
    return (1.0f / kCentsFrac[liA % 100]) * lfR;
}

// ---------------------------------------------------------------------------
// GetIntPitchMultFromCents @0x82B45690 -- sign-extend the 16-bit cents, scale by 4096.
//   if (cents & 0x8000) cents |= 0xFFFF0000;   (extsh / oris)
//   return (int)(GetPitchMultFromCents(cents) * 4096.0);   (flt_821478DC; fctiwz)
// ---------------------------------------------------------------------------
int GetIntPitchMultFromCents(int liCents)
{
    const short lsCents = static_cast<short>(liCents);   // sign-extend 16-bit
    return static_cast<int>(GetPitchMultFromCents(lsCents) * 4096.0f);
}

} // namespace NFSMixShape
