// DirtySDK lobby -- tagfield record codec (core/source/util/lobbytagfield.c).
//
// A tagfield record is flat text: NAME=value fields separated by the divider (a
// newline). Every setter appends (or replaces) one field through
// _TagFieldSetupAppend, which removes an existing field of the same name, checks the
// space left and writes "<divider>NAME="; the setter then writes its value text and
// terminates the record. Getters take the value text TagFieldFind returned.
//
// Only the entry points the game build carries are reconstructed; the rest of the
// SDK module (TagFieldDivider/Format/Rename/Find2/GetNumber64/GetFlags/SetDate/
// GetDate/SetFloat/First/FindNext/GetStructureOffsets) is not part of it.

#include "lobbytagfield.h"
#include "dirtylib.h"   // NetPrintf
#include "platform.h"   // ds_snzprintf, ds_timeinsecs, ds_secstotime, ds_timetosecs

#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <ctime>

// The five translation tables, laid out back to back exactly as the SDK links them.
// Two getters index hex_decode with a sign-extended character, so a byte >= 0x80
// reads the 128 bytes that precede the table (the tail of char_encode and
// hex_encode); keeping the block contiguous reproduces those reads byte for byte.
static const unsigned char _TagField_aTables[0x310 + 33] =
{
    // char_encode[256] (+0x000): bit 0 = escape in a string, bit 1 = escape in a
    // token, bit 2 = escape in a structure string, bit 3 = forces quoting.
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x0E, 0x04, 0x07, 0x04, 0x04, 0x07, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0C, 0x04, 0x04, 0x04,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    // hex_encode[16] (+0x100)
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
    // hex_decode[256] (+0x110): digit value, 0x80 for a non-hex character.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    // to_upper[256] (+0x210)
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
    0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
    0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
    0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
    0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
    0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
    0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
    // flag_encode[33] (+0x310): "@ABCDEFGHIJKLMNOPQRSTUVWXYZ0123-"
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x30, 0x31, 0x32, 0x33, 0x2D,
    0x00,
};

static const unsigned char* const char_encode = _TagField_aTables + 0x000;
static const unsigned char* const hex_encode  = _TagField_aTables + 0x100;
static const unsigned char* const hex_decode  = _TagField_aTables + 0x110;
static const unsigned char* const to_upper    = _TagField_aTables + 0x210;
static const char* const          flag_encode = reinterpret_cast<const char*>(_TagField_aTables + 0x310);

// Field divider, and whether a setter also writes it after its value (TagFieldDivider,
// which would change them, is not part of the build, so these keep their defaults).
static char    g_divider  = '\n';
static s32     g_divafter = 1;

// Remove any existing pName field from the record, make sure iMinData value bytes (plus
// divider, name, '=' and terminator) still fit, then append "<divider>pName=" and
// return the write position. A NULL pName empties the record and appends no name.
// Returns NULL when the record is unusable or too small.
static unsigned char* _TagFieldSetupAppend(unsigned char* pRecord, s32 iReclen, const char* pName, s32 iMinData)
{
    s32 iNameLen;
    s32 iInclEqu = 1;
    s32 iSeparate;
    unsigned char* pData = pRecord;
    unsigned char* pFind;
    unsigned char* pFindCmp;
    unsigned char* pFindDiv;
    const unsigned char* pNameCmp;

    if ((pRecord == NULL) || (*pRecord == '='))
    {
        return NULL;
    }
    if (pName == NULL)
    {
        *pRecord = 0;
        iInclEqu = 0;
        pName = "";
    }

    // step over as many record bytes as the name is long (never past the terminator)
    for (iNameLen = 0; pName[iNameLen] != 0; ++iNameLen)
    {
        if (*pData != 0)
        {
            ++pData;
        }
    }

    // pData walks the record looking for '='; pFind trails it by the name length
    for (pFind = pData - iNameLen; *pData != 0; ++pData, ++pFind)
    {
        if (*pData != '=')
        {
            continue;
        }
        if (pData[-1] <= ' ')
        {
            return NULL;
        }
        if ((pFind != pRecord) && (pFind[-1] > ' '))
        {
            continue;
        }
        for (pFindCmp = pFind, pNameCmp = reinterpret_cast<const unsigned char*>(pName);
             to_upper[*pFindCmp] == to_upper[*pNameCmp]; ++pFindCmp, ++pNameCmp)
        {
        }
        if (*pNameCmp == 0)
        {
            break;
        }
    }

    // an existing field of this name: slide the rest of the record down over it
    if (*pData != 0)
    {
        pFindDiv = NULL;
        pFindCmp = pData;
        if (*pFindCmp >= ' ')
        {
            for (;;)
            {
                if (*pFindCmp == ' ')
                {
                    pFindDiv = pFindCmp;
                }
                if ((*pFindCmp == '=') && (pFindDiv != NULL))
                {
                    pFindCmp = pFindDiv;
                    break;
                }
                ++pFindCmp;
                if (*pFindCmp < ' ')
                {
                    break;
                }
            }
        }
        while ((*pFindCmp < ' ') && (*pFindCmp != 0))
        {
            ++pFindCmp;
        }
        for (pData -= iNameLen; (*pData = *pFindCmp) != 0; ++pData, ++pFindCmp)
        {
        }
    }

    // a divider goes in unless the record is empty or already ends in one
    iSeparate = 1;
    if ((pData == pRecord) || (pData[-1] < ' ') || (pData[-1] == g_divider))
    {
        iSeparate = 0;
    }
    if ((s32)(pRecord - pData) + iReclen < g_divafter + iSeparate + iNameLen + iMinData + 2)
    {
        return NULL;
    }
    if (iSeparate)
    {
        *pData++ = (unsigned char)g_divider;
    }
    while (*pName != 0)
    {
        *pData++ = (unsigned char)*pName++;
    }
    if (iInclEqu)
    {
        *pData++ = '=';
    }
    *pData = 0;
    return pData;
}

// A value did not fit: cut the record back to the start of the field being appended,
// report it and fail.
static s32 _TagFieldSetupTerm(unsigned char* pRecord, unsigned char* pAppend)
{
    while (pAppend != pRecord)
    {
        --pAppend;
        if (*pAppend < ' ')
        {
            if (*pAppend == '\n')
            {
                ++pAppend;
            }
            break;
        }
    }
    NetPrintf(("lobbytagfield: tagfield buffer overflow appending record %s\n", pAppend));
    *pAppend = 0;
    return -1;
}

// Parse up to iDigits decimal digits into *pValue (accumulating onto its current value).
static const char* _ParseNumber(const char* pData, u32* pValue, s32 iDigits)
{
    while (((signed char)*pData >= '0') && ((signed char)*pData <= '9') && (iDigits > 0))
    {
        *pValue = (*pValue * 10) + (*pData & 15);
        ++pData;
        --iDigits;
    }
    return pData;
}

