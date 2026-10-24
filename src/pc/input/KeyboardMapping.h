#pragma once

#include <SDL3/SDL_gamepad.h>
#include <algorithm>
#include <bitset>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

// FLAG PC-platform leaf: optional controller chords translated to game-local
// Windows virtual keys. This does not alter the recovered console action map.
namespace CgsInput::PCGamepad
{
    class KeyboardMapping
    {
    public:
        using Keys = std::bitset<256>;

        unsigned LoadSection(const char* lpSection)
        {
            mBindings.clear();
            mKeys.reset();
            muConsumed = 0;
            unsigned luInvalid = 0;
            for (const char* lpRow = lpSection; lpRow && *lpRow; lpRow += std::char_traits<char>::length(lpRow) + 1)
            {
                const std::string lRow(lpRow);
                const auto luEqual = lRow.find('=');
                Binding lBinding{};
                if (luEqual == std::string::npos || mBindings.size() >= 64 ||
                    !ParseButtons(lRow.substr(0, luEqual), lBinding.muButtons) ||
                    !ParseKeys(lRow.substr(luEqual + 1), lBinding.mKeys))
                {
                    ++luInvalid;
                    continue;
                }
                mBindings.push_back(lBinding);
            }
            // A complete chord wins over an overlapping single button. Stable
            // order makes equally specific overlapping bindings deterministic.
            std::stable_sort(mBindings.begin(), mBindings.end(), [](const Binding& a, const Binding& b) {
                return Count(a.muButtons) > Count(b.muButtons);
            });
            return luInvalid;
        }

        void Update(std::uint64_t luButtons, bool lbAllowed)
        {
            mKeys.reset();
            muConsumed = 0;
            if (!lbAllowed) return;
            for (const Binding& lrBinding : mBindings)
            {
                if ((luButtons & lrBinding.muButtons) == lrBinding.muButtons &&
                    (muConsumed & lrBinding.muButtons) == 0)
                {
                    mKeys |= lrBinding.mKeys;
                    muConsumed |= lrBinding.muButtons;
                }
            }
        }

        const Keys& GetKeys() const { return mKeys; }
        std::uint64_t GetConsumedButtons() const { return muConsumed; }
        bool HasBindings() const { return !mBindings.empty(); }

    private:
        struct Binding
        {
            std::uint64_t muButtons = 0;
            Keys mKeys;
        };
        std::vector<Binding> mBindings;
        Keys mKeys;
        std::uint64_t muConsumed = 0;

        static unsigned Count(std::uint64_t luMask)
        {
            unsigned luCount = 0;
            for (; luMask; luMask &= luMask - 1) ++luCount;
            return luCount;
        }

        static std::string Normalize(const std::string& lrName)
        {
            std::string lName;
            for (unsigned char lcChar : lrName)
                if (!std::isspace(lcChar) && lcChar != '_' && lcChar != '-')
                    lName += static_cast<char>(std::tolower(lcChar));
            return lName;
        }

        template<class Parser>
        static bool ParseChord(const std::string& lrChord, Parser lParser)
        {
            std::size_t luStart = 0;
            do
            {
                const auto luEnd = lrChord.find('+', luStart);
                const auto lPart = Normalize(lrChord.substr(luStart, luEnd - luStart));
                if (lPart.empty() || !lParser(lPart)) return false;
                if (luEnd == std::string::npos) return true;
                luStart = luEnd + 1;
            } while (luStart <= lrChord.size());
            return false;
        }

