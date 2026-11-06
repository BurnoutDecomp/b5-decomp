from pathlib import Path


here = Path(__file__).resolve().parent
source = here.parent / "r85-apt-diagnostic-src/CgsAptAux.cpp"
body = source.read_text(encoding="utf-8")


def replace(anchor, replacement):
    global body
    assert body.count(anchor) == 1, anchor
    body = body.replace(anchor, replacement)


replace('#include <xtl.h>\n', '#include <xtl.h>\nstatic bool sbTraceNextAptUpdate = false;\n')
replace('        OutputDebugStringA("R85_APT_LOAD_END\\n");',
        '        sbTraceNextAptUpdate = true;\n        OutputDebugStringA("R85_APT_LOAD_END\\n");')
replace('''        CgsDev::PerfMonCpu::StartMonitor(BrnGui::GuiPerfmons::miAptAuxUpdateComponents);
        UpdateComponents();
        CgsDev::PerfMonCpu::StopMonitor(BrnGui::GuiPerfmons::miAptAuxUpdateComponents);

        CgsDev::PerfMonCpu::StartMonitor(BrnGui::GuiPerfmons::miAptAuxUpdateTarget);
        mAptMutex.Lock();
        AptUpdateTarget(mpAptTargetInstance, liDeltaMs, -1, 16);
        mAptMutex.Unlock();''', '''        const bool lbTrace = sbTraceNextAptUpdate;
        sbTraceNextAptUpdate = false;
        if (lbTrace)
            OutputDebugStringA("R86_APT_UPDATE_BEGIN\\n");
        CgsDev::PerfMonCpu::StartMonitor(BrnGui::GuiPerfmons::miAptAuxUpdateComponents);
        UpdateComponents();
        CgsDev::PerfMonCpu::StopMonitor(BrnGui::GuiPerfmons::miAptAuxUpdateComponents);
        if (lbTrace)
            OutputDebugStringA("R86_APT_COMPONENTS_END\\n");

        CgsDev::PerfMonCpu::StartMonitor(BrnGui::GuiPerfmons::miAptAuxUpdateTarget);
        mAptMutex.Lock();
        if (lbTrace)
            OutputDebugStringA("R86_APT_TARGET_BEGIN\\n");
        AptUpdateTarget(mpAptTargetInstance, liDeltaMs, -1, 16);
        if (lbTrace)
            OutputDebugStringA("R86_APT_TARGET_END\\n");
        mAptMutex.Unlock();''')
with (here / source.name).open("x", encoding="utf-8") as output:
    output.write(body)