extern "C" s32 TagFieldDupl(char* pRecord, s32 iReclen, const char* pData)
{
    s32 iLimit;

    for (iLimit = iReclen; (iLimit > 1) && (*pData != 0); --iLimit)
    {
        *pRecord++ = *pData++;
    }
    if (iLimit > 0)
    {
        *pRecord = 0;
    }
    return iReclen - iLimit;
}

extern "C" s32 TagFieldMerge(char* pRecord_, s32 iReclen, const char* pMerge_)
{
    unsigned char* pRecord = reinterpret_cast<unsigned char*>(pRecord_);
    unsigned char* pLimit = pRecord + iReclen - 1;
    const unsigned char* pMerge = reinterpret_cast<const unsigned char*>(pMerge_);
    unsigned char* pSrc;
    unsigned char* pDst;
    unsigned char* pName;
    unsigned char uChar;

    if ((pMerge == NULL) || (*pMerge == 0))
    {
        return 0;
    }

    // compact the record in place, dropping every field the merge record also has
    pName = pDst = pSrc = pRecord;
    if (*pRecord != 0)
    {
        do
        {
            uChar = *pSrc;
            if (uChar <= ' ')
            {
                *pDst++ = uChar;
                ++pSrc;
                pName = pSrc;
            }
            else if (uChar != '=')
            {
                *pDst++ = uChar;
                ++pSrc;
            }
            else if (pSrc == pName)
            {
                *pDst = 0;
                break;
            }
            else
            {
                *pSrc = 0;
                if (TagFieldFind(pMerge_, reinterpret_cast<const char*>(pName)) == NULL)
                {
                    *pSrc++ = '=';
                    *pDst++ = '=';
                }
                else
                {
                    pDst += pName - pSrc;
                    do
                    {
                        ++pSrc;
                    }
                    while ((*pSrc != 0) && (*pSrc != '='));
                    if (*pSrc != 0)
                    {
                        while (pSrc[-1] > ' ')
                        {
                            --pSrc;
                        }
                    }
                    pName = pSrc;
                }
            }
        }
        while (*pSrc != 0);

        while ((pDst != pRecord) && (pDst[-1] <= ' '))
        {
            --pDst;
        }
    }

    // no room left for a divider plus anything from the merge record
    if (pDst >= pLimit - 1)
    {
        *pDst = 0;
        if (pDst + 1 < pRecord + iReclen)
        {
            if (g_divafter)
            {
                *pDst++ = (unsigned char)g_divider;
            }
            *pDst = 0;
        }
        return -1;
    }

    // append the merge record
    if (pDst != pRecord)
    {
        *pDst++ = (unsigned char)g_divider;
    }
    while ((*pMerge != 0) && (pDst < pLimit))
    {
        *pDst++ = *pMerge++;
    }
    if (*pMerge != 0)
    {
        return _TagFieldSetupTerm(pRecord, pDst);
    }
    *pDst = 0;
    return 1;
}

extern "C" s32 TagFieldDelete(char* _record, const char* name)
{
    unsigned char* data;
    unsigned char* head;
    unsigned char* tail;
    unsigned char* last;
    unsigned char* record = reinterpret_cast<unsigned char*>(_record);

    data = (unsigned char*)TagFieldFind(_record, name);
    if (data == NULL)
    {
        return -1;
    }

    // back up to the start of the name
    for (head = data; (head != record) && (head[-1] > ' '); --head)
    {
    }

    // the value ends at the last space before the next "name=" (or at a control char)
    tail = data;
    for (last = data; (*last >= ' ') && (*last != '='); ++last)
    {
        if (*last == ' ')
        {
            tail = last;
        }
    }
    if (*last != '=')
    {
        tail = last;
    }
    while ((*tail != 0) && (*tail <= ' '))
    {
        ++tail;
    }

    while (*tail != 0)
    {
        *head++ = *tail++;
    }
    while ((head != record) && (head[-1] <= ' '))
    {
        --head;
    }
    *head = 0;
    return 0;
}

extern "C" const char* TagFieldFind(const char* pRecord_, const char* pName_)
{
    s32 iNameLen;
    unsigned char uMatch = 0;
    const unsigned char* pCmp1;
    const unsigned char* pCmp2;
    const unsigned char* pName = reinterpret_cast<const unsigned char*>(pName_);
    const unsigned char* pRecord = reinterpret_cast<const unsigned char*>(pRecord_);

    if ((pRecord == NULL) || (pName == NULL) || (*pName == 0))
    {
        return NULL;
    }

    // fast path: does the record open with this name?
    for (pCmp2 = pName; *pCmp2 != 0; )
    {
        if (*pRecord == 0)
        {
            return NULL;
        }
        uMatch |= to_upper[*pRecord++] ^ to_upper[*pCmp2++];
    }
    iNameLen = (s32)(pCmp2 - pName);
    if ((uMatch == 0) && (*pRecord == '='))
    {
        return reinterpret_cast<const char*>(pRecord + 1);
    }
    if (*pRecord == 0)
    {
        return NULL;
    }

    // scan the rest for "<separator>name="
    for (pCmp1 = pRecord + 1; *pCmp1 != 0; ++pCmp1)
    {
        const unsigned char* pStart;
        s32 iIndex;

        if (*pCmp1 != '=')
        {
            continue;
        }
        pStart = pCmp1 - iNameLen;
        if (pCmp1[-1] <= ' ')
        {
            return NULL;
        }
        if (pStart[-1] > ' ')
        {
            continue;
        }
        for (iIndex = 0; to_upper[pStart[iIndex]] == to_upper[pName[iIndex]]; ++iIndex)
        {
        }
        if (pStart + iIndex == pCmp1)
        {
            return reinterpret_cast<const char*>(pCmp1 + 1);
        }
    }
    return NULL;
}

extern "C" const char* TagFieldFindIdx(const char* pRecord, const char* pName, s32 iIdx)
{
    char strTag[256];

    ds_snzprintf(strTag, sizeof(strTag), "%s%d", pName, iIdx);
    return TagFieldFind(pRecord, strTag);
}

