#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x828155E0
//   (CgsContainers::CgsHash16::CalculateHash)
//
// Behaviour-faithful to the X360 pseudocode/asm.
// This is a 16-bit hash (CRC-16 variant) using a 256-entry table.
//
// If the input length count <= 0, the function early-outs and returns the
// initial hash value 0xFFFFFFFF (as in the X360 register r3 initialization).
//
// Table note: the table is regenerated here using the polynomial 0x8810,
// which matches the table observed in the X360 binary at 0x82F31500.
//
// Signedness note: the asm sign-extends each byte (extsb) before the xor, and
// does NOT mask the index down to 8 bits, so a byte >= 0x80 would index outside
// the 256-entry table. We keep the index in range.

namespace CgsContainers
{
    namespace CgsHash16
    {
        namespace
        {
            struct Crc16Table
            {
                u32 entry[256];
            };

            const Crc16Table kCrc16 = {{
                0x00000000u, 0x00002314u, 0x00004628u, 0x0000653Cu, 0x00008C50u, 0x0000AF44u, 0x0000CA78u, 0x0000E96Cu,
                0x00000881u, 0x00002B95u, 0x00004EA9u, 0x00006DBDu, 0x000084D1u, 0x0000A7C5u, 0x0000C2F9u, 0x0000E1EDu,
                0x00001102u, 0x00003216u, 0x0000572Au, 0x0000743Eu, 0x00009D52u, 0x0000BE46u, 0x0000DB7Au, 0x0000F86Eu,
                0x00001983u, 0x00003A97u, 0x00005FABu, 0x00007CBFu, 0x000095D3u, 0x0000B6C7u, 0x0000D3FBu, 0x0000F0EFu,
                0x00002204u, 0x00000110u, 0x0000642Cu, 0x00004738u, 0x0000AE54u, 0x00008D40u, 0x0000E87Cu, 0x0000CB68u,
                0x00002A85u, 0x00000991u, 0x00006CADu, 0x00004FB9u, 0x0000A6D5u, 0x000085C1u, 0x0000E0FDu, 0x0000C3E9u,
                0x00003306u, 0x00001012u, 0x0000752Eu, 0x0000563Au, 0x0000BF56u, 0x00009C42u, 0x0000F97Eu, 0x0000DA6Au,
                0x00003B87u, 0x00001893u, 0x00007DAFu, 0x00005EBBu, 0x0000B7D7u, 0x000094C3u, 0x0000F1FFu, 0x0000D2EBu,
                0x00004408u, 0x0000671Cu, 0x00000220u, 0x00002134u, 0x0000C858u, 0x0000EB4Cu, 0x00008E70u, 0x0000AD64u,
                0x00004C89u, 0x00006F9Du, 0x00000AA1u, 0x000029B5u, 0x0000C0D9u, 0x0000E3CDu, 0x000086F1u, 0x0000A5E5u,
                0x0000550Au, 0x0000761Eu, 0x00001322u, 0x00003036u, 0x0000D95Au, 0x0000FA4Eu, 0x00009F72u, 0x0000BC66u,
                0x00005D8Bu, 0x00007E9Fu, 0x00001BA3u, 0x000038B7u, 0x0000D1DBu, 0x0000F2CFu, 0x000097F3u, 0x0000B4E7u,
                0x0000660Cu, 0x00004518u, 0x00002024u, 0x00000330u, 0x0000EA5Cu, 0x0000C948u, 0x0000AC74u, 0x00008F60u,
                0x00006E8Du, 0x00004D99u, 0x000028A5u, 0x00000BB1u, 0x0000E2DDu, 0x0000C1C9u, 0x0000A4F5u, 0x000087E1u,
                0x0000770Eu, 0x0000541Au, 0x00003126u, 0x00001232u, 0x0000FB5Eu, 0x0000D84Au, 0x0000BD76u, 0x00009E62u,
                0x00007F8Fu, 0x00005C9Bu, 0x000039A7u, 0x00001AB3u, 0x0000F3DFu, 0x0000D0CBu, 0x0000B5F7u, 0x000096E3u,
                0x00008810u, 0x0000AB04u, 0x0000CE38u, 0x0000ED2Cu, 0x00000440u, 0x00002754u, 0x00004268u, 0x0000617Cu,
                0x00008091u, 0x0000A385u, 0x0000C6B9u, 0x0000E5ADu, 0x00000CC1u, 0x00002FD5u, 0x00004AE9u, 0x000069FDu,
                0x00009912u, 0x0000BA06u, 0x0000DF3Au, 0x0000FC2Eu, 0x00001542u, 0x00003656u, 0x0000536Au, 0x0000707Eu,
                0x00009193u, 0x0000B287u, 0x0000D7BBu, 0x0000F4AFu, 0x00001DC3u, 0x00003ED7u, 0x00005BEBu, 0x000078FFu,
                0x0000AA14u, 0x00008900u, 0x0000EC3Cu, 0x0000CF28u, 0x00002644u, 0x00000550u, 0x0000606Cu, 0x00004378u,
                0x0000A295u, 0x00008181u, 0x0000E4BDu, 0x0000C7A9u, 0x00002EC5u, 0x00000DD1u, 0x000068EDu, 0x00004BF9u,
                0x0000BB16u, 0x00009802u, 0x0000FD3Eu, 0x0000DE2Au, 0x00003746u, 0x00001452u, 0x0000716Eu, 0x0000527Au,
                0x0000B397u, 0x00009083u, 0x0000F5BFu, 0x0000D6ABu, 0x00003FC7u, 0x00001CD3u, 0x000079EFu, 0x00005AFBu,
                0x0000CC18u, 0x0000EF0Cu, 0x00008A30u, 0x0000A924u, 0x00004048u, 0x0000635Cu, 0x00000660u, 0x00002574u,
                0x0000C499u, 0x0000E78Du, 0x000082B1u, 0x0000A1A5u, 0x000048C9u, 0x00006BDDu, 0x00000EE1u, 0x00002DF5u,
                0x0000DD1Au, 0x0000FE0Eu, 0x00009B32u, 0x0000B826u, 0x0000514Au, 0x0000725Eu, 0x00001762u, 0x00003476u,
                0x0000D59Bu, 0x0000F68Fu, 0x000093B3u, 0x0000B0A7u, 0x000059CBu, 0x00007ADFu, 0x00001FE3u, 0x00003CF7u,
                0x0000EE1Cu, 0x0000CD08u, 0x0000A834u, 0x00008B20u, 0x0000624Cu, 0x00004158u, 0x00002464u, 0x00000770u,
                0x0000E69Du, 0x0000C589u, 0x0000A0B5u, 0x000083A1u, 0x00006ACDu, 0x000049D9u, 0x00002CE5u, 0x00000FF1u,
                0x0000FF1Eu, 0x0000DC0Au, 0x0000B936u, 0x00009A22u, 0x0000734Eu, 0x0000505Au, 0x00003566u, 0x00001672u,
                0x0000F79Fu, 0x0000D48Bu, 0x0000B1B7u, 0x000092A3u, 0x00007BCFu, 0x000058DBu, 0x00003DE7u, 0x00001EF3u,
            }};
        }

        int CalculateHash(char* a1, int a2)
        {
            u32 result = 0xFFFFFFFFu;
            for (int i = 0; i < a2; ++i)
            {
                const s32 b = static_cast<s8>(a1[i]);            // extsb: char is signed
                const u32 idx = (result & 0xFFu) ^ static_cast<u32>(b);
                result = kCrc16.entry[idx & 0xFFu] ^ ((result >> 8) & 0xFFu);
            }
            return static_cast<int>(result);
        }
    }
}
