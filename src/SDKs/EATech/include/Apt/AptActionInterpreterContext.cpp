// Native ActionScript lookup context and leaf-name parsing.

#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptAnimationTarget.h"
#include "SDKs/EATech/include/Apt/AptCIH.h"
#include "SDKs/EATech/include/Apt/AptString/EAString.h"
#include "SDKs/EATech/include/Apt/AptTarget.h"

#include <cstring>

namespace
{
    // The original parser uses one 256-byte segment and one 256-byte leaf buffer.
    const unsigned int KU_PathBufferLength = 256;
}

bool AptActionInterpreter::getContext(AptValue* pScope, AptValue* pTarget, const EAStringC* pPath,
                                      AptValue** ppOutContext, EAStringC& rOutName)
{
    const char* pBuffer = pPath->GetBuffer();
    char bSimple = 1;
    int c = static_cast<signed char>(*pBuffer++);
    if (c)
    {
        while (c >= '0' && c != ':')
        {
            c = static_cast<signed char>(*pBuffer++);
            if (!c)
                goto done_scan;
        }
        bSimple = 0;
    }
done_scan:
    if (bSimple && !pTarget)
    {
        rOutName = *pPath;
        *ppOutContext = pScope;
        return false;
    }

    char acName[KU_PathBufferLength];
    const bool bRooted = getContext(pScope, pTarget, pPath, ppOutContext, acName);
    rOutName = acName;
    return bRooted;
}

bool AptActionInterpreter::getContext(AptValue* pScope, AptValue* pTarget, const EAStringC* pPath,
                                      AptValue** ppOutContext, char* pNameBuffer)
{
    const char* pCurrent = pPath->GetBuffer();
    AptValue* pContext = pScope;
    bool bRooted = false;
    pNameBuffer[0] = '\0';
    if (*pCurrent == '/')
    {
        pContext = gpAptTarget->mpAnimationTarget->mDisplayList.mpHead->mpFirst;
        ++pCurrent;
        *ppOutContext = pContext;
        bRooted = true;
    }
    else
    {
        *ppOutContext = pScope;
    }

    char acSegment[KU_PathBufferLength];
    char* pSegmentEnd = acSegment;
    for (;;)
    {
        switch (*pCurrent)
        {
        case '.':
            if (pCurrent[1] == '.')
            {
                *pSegmentEnd++ = *pCurrent++;
                *pSegmentEnd++ = *pCurrent++;
                continue;
            }
            else if (pCurrent[1] == '\0')
            {
                *ppOutContext = 0;
                return bRooted;
            }
            *pSegmentEnd = '\0';
            {
                EAStringC lSegment(acSegment);
                pContext = pContext->findChild(&lSegment, pTarget);
            }
            pTarget = 0;
            if (!pContext)
            {
                *ppOutContext = 0;
                return bRooted;
            }
            ++pCurrent;
            pSegmentEnd = acSegment;
            break;
        case ':':
        {
            *pSegmentEnd = '\0';
            AptValue* pNewContext;
            {
                EAStringC lSegment(acSegment);
                pNewContext = pContext->findChild(&lSegment, pTarget);
            }
            pTarget = 0;
            if (pNewContext)
            {
                *ppOutContext = pNewContext;
                strcpy(pNameBuffer, pCurrent + 1);
                return bRooted;
            }
            ++pCurrent;
            pSegmentEnd = acSegment;
            break;
        }
        case '\0':
            *pSegmentEnd = '\0';
            *ppOutContext = pTarget ? pTarget : pContext;
            strcpy(pNameBuffer, acSegment);
            return bRooted;
        default:
            *pSegmentEnd++ = *pCurrent++;
            break;
        }
    }
}
