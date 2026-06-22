// Embed-check: instantiate CgsSystem::DateAndTime by value and exercise the bodies so the
// gate links the whole TU. Not shipped; lives next to the home.
#include "GameShared/GameClasses/System/Timer/PS3/CgsDateAndTimePS3.h"

int CgsDateAndTimePS3_EmbedCheck()
{
    CgsSystem::DateAndTime lNow;
    lNow.SetLocal(true);
    lNow.Update();

    CgsSystem::DateAndTime lThen;
    lThen.SetLocal(false);
    lThen.Update();

    lNow.SetYear(2026);
    lNow.SetMonth(6);
    lNow.SetDay(22);
    lNow.SetHour(13);
    lNow.SetMinute(45);
    lNow.SetSecond(30);

    s32 liSum = lNow.GetYear() + lNow.GetMonth() + lNow.GetDay()
              + lNow.GetHour() + lNow.GetMinute() + lNow.GetSecond();

    liSum += lNow.GetDifferenceInSeconds(lThen);
    liSum += static_cast<s32>(lNow.GetRawTimeValue() & 0xFF);
    liSum += lNow.IsLocal() ? 1 : 0;
    liSum += lNow.IsZero() ? 1 : 0;

    if (lNow == lThen) liSum += 1;
    if (lNow < lThen)  liSum += 2;
    if (lNow <= lThen) liSum += 4;
    if (lNow > lThen)  liSum += 8;
    if (lNow >= lThen) liSum += 16;

    lNow.Clear();
    return liSum;
}
