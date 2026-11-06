from pathlib import Path


here = Path(__file__).resolve().parent
source = here.parent / "r94-apt-action-diagnostic-src/AptActionRun.cpp"
body = source.read_text(encoding="utf-8")


def replace(anchor, replacement):
    global body
    assert body.count(anchor) == 1, anchor
    body = body.replace(anchor, replacement)


replace("gbTraceAptRuntime && suProbeRuns < 6;", "gbTraceAptRuntime && suProbeRuns < 64;")
replace(
    "            const bool lbDispatch = lbProbe && luDispatch++ < 40;",
    """            if (lbProbe)
                ++luDispatch;
            const bool lbDispatch = lbProbe && (luDispatch <= 128 ||
                (luDispatch & (luDispatch - 1)) == 0);""",
)
with (here / source.name).open("x", encoding="utf-8") as output:
    output.write(body)