extern "C" s32 TagFieldSetRaw(char* pRecord_, s32 iReclen, const char* pName, const char* pData_)
{
    unsigned char uTerm;
    unsigned char* pAppend;
    unsigned char* pRecord = reinterpret_cast<unsigned char*>(pRecord_);
    const unsigned char* pData = reinterpret_cast<const unsigned char*>(pData_);

    if (pData == NULL)
    {
        TagFieldDelete(pRecord_, pName);
        return 0;
    }
    if ((pAppend = _TagFieldSetupAppend(pRecord, iReclen, pName, 0)) == NULL)
    {
        return -1;
    }

    // copy up to the first separator; inside quotes a space does not end the value
    for (uTerm = '!'; *pData >= uTerm; )
    {
        if (pAppend >= pRecord + iReclen - 2)
        {
            return _TagFieldSetupTerm(pRecord, pAppend);
        }
        uTerm ^= (*pData == '"') ? 1 : 0;
        *pAppend++ = *pData++;
    }

    if ((g_divafter != 0) && (pName != NULL))
    {
        *pAppend++ = (unsigned char)g_divider;
    }
    *pAppend = 0;
    return (s32)(pAppend - pRecord);
}

extern "C" s32 TagFieldGetRaw(const char* pData, char* pBuffer, s32 iBuflen, const char* pDefval)
{
    s32 iLen;
    unsigned char uTerm;
    const unsigned char* pPtr;

    if (pData == NULL)
    {
        if (pDefval == NULL)
        {
            return -1;
        }
        for (iLen = 1; (iLen < iBuflen) && (*pDefval != 0); ++iLen)
        {
            *pBuffer++ = *pDefval++;
        }
    }
    else
    {
        pPtr = reinterpret_cast<const unsigned char*>(pData);
        for (iLen = 1, uTerm = '!'; (iLen < iBuflen) && (*pPtr >= uTerm); ++iLen)
        {
            uTerm ^= (*pPtr == '"') ? 1 : 0;
            *pBuffer++ = (char)*pPtr++;
        }
    }
    *pBuffer = 0;
    return iLen - 1;
}

extern "C" s32 TagFieldSetNumber(char* pRecord, s32 iReclen, const char* pName, s32 iValue)
{
    s32 iLength;
    u32 uValue;
    unsigned char* pData;
    unsigned char* pAppend;
    unsigned char strData[12];

    if ((iValue >= 0) && (iValue < 10))
    {
        // single digit fast path
        if ((pAppend = _TagFieldSetupAppend(reinterpret_cast<unsigned char*>(pRecord), iReclen, pName, 1)) == NULL)
        {
            return -1;
        }
        *pAppend++ = (unsigned char)('0' + iValue);
    }
    else
    {
        uValue = (iValue < 0) ? (u32)-iValue : (u32)iValue;
        pData = strData + sizeof(strData) - 1;
        *pData = 0;
        do
        {
            *--pData = (unsigned char)('0' + (uValue % 10));
            uValue /= 10;
        }
        while (uValue != 0);
        if (iValue < 0)
        {
            *--pData = '-';
        }
        iLength = (s32)(strData + sizeof(strData) - 1 - pData);
        if ((pAppend = _TagFieldSetupAppend(reinterpret_cast<unsigned char*>(pRecord), iReclen, pName, iLength)) == NULL)
        {
            return -1;
        }
        while (*pData != 0)
        {
            *pAppend++ = *pData++;
        }
    }

    if ((g_divafter != 0) && (pName != NULL))
    {
        *pAppend++ = (unsigned char)g_divider;
    }
    *pAppend = 0;
    return (s32)(pAppend - reinterpret_cast<unsigned char*>(pRecord));
}

extern "C" s32 TagFieldSetNumber64(char* pRecord, s32 iReclen, const char* pName, s64 iValue)
{
    s32 iLength;
    s32 iNeg = 0;
    unsigned char* pData;
    unsigned char* pAppend;
    unsigned char strData[20];

    // values a 32-bit decimal field holds comfortably go through TagFieldSetNumber
    if (((u64)iValue + 9999) <= 19998)
    {
        return TagFieldSetNumber(pRecord, iReclen, pName, (s32)iValue);
    }

    pData = strData + sizeof(strData) - 1;
    *pData = 0;
    if (iValue < 0)
    {
        iValue = (s64)(0 - (u64)iValue);
        iNeg = 1;
    }
    while (iValue > 0)
    {
        *--pData = hex_encode[iValue & 15];
        iValue >>= 4;
    }
    if (*pData == 0)
    {
        *--pData = '0';
    }
    *--pData = '$';
    if (iNeg)
    {
        *--pData = '-';
    }

    iLength = (s32)(strData + sizeof(strData) - 1 - pData);
    if ((pAppend = _TagFieldSetupAppend(reinterpret_cast<unsigned char*>(pRecord), iReclen, pName, iLength)) == NULL)
    {
        return -1;
    }
    while (*pData != 0)
    {
        *pAppend++ = *pData++;
    }
    if ((g_divafter != 0) && (pName != NULL))
    {
        *pAppend++ = (unsigned char)g_divider;
    }
    *pAppend = 0;
    return (s32)(pAppend - reinterpret_cast<unsigned char*>(pRecord));
}

extern "C" s32 TagFieldGetNumber(const char* pData, s32 iDefval)
{
    s32 iSign = 1;
    u32 uDigit;
    u32 uValue;
    u32 uBase = 10;

    if (pData == NULL)
    {
        return iDefval;
    }
    if (*pData == '-')
    {
        iSign = -1;
        ++pData;
    }
    else if (*pData == '$')
    {
        uBase = 16;
        ++pData;
    }
    else if (*pData == '+')
    {
        ++pData;
    }

    // the digit lookup uses the signed character value (see the table block)
    for (uValue = 0; (uDigit = hex_decode[(signed char)*pData]) < uBase; ++pData)
    {
        uValue = (uValue * uBase) + uDigit;
    }
    return (s32)(uValue * (u32)iSign);
}

extern "C" s32 TagFieldSetFlags(char* pRecord, s32 iReclen, const char* pName, s32 iValue)
{
    const char* pFlags;
    unsigned char* pAppend;
    unsigned char* pLimit = reinterpret_cast<unsigned char*>(pRecord) + iReclen - g_divafter - 1;

    if ((pAppend = _TagFieldSetupAppend(reinterpret_cast<unsigned char*>(pRecord), iReclen, pName, 0)) == NULL)
    {
        return -1;
    }
    for (pFlags = flag_encode; (iValue != 0) && (*pFlags != 0); ++pFlags, iValue >>= 1)
    {
        if (iValue & 1)
        {
            if (pAppend >= pLimit)
            {
                return _TagFieldSetupTerm(reinterpret_cast<unsigned char*>(pRecord), pAppend);
            }
            *pAppend++ = (unsigned char)*pFlags;
        }
    }
    if ((g_divafter != 0) && (pName != NULL))
    {
        *pAppend++ = (unsigned char)g_divider;
    }
    *pAppend = 0;
    return (s32)(pAppend - reinterpret_cast<unsigned char*>(pRecord));
}

