"""Run complete graph/callback bodies with draw/report spies; --rev is the control."""
import argparse
import os
from pathlib import Path
import fxgs_common as common

parser = argparse.ArgumentParser()
parser.add_argument("--rev")
args = parser.parse_args()
os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
common.WORKFLOW = next(p for p in Path(__file__).resolve().parents
                       if (p / "tools/build/msvc_env.bat").is_file())
root = common.REPO
source = common.Tree(args.rev).read("src/GameShared/GameClasses/Development/PerfMon/DebugComponent/CgsDebugComponentPerfMonGpu.cpp")
constants = source[source.index("        const f32 KF_GRAPHSEGMENTSCALE"):
                   source.index("        // FLAG PC-platform leaf: opt-in witnesses")]
body = "namespace CgsDev { namespace {\n" + constants + "bool gbTraceGpuCallbacksPC=false;\n}\n"
body += common.definition(source, "void DebugComponentPerfMonGpu::RenderPerformanceGraph(") + "\n"
body += common.definition(source, "void DebugComponentPerfMonGpu::RenderGraphCallback(") + "\n}\n"
numeric = common.compile_and_run(Path(__file__).with_name("PCGpuMonitorGraph.cpp"),
    "pc_gpu_monitor_graph.inc", body, "PCGpuMonitorGraph",
    extra_flags=f'/I"{root / "src"}"', extra_sources=[
        root / "src/GameShared/GameClasses/Development/CgsStrStream.cpp",
        root / "src/GameShared/GameClasses/RenderWare/RwRGBA.cpp"])
raise SystemExit(common.report("run_pc_gpu_monitor_graph", [], numeric, 12))