        static bool ParseButtons(const std::string& lrChord, std::uint64_t& lrMask)
        {
            struct Alias { const char* mpcName; SDL_GamepadButton meButton; };
            static const Alias KAA_ALIASES[] = {
                {"r4", SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1}, {"l4", SDL_GAMEPAD_BUTTON_LEFT_PADDLE1},
                {"r5", SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2}, {"l5", SDL_GAMEPAD_BUTTON_LEFT_PADDLE2},
                {"rightpaddle1", SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1}, {"leftpaddle1", SDL_GAMEPAD_BUTTON_LEFT_PADDLE1},
                {"rightpaddle2", SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2}, {"leftpaddle2", SDL_GAMEPAD_BUTTON_LEFT_PADDLE2},
                {"a", SDL_GAMEPAD_BUTTON_SOUTH}, {"cross", SDL_GAMEPAD_BUTTON_SOUTH},
                {"south", SDL_GAMEPAD_BUTTON_SOUTH}, {"east", SDL_GAMEPAD_BUTTON_EAST},
                {"west", SDL_GAMEPAD_BUTTON_WEST}, {"north", SDL_GAMEPAD_BUTTON_NORTH},
                {"b", SDL_GAMEPAD_BUTTON_EAST}, {"circle", SDL_GAMEPAD_BUTTON_EAST},
                {"x", SDL_GAMEPAD_BUTTON_WEST}, {"square", SDL_GAMEPAD_BUTTON_WEST},
                {"y", SDL_GAMEPAD_BUTTON_NORTH}, {"triangle", SDL_GAMEPAD_BUTTON_NORTH},
                {"lb", SDL_GAMEPAD_BUTTON_LEFT_SHOULDER}, {"l1", SDL_GAMEPAD_BUTTON_LEFT_SHOULDER},
                {"rb", SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER}, {"r1", SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER},
                {"ls", SDL_GAMEPAD_BUTTON_LEFT_STICK}, {"rs", SDL_GAMEPAD_BUTTON_RIGHT_STICK},
                {"select", SDL_GAMEPAD_BUTTON_BACK}, {"menu", SDL_GAMEPAD_BUTTON_START},
                {"dpadup", SDL_GAMEPAD_BUTTON_DPAD_UP}, {"dpaddown", SDL_GAMEPAD_BUTTON_DPAD_DOWN},
                {"dpadleft", SDL_GAMEPAD_BUTTON_DPAD_LEFT}, {"dpadright", SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
            };
            return ParseChord(lrChord, [&](const std::string& lrName) {
                SDL_GamepadButton leButton = SDL_GAMEPAD_BUTTON_INVALID;
                for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; ++i)
                {
                    const auto leCandidate = static_cast<SDL_GamepadButton>(i);
                    const char* lpcName = SDL_GetGamepadStringForButton(leCandidate);
                    if (lpcName && lrName == Normalize(lpcName)) { leButton = leCandidate; break; }
                }
                for (const Alias& lrAlias : KAA_ALIASES)
                    if (lrName == lrAlias.mpcName) { leButton = lrAlias.meButton; break; }
                if (leButton < 0 || leButton >= SDL_GAMEPAD_BUTTON_COUNT) return false;
                const auto luBit = std::uint64_t{1} << leButton;
                if (lrMask & luBit) return false;
                lrMask |= luBit;
                return true;
            });
        }

        static bool ParseKeys(const std::string& lrChord, Keys& lrKeys)
        {
            struct Alias { const char* mpcName; unsigned muKey; };
            static const Alias KAA_KEYS[] = {
                {"end", 0x23}, {"home", 0x24}, {"insert", 0x2d}, {"delete", 0x2e},
                {"enter", 0x0d}, {"return", 0x0d}, {"escape", 0x1b}, {"esc", 0x1b},
                {"space", 0x20}, {"tab", 0x09}, {"backspace", 0x08},
                {"pageup", 0x21}, {"pagedown", 0x22},
                {"left", 0x25}, {"up", 0x26}, {"right", 0x27}, {"down", 0x28},
                {"shift", 0x10}, {"ctrl", 0x11}, {"control", 0x11}, {"alt", 0x12},
                {"capslock", 0x14}, {"pause", 0x13}, {"printscreen", 0x2c},
                {"numlock", 0x90}, {"scrolllock", 0x91},
            };
            return ParseChord(lrChord, [&](std::string lName) {
                if (lName.compare(0, 2, "vk") == 0) lName.erase(0, 2);
                if (lName.empty()) return false;
                unsigned luKey = 0;
                if (lName.size() == 1 && std::isalnum(static_cast<unsigned char>(lName[0])))
                    luKey = static_cast<unsigned char>(std::toupper(static_cast<unsigned char>(lName[0])));
                else if ((lName[0] == 'f' && lName.size() > 1) || lName.compare(0, 2, "0x") == 0)
                {
                    char* lpEnd = nullptr;
                    const bool lbFunction = lName[0] == 'f';
                    const unsigned long luValue = std::strtoul(lName.c_str() + (lbFunction ? 1 : 2), &lpEnd, lbFunction ? 10 : 16);
                    if (*lpEnd || lpEnd == lName.c_str() + (lbFunction ? 1 : 2)) return false;
                    if (lbFunction) { if (luValue < 1 || luValue > 24) return false; luKey = 0x6f + luValue; }
                    else { if (luValue < 1 || luValue > 255) return false; luKey = static_cast<unsigned>(luValue); }
                }
                else
                    for (const Alias& lrAlias : KAA_KEYS)
                        if (lName == lrAlias.mpcName) { luKey = lrAlias.muKey; break; }
                if (!luKey || lrKeys.test(luKey)) return false;
                lrKeys.set(luKey);
                return true;
            });
        }
    };
}