extern "C" s32 TagFieldSetAddress(char* pRecord, s32 iReclen, const char* pName, u32 uAddr)
{
    s32 iIndex;
    unsigned char uValue;
    unsigned char* pData;
    unsigned char* pAppend;
    unsigned char Segment[4];
    unsigned char strAddr[16];

    // network order: most significant octet first
    Segment[0] = (unsigned char)(uAddr >> 24);
    Segment[1] = (unsigned char)(uAddr >> 16);
    Segment[2] = (unsigned char)(uAddr >> 8);
    Segment[3] = (unsigned char)(uAddr >> 0);

    for (pData = strAddr, iIndex = 0; iIndex < 4; ++iIndex)
    {
        uValue = Segment[iIndex];
        if (iIndex > 0)
        {
            *pData++ = '.';
        }
        if (uValue > 9)
        {
            if (uValue > 99)
            {
                *pData++ = (unsigned char)('0' + (uValue / 100));
                uValue %= 100;
            }
            *pData++ = (unsigned char)('0' + (uValue / 10));
            uValue %= 10;
        }
        *pData++ = (unsigned char)('0' + uValue);
    }
    *pData = 0;

    if ((pAppend = _TagFieldSetupAppend(reinterpret_cast<unsigned char*>(pRecord), iReclen, pName, (s32)(pData - strAddr))) == NULL)
    {
        return -1;
    }
    for (pData = strAddr; *pData != 0; )
    {
        *pAppend++ = *pData++;
    }
    if ((g_divafter != 0) && (pName != NULL))
    {
        *pAppend++ = (unsigned char)g_divider;
    }
    *pAppend = 0;
    return (s32)(pAppend - reinterpret_cast<unsigned char*>(pRecord));
}

extern "C" u32 TagFieldGetAddress(const char* pData, u32 uDefval)
{
    u32 uAddr = 0;

    if (pData == NULL)
    {
        return uDefval;
    }
    for (;; ++pData)
    {
        if ((*pData >= '0') && (*pData <= '9'))
        {
            uAddr = (uAddr & 0xFFFFFF00u) | (((uAddr & 0xFFu) * 10) + (*pData & 15));
        }
        else if (*pData == '.')
        {
            uAddr <<= 8;
        }
        else
        {
            return uAddr;
        }
    }
}

extern "C" s32 TagFieldSetToken(char* pRecord, s32 iReclen, const char* pName, s32 iToken)
{
    s32 iIndex;
    unsigned char uByte;
    unsigned char strToken[16];
    unsigned char* pData;
    unsigned char* pAppend;
    u32 uToken = (u32)iToken;

    // four bytes, most significant first, escaping the ones a token cannot carry
    pData = strToken + sizeof(strToken) - 1;
    *pData = 0;
    for (iIndex = 4; iIndex != 0; --iIndex)
    {
        uByte = (unsigned char)uToken;
        uToken >>= 8;
        if (char_encode[uByte] & 2)
        {
            *--pData = hex_encode[uByte & 15];
            *--pData = hex_encode[uByte >> 4];
            *--pData = '%';
        }
        else
        {
            *--pData = uByte;
        }
    }

    if ((pAppend = _TagFieldSetupAppend(reinterpret_cast<unsigned char*>(pRecord), iReclen, pName, (s32)(strToken + sizeof(strToken) - 1 - pData))) == NULL)
    {
        return -1;
    }
    while (*pData != 0)
    {
        *pAppend++ = *pData++;
    }
    if ((g_divafter != 0) && (pName != NULL))
    {
        *pAppend++ = (unsigned char)g_divider;
    }
    *pAppend = 0;
    return (s32)(pAppend - reinterpret_cast<unsigned char*>(pRecord));
}

extern "C" s32 TagFieldGetToken(const char* pData_, s32 iDefault)
{
    s32 iIndex;
    unsigned char uByte;
    u32 uToken = 0;
    const unsigned char* pData = reinterpret_cast<const unsigned char*>(pData_);

    if ((pData == NULL) || (*pData <= ' '))
    {
        return iDefault;
    }
    for (iIndex = 4; iIndex != 0; --iIndex)
    {
        uByte = *pData++;
        if (uByte < ' ')
        {
            // a short token is padded with spaces
            --pData;
            uByte = ' ';
        }
        if (uByte == '%')
        {
            uByte = (unsigned char)((hex_decode[pData[0]] << 4) | hex_decode[pData[1]]);
            pData += 2;
        }
        uToken = (uToken << 8) | uByte;
    }
    return (s32)uToken;
}

extern "C" s32 TagFieldSetString(char* pRecord_, s32 iReclen, const char* pName, const char* pValue_)
{
    s32 iRemain;
    unsigned char* pAppend;
    const unsigned char* pSpace;
    unsigned char* pRecord = reinterpret_cast<unsigned char*>(pRecord_);
    const unsigned char* pValue = reinterpret_cast<const unsigned char*>(pValue_);

    if (pValue == NULL)
    {
        TagFieldDelete(pRecord_, pName);
        return 0;
    }
    if ((pAppend = _TagFieldSetupAppend(pRecord, iReclen, pName, 2)) == NULL)
    {
        return -1;
    }
    iRemain = (s32)(pRecord - pAppend) + iReclen - 1;

    // a value holding a space or comma is quoted
    for (pSpace = pValue; *pSpace != 0; ++pSpace)
    {
        if ((*pSpace == ' ') || (*pSpace == ','))
        {
            *pAppend++ = '"';
            iRemain -= 2;
            break;
        }
    }

    for (; (*pValue != 0) && (iRemain > 0); ++pValue)
    {
        if (char_encode[*pValue] & 1)
        {
            if (iRemain >= 3)
            {
                *pAppend++ = '%';
                *pAppend++ = hex_encode[*pValue >> 4];
                *pAppend++ = hex_encode[*pValue & 15];
            }
            iRemain -= 3;
        }
        else
        {
            *pAppend++ = *pValue;
            iRemain -= 1;
        }
    }
    if ((*pSpace == ' ') || (*pSpace == ','))
    {
        *pAppend++ = '"';
    }

    if (iRemain < g_divafter)
    {
        return _TagFieldSetupTerm(pRecord, pAppend);
    }
    if ((g_divafter != 0) && (pName != NULL))
    {
        *pAppend++ = (unsigned char)g_divider;
    }
    *pAppend = 0;
    return (s32)(pAppend - pRecord);
}

