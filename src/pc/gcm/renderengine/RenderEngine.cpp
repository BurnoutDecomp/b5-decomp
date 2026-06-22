#include "pc/gcm/renderengine/RenderEngine.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// renderengine namespace-scope helpers. VertexFormatGetStride reproduces the X360 stride switch
// (0x82B61310) value-for-value; TextureS (0x82856560) reproduces the dictionary-find wrapper
// including its initialised-flag guard and the entry + 0x0C value offset.

namespace renderengine
{
    // 0x82B61310 -- byte size of one vertex element for the given format code. The branch tree is
    // the compiler's binary search; the reconstruction states the same code -> size mapping.
    int VertexFormatGetStride(int liFormatCode)
    {
        if (liFormatCode <= 2761104)
        {
            if (liFormatCode == 2761104)
                return 4;
            if (liFormatCode > 1712775)
            {
                if (liFormatCode <= 1713059)
                {
                    switch (liFormatCode)
                    {
                    case 1713059:
                    case 1712803:
                        return 16;
                    case 1712986:
                    case 1712992:
                        return 8;
                    case 1713030:
                        return 4;
                    }
                    if (liFormatCode == 1713031)
                        return 8;
                    return 0;
                }
                if (liFormatCode == 1713062)
                    return 16;
                if (liFormatCode == 2760839 || liFormatCode == 2760848 || liFormatCode == 2760849)
                    return 4;
                if ((liFormatCode - 2760839) == 256)
                    return 4;
                return 0;
            }
            if (liFormatCode == 1712775)
                return 4;
            if (liFormatCode <= 1712474)
            {
                if (liFormatCode != 1712474)
                {
                    if (liFormatCode == 85126 || liFormatCode == 1583238)
                        return 4;
                    if (liFormatCode != 1712218)
                    {
                        if (liFormatCode != 1712262 && liFormatCode != 1712263)
                        {
                            if (liFormatCode == 1712291)
                                return 16;
                            return 0;
                        }
                        return 4;
                    }
                }
                return 8;
            }
            if (liFormatCode == 1712518 || liFormatCode == 1712519)
                return 4;
            if (liFormatCode == 1712547)
                return 16;
            if ((liFormatCode - 1712518) == 212)
                return 8;
            if ((liFormatCode - 1712518) == 256)
                return 4;
            return 0;
        }

        if (liFormatCode <= 2892194)
        {
            if (liFormatCode == 2892194)
                return 8;
            if (liFormatCode <= 2761616)
            {
                if (liFormatCode == 2761616 || liFormatCode == 2761105 || liFormatCode == 2761351
                    || liFormatCode == 2761360 || liFormatCode == 2761361)
                    return 4;
                if (liFormatCode == 2761607)
                    return 4;
                return 0;
            }
            if (liFormatCode == 2761617)
                return 4;
            if (liFormatCode == 2761657)
                return 12;
            if (liFormatCode == 2891865)
                return 4;
            if (((liFormatCode - 2892689) + 824) == 73)
                return 8;
            return 0;
        }

        if (liFormatCode > 2892709)
        {
            if (liFormatCode == 2916513 || liFormatCode == 2916769 || liFormatCode == 2917025
                || liFormatCode == 2917281 || liFormatCode == 2917284)
                return 4;
        }
        else
        {
            switch (liFormatCode)
            {
            case 2892709:
                return 8;
            case 2892377:
                return 4;
            case 2892450:
                return 8;
            case 2892633:
            case 2892639:
                return 4;
            case 2892706:
                return 8;
            }
        }
        return 0;
    }

    // 0x82856560 -- dictionary find that returns the stored Texture* (entry value at +0x0C). The
    // X360 guards the access with the HashTable's initialised flag (byte at +0x12C) and the assert
    // string "HashTable accessed when uninitialised" from CgsHashTable.h.
    int TextureS(int liDictionary, int liKey)
    {
        const u8* lpInitialisedFlag =
            reinterpret_cast<const u8*>(static_cast<usize>(static_cast<u32>(liDictionary))) + 0x12C;
        CGS_ASSERT(*lpInitialisedFlag != 0, "HashTable accessed when uninitialised");

        int liEntry = renderengine_dictionary_find(liDictionary, static_cast<unsigned int>(liKey), 0);
        if (liEntry != 0)
        {
            liEntry += 0xC;
        }
        return liEntry;
    }
}
