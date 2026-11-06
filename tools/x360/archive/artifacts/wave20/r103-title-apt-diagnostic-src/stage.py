from pathlib import Path


here = Path(__file__).resolve().parent
source = here.parent / "r91-apt-preload-diagnostic-src/CgsAptAux.cpp"
body = source.read_text(encoding="utf-8")


def replace(anchor, replacement):
    global body
    assert body.count(anchor) == 1, anchor
    body = body.replace(anchor, replacement)


replace("#include <xtl.h>\n", "#include <xtl.h>\n#include <cstring>\n")
replace(
    "static unsigned int suTraceAptUpdates = 0;",
    "static unsigned int suTraceAptUpdates = 0;\nstatic bool sbTraceTitleRuntime = false;",
)
replace(
    "        suTraceAptUpdates = 8;",
    """        suTraceAptUpdates = 8;
        sbTraceTitleRuntime = std::strcmp(lpacFileName, "Title_Screen02") == 0;""",
)
replace("        gbTraceAptRuntime = lbTrace;", "        gbTraceAptRuntime = lbTrace && sbTraceTitleRuntime;")
with (here / source.name).open("x", encoding="utf-8") as output:
    output.write(body)