extern "C" s32 TagFieldGetString(const char* data, char* buffer, s32 buflen, const char* defval)
{
    s32 len;
    unsigned char term;
    const unsigned char* ptr;

    if (data == NULL)
    {
        if (defval == NULL)
        {
            return -1;
        }
        if (buffer == NULL)
        {
            return (s32)strlen(defval);
        }
        for (len = 1; (len < buflen) && (*defval != 0); ++len)
        {
            *buffer++ = *defval++;
        }
        *buffer = 0;
        return len - 1;
    }

    ptr = reinterpret_cast<const unsigned char*>(data);
    term = ' ';
    if (*ptr == '"')
    {
        term = '"';
        ++ptr;
    }

    // no buffer: return the decoded length
    if (buffer == NULL)
    {
        for (len = 0; *ptr != term; ++len, ++ptr)
        {
            if (*ptr < ' ')
            {
                break;
            }
            if ((*ptr == '%') && (ptr[1] >= ' ') && (ptr[2] >= ' '))
            {
                ptr += 2;
            }
        }
        return len;
    }

    if (buflen < 1)
    {
        return -1;
    }
    for (len = 1; len < buflen; ++len)
    {
        if ((*ptr == term) || (*ptr < ' '))
        {
            break;
        }
        if ((*ptr == '%') && (ptr[1] == '%'))
        {
            *buffer++ = '%';
            ptr += 2;
        }
        else if ((*ptr == '%') && (ptr[1] >= ' ') && (ptr[2] >= ' '))
        {
            *buffer++ = (char)((hex_decode[ptr[1]] << 4) | hex_decode[ptr[2]]);
            ptr += 3;
        }
        else
        {
            *buffer++ = (char)*ptr++;
        }
    }
    *buffer = 0;
    return len - 1;
}

extern "C" s32 TagFieldSetBinary(char* pRecord, s32 iReclen, const char* pName, const void* pValue_, s32 iVallen)
{
    unsigned char* pAppend;
    const unsigned char* pValue = static_cast<const unsigned char*>(pValue_);

    if (iVallen < 0)
    {
        return -1;
    }
    if ((pAppend = _TagFieldSetupAppend(reinterpret_cast<unsigned char*>(pRecord), iReclen, pName, (iVallen * 2) + 1)) == NULL)
    {
        return -1;
    }
    *pAppend++ = '$';
    for (; iVallen > 0; --iVallen, ++pValue)
    {
        *pAppend++ = hex_encode[*pValue >> 4];
        *pAppend++ = hex_encode[*pValue & 15];
    }
    if ((g_divafter != 0) && (pName != NULL))
    {
        *pAppend++ = (unsigned char)g_divider;
    }
    *pAppend = 0;
    return (s32)(pAppend - reinterpret_cast<unsigned char*>(pRecord));
}

extern "C" s32 TagFieldSetBinary7(char* pRecord, s32 iReclen, const char* pName, const void* pValue_, s32 iVallen)
{
    s32 iBits;
    u32 uBuffer;
    unsigned char* pAppend;
    const unsigned char* pValue = static_cast<const unsigned char*>(pValue_);

    if (iVallen < 0)
    {
        return -1;
    }
    if ((pAppend = _TagFieldSetupAppend(reinterpret_cast<unsigned char*>(pRecord), iReclen, pName, (((iVallen * 8) + 6) / 7) + 1)) == NULL)
    {
        return -1;
    }

    // seven data bits per character, the high bit always set
    *pAppend++ = '^';
    for (iBits = 0, uBuffer = 0; iVallen > 0; --iVallen)
    {
        uBuffer |= (u32)*pValue++ << iBits;
        iBits += 8;
        while (iBits >= 7)
        {
            *pAppend++ = (unsigned char)((uBuffer & 0x7F) | 0x80);
            uBuffer >>= 7;
            iBits -= 7;
        }
    }
    if (iBits > 0)
    {
        *pAppend++ = (unsigned char)((uBuffer & 0x7F) | 0x80);
    }

    if ((g_divafter != 0) && (pName != NULL))
    {
        *pAppend++ = (unsigned char)g_divider;
    }
    *pAppend = 0;
    return (s32)(pAppend - reinterpret_cast<unsigned char*>(pRecord));
}

extern "C" s32 TagFieldGetBinary(const char* pData_, void* pBuffer_, s32 iBuflen)
{
    s32 iBits;
    u32 uBin7;
    const unsigned char* pBinary;
    const unsigned char* pData = reinterpret_cast<const unsigned char*>(pData_);
    unsigned char* pBuffer = static_cast<unsigned char*>(pBuffer_);
    unsigned char* pBufend = pBuffer + iBuflen;

    if ((pData == NULL) || ((*pData != '$') && (*pData != '^')))
    {
        return -1;
    }

    // no buffer: return the decoded size
    if (pBuffer == NULL)
    {
        pBinary = pData + 1;
        if (*pData == '$')
        {
            while ((pBinary[0] >= '0') && (pBinary[1] >= '0'))
            {
                pBinary += 2;
            }
            return (s32)(pBinary - pData - 1) / 2;
        }
        while (*pBinary >= 0x80)
        {
            ++pBinary;
        }
        return ((s32)(pBinary - pData - 1) * 7) / 8;
    }

    if (iBuflen < 1)
    {
        return -1;
    }
    if (*pData == '$')
    {
        for (pBinary = pData + 1; (pBinary[0] >= '0') && (pBinary[1] >= '0') && (pBuffer < pBufend); pBinary += 2)
        {
            *pBuffer++ = (unsigned char)((hex_decode[pBinary[0]] << 4) | hex_decode[pBinary[1]]);
        }
    }
    else
    {
        for (pBinary = pData + 1, iBits = 0, uBin7 = 0; (*pBinary >= 0x80) && (pBuffer < pBufend); ++pBinary)
        {
            uBin7 |= (u32)(*pBinary & 0x7F) << iBits;
            iBits += 7;
            if (iBits >= 8)
            {
                *pBuffer++ = (unsigned char)uBin7;
                uBin7 >>= 8;
                iBits -= 8;
            }
        }
    }
    return (s32)(pBuffer - pBufend) + iBuflen;
}

