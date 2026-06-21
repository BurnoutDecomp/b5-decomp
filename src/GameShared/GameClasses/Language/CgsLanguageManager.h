#pragma once

#include "types.hpp"

// CgsLanguage::LanguageManager - localisation/units manager. Only the metric-units
// query needed by the in-scope GUI code is declared here; the full manager (string
// tables, distance/speed formatting, etc.) is a large out-of-scope object. Members
// and the metric flag are from the DecFIGS DWARF (CgsLanguageManager.h).
namespace CgsLanguage
{
    class LanguageManager
    {
    public:
        void SetUseMetricUnits(bool lbUseMetric);
        bool IsUsingMetricUnits() const;

        // Localised value -> string formatters. Signatures for the overlapping ones are from the
        // Feb-2007 partial source CgsLanguageManager.h (target buffer, value(s), buffer size); the X360
        // ARTIST build adds the XoverY / Date / *AndHundreds variants (version drift over the leak).
        // The leaked source types the buffer as CgsUnicode::CgsUtf8* (== u8); modelled here as char*
        // since callers (e.g. the debug HUD) pass a plain byte buffer straight to the text renderer.
        // Bodies link from the CgsLanguageManager TU.
        void FormatIntegerString(char* lpcTarget, s32 liValue, s32 liTargetSize) const;
        void FormatXoverYString(char* lpcTarget, s32 liX, s32 liY, s32 liTargetSize) const;
        void FormatPercentageString(char* lpcTarget, s32 liValue, s32 liTargetSize) const;
        void FormatCurrencyString(char* lpcTarget, s32 liCurrencyValue, s32 liTargetSize) const;
        void FormatDateString(char* lpcTarget, s32 liDays, s32 liMonths, s32 liYears, s32 liTargetSize) const;

        void FormatHoursMinutesAndSecondsString(char* lpcTarget, f32 lfTimeInSeconds, s32 liTargetSize) const;
        void FormatMinutesAndSecondsString(char* lpcTarget, f32 lfTimeInSeconds, s32 liTargetSize) const;
        void FormatMinutesAndSecondsAndHundredsString(char* lpcTarget, f32 lfTimeInSeconds, s32 liTargetSize) const;
        void FormatSecondsAndHundredsString(char* lpcTarget, f32 lfTimeInSeconds, s32 liTargetSize) const;
        void FormatSecondsString(char* lpcTarget, f32 lfTimeInSeconds, s32 liTargetSize) const;

        void FormatSmallDistanceString(char* lpcTarget, f32 lfMetres, s32 liTargetSize) const;
        void FormatLargeDistanceString(char* lpcTarget, f32 lfMetres, s32 liTargetSize) const;

        // The metres -> display-unit scale the HUD multiplies a distance by to decide whether the
        // small or large distance string reads better (X360 reads it as a float member at +0x60F8;
        // exposed as a named accessor so callers don't poke the raw offset).
        f32 GetDistanceDisplayScale() const;
    };
}
