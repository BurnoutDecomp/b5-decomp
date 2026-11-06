from pathlib import Path


here = Path(__file__).resolve().parent
prior = here.parent / "r103-title-apt-diagnostic-src/CgsAptAux.cpp"
body = prior.read_text(encoding="utf-8")
anchor = """        CGS_ASSERT(lpAptAux->mpAptCommunicator != 0,
                   "Invalid pointer to apt communicator in AptAux::UpdateFlashComponent");"""
assert body.count(anchor) == 1, anchor
body = body.replace(
    anchor,
    """        static unsigned int suTitleViewUpdates = 0;
        if (sbTraceTitleRuntime && suTitleViewUpdates++ < 20)
        {
            char lacProbe[210];
            CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                "R107_TITLE_VIEW aux=%08X live=%08X communicator=%08X name=%.48s key=%.28s value=%.28s\\n",
                reinterpret_cast<u32>(lpAptAux),
                reinterpret_cast<u32>(AptAuxPointer::mpAptAuxInst),
                lpAptAux ? reinterpret_cast<u32>(lpAptAux->mpAptCommunicator) : 0u,
                lpacAptName, lpacViewState, lpacParam);
            OutputDebugStringA(lacProbe);
        }
""" + anchor,
)
with (here / prior.name).open("x", encoding="utf-8") as output:
    output.write(body)