extern "C" s32 TagFieldSetStructure(char* pRecord, s32 iReclen, const char* pName, const void* pStruct_, s32 iLength, const char* pPattern)
{
    s32 iShift;
    s32 iWidth;
    s32 iCount;
    u32 uValue = 0;
    unsigned char* pMarker;
    unsigned char* pAppend;
    const unsigned char* pStruct = static_cast<const unsigned char*>(pStruct_);
    const unsigned char* pLength = reinterpret_cast<unsigned char*>(pRecord) + iReclen - 2;
    const unsigned char* pLimit = (iLength < 0) ? pStruct + 0x10000 : pStruct + iLength;

    if ((pAppend = _TagFieldSetupAppend(reinterpret_cast<unsigned char*>(pRecord), iReclen, pName, 0)) == NULL)
    {
        return -1;
    }

    while ((*pPattern != 0) && (pStruct < pLimit))
    {
        // "#name=" labels the element and is skipped
        if (*pPattern == '#')
        {
            ++pPattern;
            while (*pPattern != 0)
            {
                if (*pPattern++ == '=')
                {
                    break;
                }
            }
        }

        // optional decimal count
        for (iCount = 0; ((signed char)*pPattern >= '0') && ((signed char)*pPattern <= '9'); ++pPattern)
        {
            iCount = (iCount * 10) + (*pPattern & 15);
        }

        // "<n>a" skips struct bytes
        if (*pPattern == 'a')
        {
            pStruct += (iCount != 0) ? iCount : 1;
        }

        // numeric elements are written as hex, the width bounding the text
        iWidth = 0;
        if (*pPattern == TAGFIELD_PATTERN_INT8)
        {
            uValue = *pStruct;
            iWidth = 2;
            pStruct += 1;
        }
        if (*pPattern == TAGFIELD_PATTERN_INT16)
        {
            u16 uHalf;
            memcpy(&uHalf, pStruct, sizeof(uHalf));
            uValue = uHalf;
            iWidth = 4;
            pStruct += 2;
        }
        if (*pPattern == TAGFIELD_PATTERN_INT32)
        {
            memcpy(&uValue, pStruct, sizeof(uValue));
            iWidth = 8;
            pStruct += 4;
        }
        if (iWidth > 0)
        {
            if (pAppend + iWidth > pLength)
            {
                return _TagFieldSetupTerm(reinterpret_cast<unsigned char*>(pRecord), pAppend);
            }
            pMarker = pAppend;
            if (uValue > 0xFFFF0000u)
            {
                *pAppend++ = '-';
                uValue = 0u - uValue;
            }
            for (iShift = 28; iShift >= 0; iShift -= 4)
            {
                if ((uValue >> iShift) & 15)
                {
                    break;
                }
            }
            for (; iShift >= 0; iShift -= 4)
            {
                *pAppend++ = hex_encode[(uValue >> iShift) & 15];
            }
            if (pAppend < pMarker + iWidth)
            {
                *pAppend++ = ',';
            }
        }

        // "<n>s" writes a string of at most n bytes, escaping what the record cannot carry
        if ((*pPattern == TAGFIELD_PATTERN_STR) && (iCount > 0))
        {
            s32 iIndex;
            for (iIndex = 0; (iIndex < iCount) && (*pStruct != 0); ++iIndex, ++pStruct)
            {
                if (pAppend + 3 > pLength)
                {
                    return _TagFieldSetupTerm(reinterpret_cast<unsigned char*>(pRecord), pAppend);
                }
                if (char_encode[*pStruct] & 4)
                {
                    *pAppend++ = '%';
                    *pAppend++ = hex_encode[*pStruct >> 4];
                    *pAppend++ = hex_encode[*pStruct & 15];
                }
                else
                {
                    *pAppend++ = *pStruct;
                }
            }
            pStruct += iCount - iIndex;
            if (pAppend + 1 > pLength)
            {
                return _TagFieldSetupTerm(reinterpret_cast<unsigned char*>(pRecord), pAppend);
            }
            *pAppend++ = ',';
        }

        // "*" repeats the current element until the struct is consumed
        if (pPattern[1] != '*')
        {
            ++pPattern;
        }
    }

    // drop trailing element separators
    while (pAppend[-1] == ',')
    {
        --pAppend;
    }
    if ((g_divafter != 0) && (pName != NULL))
    {
        *pAppend++ = (unsigned char)g_divider;
    }
    *pAppend = 0;
    return (s32)(pAppend - reinterpret_cast<unsigned char*>(pRecord));
}

extern "C" s32 TagFieldGetStructure(const char* _pData, void* _pBuf, s32 iLen, const char* pPat)
{
    char iNeg;
    s32 iSiz;
    s32 iCnt;
    char strTmp[1024];
    u32 uHex;
    u32 uRaw;
    unsigned char* pData = (unsigned char*)_pData;
    unsigned char* pBuf = static_cast<unsigned char*>(_pBuf);
    unsigned char* pEnd;
    s32 iDecodeLen;

    // the end is taken from the caller's buffer pointer before a NULL is replaced
    pEnd = reinterpret_cast<unsigned char*>(reinterpret_cast<uintptr_t>(_pBuf) + ((iLen < 0) ? 0xFFFF : (uintptr_t)(intptr_t)iLen));
    if (pData == NULL)
    {
        pData = (unsigned char*)"";
    }
    if (_pBuf == NULL)
    {
        _pBuf = strTmp;
        pBuf = reinterpret_cast<unsigned char*>(strTmp);
    }

    while ((*pPat != 0) && (*pData > ' '))
    {
        // optional decimal count
        for (iCnt = 0; ((signed char)*pPat >= '0') && ((signed char)*pPat <= '9'); ++pPat)
        {
            iCnt = (iCnt * 10) + (*pPat & 15);
        }

        iSiz = 0;
        if (*pPat == TAGFIELD_PATTERN_INT32)
        {
            iSiz = 8;
        }
        else if (*pPat == TAGFIELD_PATTERN_INT8)
        {
            iSiz = 2;
        }
        else if (*pPat == TAGFIELD_PATTERN_INT16)
        {
            iSiz = 4;
        }

        if (iSiz > 0)
        {
            // up to iSiz hex digits, optionally negated; a comma follows a short value
            iNeg = (char)*pData;
            if (iNeg == '-')
            {
                ++pData;
            }
            for (uHex = 0; (uRaw = hex_decode[*pData]) < 16; )
            {
                uHex = (uHex << 4) | uRaw;
                ++pData;
                if (--iSiz <= 0)
                {
                    break;
                }
            }
            if ((iSiz > 0) && (*pData == ','))
            {
                ++pData;
            }
            if (iNeg == '-')
            {
                uHex = 0u - uHex;
            }
            if (*pPat == TAGFIELD_PATTERN_INT32)
            {
                memcpy(pBuf, &uHex, sizeof(u32));
                pBuf += 4;
            }
            else if (*pPat == TAGFIELD_PATTERN_INT8)
            {
                *pBuf = (unsigned char)uHex;
                pBuf += 1;
            }
            else if (*pPat == TAGFIELD_PATTERN_INT16)
            {
                u16 uHalf = (u16)uHex;
                memcpy(pBuf, &uHalf, sizeof(uHalf));
                pBuf += 2;
            }
        }
        else if ((*pPat == TAGFIELD_PATTERN_STR) && (iCnt > 0))
        {
            // at most iCnt-1 characters, zero-filled to iCnt
            s32 iIndex;
            for (iIndex = 0; ((*pData >= '0') || (*pData == '%')) && (iIndex + 1 < iCnt); ++iIndex)
            {
                if (*pData == '%')
                {
                    pBuf[iIndex] = (unsigned char)((hex_decode[pData[1]] << 4) | hex_decode[pData[2]]);
                    pData += 3;
                }
                else
                {
                    pBuf[iIndex] = *pData++;
                }
            }
            for (; iIndex < iCnt; ++iIndex)
            {
                pBuf[iIndex] = 0;
            }
            pBuf += iIndex;
            if (*pData == ',')
            {
                ++pData;
            }
        }
        else if (*pPat == 'a')
        {
            pBuf += (iCnt != 0) ? iCnt : 1;
        }

        if (pBuf >= pEnd)
        {
            break;
        }
        // "*" repeats the current element
        if (*++pPat == '*')
        {
            --pPat;
        }
    }

    iDecodeLen = (s32)(pBuf - static_cast<unsigned char*>(_pBuf));
    if (iDecodeLen < iLen)
    {
        memset(pBuf, 0, iLen - iDecodeLen);
    }
    return iDecodeLen;
}

