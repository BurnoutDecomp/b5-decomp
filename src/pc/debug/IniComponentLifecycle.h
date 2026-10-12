#pragma once

// FLAG PC-platform leaf: remember which lazy pages the startup INI activated.
// This sidecar adds no fields to recovered engine objects and owns no values.
#include <algorithm>
#include <mutex>
#include <vector>

namespace CgsPC::Debug
{
    struct IniComponentLifecycle
    {
        struct Entry { const void* component; const void* menu; };
        std::mutex mutex;
        std::vector<Entry> components;
    };

    inline IniComponentLifecycle& GetIniComponentLifecycle()
    {
        static IniComponentLifecycle state;
        return state;
    }

    inline void ResetIniComponents()
    {
        auto& state = GetIniComponentLifecycle();
        std::lock_guard<std::mutex> lock(state.mutex);
        state.components.clear();
    }

    inline void SetIniComponent(const void* component, bool enabled)
    {
        auto& state = GetIniComponentLifecycle();
        std::lock_guard<std::mutex> lock(state.mutex);
        auto entry = std::find_if(state.components.begin(), state.components.end(),
            [&](const IniComponentLifecycle::Entry& value) { return value.component == component; });
        if (enabled && entry == state.components.end()) state.components.push_back({component,nullptr});
        else if (!enabled && entry != state.components.end()) state.components.erase(entry);
    }

    inline bool IsIniComponent(const void* component)
    {
        auto& state = GetIniComponentLifecycle();
        std::lock_guard<std::mutex> lock(state.mutex);
        return std::any_of(state.components.begin(), state.components.end(),
            [&](const IniComponentLifecycle::Entry& value) { return value.component == component; });
    }

    inline void BindIniComponentMenu(const void* component, const void* menu)
    {
        auto& state = GetIniComponentLifecycle();
        std::lock_guard<std::mutex> lock(state.mutex);
        for (auto& entry : state.components)
            if (entry.component == component) entry.menu = menu;
    }

    inline void OpenIniComponentMenu(const void* menu)
    {
        auto& state = GetIniComponentLifecycle();
        std::lock_guard<std::mutex> lock(state.mutex);
        state.components.erase(std::remove_if(state.components.begin(), state.components.end(),
            [&](const IniComponentLifecycle::Entry& value) { return value.menu == menu; }),
            state.components.end());
    }
}
