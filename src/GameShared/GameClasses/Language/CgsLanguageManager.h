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
        // ADDITIVE GROW (GuiFlow-consumers group): the localised-value format selector
        // (DWARF: CgsLanguageManager.h:84 `enum ParameterFormatType`). The GUI text
        // fields pass it to LanguageManager::FormatParameter / TextField::SetLocalisedText
        // to pick how a value renders (raw text, a clock format, an id-table lookup, a
        // distance, ...). E_FORMAT_ID_LOOKUP (9) is the "look this string up in the
        // localisation database by id" mode the drive-thru panel uses for its
        // "DT_NAME_%llu" / "DT_LOC_%llu" keys. Values are X360-attested via the
        // SetLocalisedText overloads that take this type.
        enum ParameterFormatType
        {
            E_FORMAT_TEXT                       = 0,
            E_FORMAT_HOURS_MINUTES_SECONDS      = 1,
            E_FORMAT_MINUTES_SECONDS_HUNDREDTHS = 2,
            E_FORMAT_MINUTES_SECONDS            = 3,
            E_FORMAT_SECONDS_HUNDREDTHS         = 4,
            E_FORMAT_SECONDS_HUNDREDTHS_LONG    = 5,
            E_FORMAT_SECONDS                    = 6,
            E_FORMAT_SECONDS_LONG               = 7,
            E_FORMAT_MINUTES_SECONDS_MID_TEXT   = 8,
            E_FORMAT_ID_LOOKUP                  = 9,
            E_FORMAT_ID_LOOKUP_TOUPPER          = 10,
            E_FORMAT_INTEGER                    = 11,
            E_FORMAT_INTEGER_NOSEPERATOR        = 12,
            E_FORMAT_PERCENTAGE                 = 13,
            E_FORMAT_MONEY                      = 14,
            E_FORMAT_AUTO_DISTANCE              = 15,
            E_FORMAT_AUTO_DISTANCE_LONG         = 16,
            E_FORMAT_SMALL_DISTANCE            = 17,
            E_FORMAT_SMALL_DISTANCE_LONG        = 18,
            E_FORMAT_LARGE_DISTANCE             = 19,
            E_FORMAT_LARGE_DISTANCE_LONG        = 20,
            E_FORMAT_COUNT                      = 21,
        };

        void SetUseMetricUnits(bool lbUseMetric);
        bool IsUsingMetricUnits() const;

        // ADDITIVE GROW (GUI text consumers): look up a localised string by its database key
        // (e.g. "CREDITS_TITLE_0"). Returns the UTF-8 string, or NULL when the key is not in
        // the localisation database. The X360 emits it out-of-line; callers (e.g. the credits
        // renderer's RecalculateParagraphs) pass a SPrintf'd key and store the result as a
        // CgsResource::CgsUtf8* to feed the text path. Body links from the CgsLanguageManager
        // TU. (DWARF buffer type is CgsUnicode::CgsUtf8* == u8; returned as that here.)
        const u8* FindString(const char* lpcKey) const;

        // The active language id. The X360 reads it as the manager's leading field
        // (the InGameMessageRenderer compares it against 16 -- a wide-glyph language --
        // to nudge the on-screen message Y-position). Exposed as a named accessor so
        // callers read it by name rather than poking offset 0. Body links from the
        // CgsLanguageManager TU. (CgsLanguage::ELanguage modelled as s32; 0 = English.)
        s32 GetCurrentLanguage() const;

        // Localised value -> string formatters. The overlapping signatures (target buffer, value(s),
        // buffer size) are grounded in the DWARF; the X360 ARTIST build adds the XoverY / Date /
        // *AndHundreds variants.
        // The DWARF types the buffer as CgsUnicode::CgsUtf8* (== u8); modelled here as char*
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
