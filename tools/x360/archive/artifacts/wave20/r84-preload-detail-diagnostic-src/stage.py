from pathlib import Path


here = Path(__file__).resolve().parent
source = here.parent / "r83-preload-diagnostic-src/BrnBootPreload.cpp"
body = source.read_text(encoding="utf-8")


def replace(anchor, replacement):
    global body
    assert body.count(anchor) == 1, anchor
    body = body.replace(anchor, replacement)


replace('''        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            if (liEventId == KI_EVENT_GUI_CACHE)''', '''        if (suUpdates == 1)
            OutputDebugStringA("R84_PRELOAD_FIRST_BEGIN\\n");
        s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
        if (suUpdates == 1)
        {
            char lacProbe[128];
            CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                "R84_PRELOAD_FIRST_END id=%d event=%08X size=%d\\n",
                liEventId, reinterpret_cast<u32>(lpEvent), liSize);
            OutputDebugStringA(lacProbe);
        }
        u32 luEvents = 0;
        for (; lpEvent != 0; liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            ++luEvents;
            if (suUpdates == 1 && (luEvents <= 4 || luEvents == 1024))
            {
                char lacProbe[136];
                CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                    "R84_PRELOAD_EVENT n=%u id=%d event=%08X size=%d\\n",
                    luEvents, liEventId, reinterpret_cast<u32>(lpEvent), liSize);
                OutputDebugStringA(lacProbe);
            }
            if (liEventId == KI_EVENT_GUI_CACHE)''')
replace('''        switch (meUpdateStage)
        {
        case E_PRELOAD_WAIT_CACHE:''', '''        if (suUpdates <= 4 || (suUpdates & 255u) == 0)
        {
            char lacProbe[136];
            CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                "R84_PRELOAD_DRAIN update=%u count=%u cache=%08X stage=%d\\n",
                suUpdates, luEvents, reinterpret_cast<u32>(mpGuiCache), meUpdateStage);
            OutputDebugStringA(lacProbe);
        }
        switch (meUpdateStage)
        {
        case E_PRELOAD_WAIT_CACHE:
        {''')
replace('''            if (!PreloadResourcesReady(mpGuiCache, KA_PRELOAD_MAIN, 1) ||
                !PreloadFontsReady(mpGuiCache))
                break;
            mpStateInterface->PlayAptMovie("main", 0);''', '''            if (suUpdates <= 4)
                OutputDebugStringA("R84_PRELOAD_MAIN_BEGIN\\n");
            const bool lbMainReady = PreloadResourcesReady(mpGuiCache, KA_PRELOAD_MAIN, 1);
            if (suUpdates <= 4 && lbMainReady)
                OutputDebugStringA("R84_PRELOAD_FONTS_BEGIN\\n");
            const bool lbFontsReady = lbMainReady && PreloadFontsReady(mpGuiCache);
            if (suUpdates <= 4 || (suUpdates & 255u) == 0)
            {
                char lacProbe[144];
                CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                    "R84_PRELOAD_GATES update=%u main=%d fonts=%d\\n",
                    suUpdates, static_cast<s32>(lbMainReady), static_cast<s32>(lbFontsReady));
                OutputDebugStringA(lacProbe);
            }
            if (!lbFontsReady)
                break;
            OutputDebugStringA("R84_PRELOAD_PLAY_BEGIN\\n");
            mpStateInterface->PlayAptMovie("main", 0);
            OutputDebugStringA("R84_PRELOAD_PLAY_END\\n");''')
replace('''            return;

        case E_PRELOAD_LOADING_SCREEN:''', '''            return;
        }

        case E_PRELOAD_LOADING_SCREEN:''')
replace('''        lpInQueue->Clear();
    }
}
''', '''        lpInQueue->Clear();
        if (suUpdates <= 4)
            OutputDebugStringA("R84_PRELOAD_EXIT\\n");
    }
}
''')
with (here / source.name).open("x", encoding="utf-8") as output:
    output.write(body)