extern "C" s32 TagFieldSetEpoch(char* pRecord, s32 iReclen, const char* pName, u32 uEpoch)
{
    s32 iLength;
    char strDate[20];
    struct tm* tm;
    struct tm tm2;
    unsigned char* pAppend;
    s32 iIndex;

    if (uEpoch == 0)
    {
        uEpoch = ds_timeinsecs();
    }
    if ((tm = ds_secstotime(&tm2, uEpoch)) == NULL)
    {
        return -1;
    }
    iLength = ds_snzprintf(strDate, sizeof(strDate), "%d.%d.%d-%d:%02d:%02d", tm->tm_year + 1900, tm->tm_mon + 1,
                           tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
    if ((pAppend = _TagFieldSetupAppend(reinterpret_cast<unsigned char*>(pRecord), iReclen, pName, iLength)) == NULL)
    {
        return -1;
    }
    for (iIndex = 0; (pAppend[iIndex] = (unsigned char)strDate[iIndex]) != 0; ++iIndex)
    {
    }
    pAppend += iLength;
    if ((g_divafter != 0) && (pName != NULL))
    {
        *pAppend++ = (unsigned char)g_divider;
    }
    *pAppend = 0;
    return (s32)(pAppend - reinterpret_cast<unsigned char*>(pRecord));
}

extern "C" u32 TagFieldGetEpoch(const char* pData, u32 uDefval)
{
    struct tm tm;
    u32 uDecimal;
    u32 uEpoch = 0;
    const char* pParse;

    if (pData != NULL)
    {
        if (*pData == '$')
        {
            // hex epoch (the digit lookup uses the signed character value)
            u32 uHex;
            for (pParse = pData + 1; (uHex = hex_decode[(signed char)*pParse]) < 16; ++pParse)
            {
                uEpoch = (uEpoch << 4) | uHex;
            }
        }
        else if (((signed char)*pData >= '0') && ((signed char)*pData <= '9'))
        {
            // plain decimal epoch ...
            uDecimal = 0;
            pParse = _ParseNumber(pData, &uDecimal, 256);
            if ((signed char)*pParse <= ' ')
            {
                uEpoch = uDecimal;
            }
            else
            {
                // ... or "Y.M.D-h:m:s", one separator character between the fields
                memset(&tm, 0, sizeof(tm));
                tm.tm_isdst = -1;
                pParse = _ParseNumber(pData, reinterpret_cast<u32*>(&tm.tm_year), 4);
                if ((((signed char)*pParse < '0') || ((signed char)*pParse > '9')) && (*pParse != 0))
                {
                    ++pParse;
                }
                pParse = _ParseNumber(pParse, reinterpret_cast<u32*>(&tm.tm_mon), 2);
                if ((((signed char)*pParse < '0') || ((signed char)*pParse > '9')) && (*pParse != 0))
                {
                    ++pParse;
                }
                pParse = _ParseNumber(pParse, reinterpret_cast<u32*>(&tm.tm_mday), 2);
                if ((((signed char)*pParse < '0') || ((signed char)*pParse > '9')) && (*pParse != 0))
                {
                    ++pParse;
                }
                pParse = _ParseNumber(pParse, reinterpret_cast<u32*>(&tm.tm_hour), 2);
                if ((((signed char)*pParse < '0') || ((signed char)*pParse > '9')) && (*pParse != 0))
                {
                    ++pParse;
                }
                pParse = _ParseNumber(pParse, reinterpret_cast<u32*>(&tm.tm_min), 2);
                if ((((signed char)*pParse < '0') || ((signed char)*pParse > '9')) && (*pParse != 0))
                {
                    ++pParse;
                }
                _ParseNumber(pParse, reinterpret_cast<u32*>(&tm.tm_sec), 2);

                if (((u32)(tm.tm_year - 1970) > 137) || ((u32)(tm.tm_mon - 1) > 11) || (tm.tm_mday < 1) || (tm.tm_mday > 31))
                {
                    tm.tm_year = 0;
                }
                if (((u32)tm.tm_hour > 23) || ((u32)tm.tm_min > 59) || (tm.tm_sec < 0) || (tm.tm_sec > 61))
                {
                    tm.tm_year = 0;
                }
                if (tm.tm_year != 0)
                {
                    tm.tm_mon -= 1;
                    tm.tm_year -= 1900;
                    uEpoch = ds_timetosecs(&tm);
                }
            }
        }
    }

    if (uEpoch == 0)
    {
        uEpoch = (uDefval != 0) ? uDefval : ds_timeinsecs();
    }
    return uEpoch;
}

extern "C" float TagFieldGetFloat(const char* pData, float fDefval)
{
    float fSign = 1.0f;
    s32 iInteger = 0;
    s32 iFraction = 0;
    s32 iDivisor = 1;

    if (pData == NULL)
    {
        return fDefval;
    }
    if (*pData == '+')
    {
        ++pData;
    }
    else if (*pData == '-')
    {
        fSign = -1.0f;
        ++pData;
    }
    for (; (*pData >= '0') && (*pData <= '9'); ++pData)
    {
        iInteger = (iInteger * 10) + (*pData & 15);
    }
    if (*pData == '.')
    {
        for (++pData; (*pData >= '0') && (*pData <= '9'); ++pData)
        {
            iFraction = (iFraction * 10) + (*pData & 15);
            iDivisor *= 10;
        }
    }
    return (((float)iFraction / (float)iDivisor) + (float)iInteger) * fSign;
}

extern "C" s32 TagFieldPrintf(char* _pRecord, s32 iLength, const char* pFormat, ...)
{
    s32 iCount = iLength;
    va_list args;
    unsigned char uCh;
    s32 iInitial = iLength;
    const char* pInput = pFormat;
    unsigned char* pRecord = reinterpret_cast<unsigned char*>(_pRecord);
    const char* pFind = "";

    va_start(args, pFormat);

    if (*pInput == '~')
    {
        // append: continue at the end of the existing record
        ++pInput;
        while ((iCount > 1) && (*pRecord != 0))
        {
            ++pRecord;
            --iCount;
        }
        if ((iCount > 1) && (iCount < iInitial) && (pRecord[-1] >= ' '))
        {
            *pRecord++ = (unsigned char)g_divider;
            --iCount;
        }
    }
    else if (*pInput == ',')
    {
        // continue the last field: the leading comma is kept only when it separates
        if (*pRecord == 0)
        {
            ++pInput;
        }
        else
        {
            while (iCount > 1)
            {
                ++pRecord;
                --iCount;
                if (*pRecord == 0)
                {
                    break;
                }
            }
            while ((iCount < iInitial) && (pRecord[-1] <= ' '))
            {
                ++iCount;
                --pRecord;
            }
            if ((pRecord == reinterpret_cast<unsigned char*>(_pRecord)) || ((iCount < iInitial) && (pRecord[-1] == '=')))
            {
                ++pInput;
            }
        }
    }

    for (uCh = (unsigned char)*pInput; uCh != 0; uCh = (unsigned char)*pInput)
    {
        ++pInput;
        if (iCount <= 1)
        {
            break;
        }

        if (uCh <= ' ')
        {
            // any run of whitespace becomes one divider
            *pRecord++ = (unsigned char)g_divider;
            --iCount;
            while ((*pInput != 0) && ((signed char)*pInput <= ' '))
            {
                ++pInput;
            }
        }
        else if (uCh == '#')
        {
            s32 iNum = va_arg(args, s32);
            s32 iLen = ds_snzprintf(reinterpret_cast<char*>(pRecord), iCount, "%d", iNum);
            iCount -= iLen;
            pRecord += iLen;
        }
        else if (uCh != '%')
        {
            *pRecord++ = uCh;
            --iCount;
        }
        else if (*pInput != 0)
        {
            s32 iLen = 0;
            uCh = (unsigned char)*pInput++;
            *pRecord = 0;
            if (uCh == 's')
            {
                const char* pStr = va_arg(args, const char*);
                iLen = TagFieldSetString(reinterpret_cast<char*>(pRecord), iCount, NULL, pStr);
            }
            else if (uCh == 'd')
            {
                s32 iNum = va_arg(args, s32);
                iLen = TagFieldSetNumber(reinterpret_cast<char*>(pRecord), iCount, NULL, iNum);
            }
            else if (uCh == 't')
            {
                s32 iToken = va_arg(args, s32);
                iLen = TagFieldSetToken(reinterpret_cast<char*>(pRecord), iCount, NULL, iToken);
            }
            else if (uCh == 'a')
            {
                u32 uAddr = va_arg(args, u32);
                iLen = TagFieldSetAddress(reinterpret_cast<char*>(pRecord), iCount, NULL, uAddr);
            }
            else if (uCh == 'f')
            {
                s32 iFlags = va_arg(args, s32);
                iLen = TagFieldSetFlags(reinterpret_cast<char*>(pRecord), iCount, NULL, iFlags);
            }
            else if (uCh == 'e')
            {
                u32 uEpoch = va_arg(args, u32);
                iLen = TagFieldSetEpoch(reinterpret_cast<char*>(pRecord), iCount, NULL, uEpoch);
            }
            else if (uCh == 'i')
            {
                pFind = va_arg(args, const char*);
                continue;
            }
            else if (uCh == 'x')
            {
                const char* pName = va_arg(args, const char*);
                iLen = TagFieldSetRaw(reinterpret_cast<char*>(pRecord), iCount, NULL, TagFieldFind(pFind, pName));
            }
            else if (uCh == 'r')
            {
                const char* pStr = va_arg(args, const char*);
                while ((*pStr != 0) && (iCount > 0))
                {
                    *pRecord++ = (unsigned char)*pStr++;
                    --iCount;
                }
            }
            if (iLen > 0)
            {
                iCount -= iLen;
                pRecord += iLen;
            }
        }
    }

    va_end(args);

    if ((g_divider == '\n') && (iInitial != iCount) && (iCount > 1))
    {
        *pRecord++ = (unsigned char)g_divider;
        --iCount;
    }
    if (iCount > 0)
    {
        *pRecord = 0;
    }
    return iInitial - iCount;
}

extern "C" s32 TagFieldGetDelim(const char* data, char* buffer, s32 buflen, const char* defval, s32 index, s32 delim)
{
    s32 len;
    unsigned char term;
    unsigned char stop;
    const unsigned char* ptr = reinterpret_cast<const unsigned char*>(data);

    // skip index elements
    if (ptr != NULL)
    {
        for (; index > 0; --index)
        {
            if (*ptr == '"')
            {
                term = ' ';
                stop = '"';
                ++ptr;
            }
            else
            {
                term = '!';
                stop = (unsigned char)delim;
            }
            while ((*ptr != stop) && (*ptr >= term))
            {
                ++ptr;
            }
            if ((*ptr == '"') && (stop == '"'))
            {
                ++ptr;
            }
            if ((signed char)*ptr != delim)
            {
                ptr = NULL;
                break;
            }
            ++ptr;
        }
    }

    // missing element: use the default
    if (ptr == NULL)
    {
        if (defval == NULL)
        {
            return -1;
        }
        if (buffer == NULL)
        {
            return (s32)strlen(defval);
        }
        for (len = 1; (len < buflen) && (*defval != 0); ++len)
        {
            *buffer++ = *defval++;
        }
        *buffer = 0;
        return len - 1;
    }

    if (*ptr == '"')
    {
        term = ' ';
        stop = '"';
        ++ptr;
    }
    else
    {
        term = '!';
        stop = (unsigned char)delim;
    }

    // no buffer: return the decoded length
    if (buffer == NULL)
    {
        for (len = 0; *ptr != stop; ++len, ++ptr)
        {
            unsigned char c = *ptr;
            if (c < term)
            {
                break;
            }
            if ((c == '%') && (ptr[1] >= ' ') && (ptr[2] >= ' '))
            {
                ptr += 2;
            }
        }
        return len;
    }

    if (buflen < 1)
    {
        return -1;
    }
    for (len = 1; len < buflen; ++len)
    {
        if ((*ptr == stop) || (*ptr < term))
        {
            break;
        }
        if ((*ptr == '%') && (ptr[1] == '%'))
        {
            *buffer++ = '%';
            ptr += 2;
        }
        else if ((*ptr == '%') && (ptr[1] >= ' ') && (ptr[2] >= ' '))
        {
            *buffer++ = (char)((hex_decode[ptr[1]] << 4) | hex_decode[ptr[2]]);
            ptr += 3;
        }
        else
        {
            *buffer++ = (char)*ptr++;
        }
    }
    *buffer = 0;
    return len - 1;
}
