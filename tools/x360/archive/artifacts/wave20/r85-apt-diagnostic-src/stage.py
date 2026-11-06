from pathlib import Path


here = Path(__file__).resolve().parent
root = Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")


def replace(body, anchor, replacement):
    assert body.count(anchor) == 1, anchor
    return body.replace(anchor, replacement)


preload = here.parent / "r84-preload-detail-diagnostic-src/BrnBootPreload.cpp"
body = preload.read_text(encoding="utf-8")
body = replace(body, '''            OutputDebugStringA("R84_PRELOAD_PLAY_END\\n");
            PreloadResourcesReady(mpGuiCache, KA_PRELOAD_FLAPTHUD, 1);
            meUpdateStage = E_PRELOAD_LOADING_SCREEN;
            lpInQueue->Clear();
            return;''', '''            OutputDebugStringA("R84_PRELOAD_PLAY_END\\n");
            OutputDebugStringA("R85_FLAPT_PREFETCH_BEGIN\\n");
            PreloadResourcesReady(mpGuiCache, KA_PRELOAD_FLAPTHUD, 1);
            OutputDebugStringA("R85_FLAPT_PREFETCH_END\\n");
            meUpdateStage = E_PRELOAD_LOADING_SCREEN;
            lpInQueue->Clear();
            OutputDebugStringA("R85_PRELOAD_RETURN\\n");
            return;''')
with (here / preload.name).open("x", encoding="utf-8") as output:
    output.write(body)

apt = root / "src/GameShared/GameClasses/Gui/View/AptInterface/CgsAptAux.cpp"
body = "#include <xtl.h>\n" + apt.read_text(encoding="utf-8")
anchor = '        AptLoadAnimation(lpacFileName, lacTargetPath);'
body = replace(body, anchor, '''        char lacProbe[136];
        CgsCore::SPrintf(lacProbe, sizeof(lacProbe), "R85_APT_LOAD_BEGIN file=%s level=%d\\n",
                        lpacFileName, liTargetLevel);
        OutputDebugStringA(lacProbe);
''' + anchor + '''
        OutputDebugStringA("R85_APT_LOAD_END\\n");''')
with (here / apt.name).open("x", encoding="utf-8") as output:
    output.write(body)
