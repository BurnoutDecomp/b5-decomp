"""Exercise the production new-save montage sequence and real GUI event emitters."""
import argparse
from pathlib import Path
import re
import sys
import tempfile

from fxgs_common import Tree, compile_and_run, definition

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--rev", help="Read state bodies from this revision for a negative control")
parser.add_argument("--output-dir", type=Path)
args = parser.parse_args()
temporary = tempfile.TemporaryDirectory(prefix="post_title_intro_") if args.output_dir is None else None
args.output_dir = (args.output_dir or Path(temporary.name)).resolve()
tree = Tree(args.rev)
source = tree.read("src/GameSource/Gui/Flow/HUD/States/BrnPostTitleScreenLoad.cpp")
alias = re.search(r"using MusicOnMenuStreamEvent = [^;]+;", source)
music = alias[0] if alias else definition(source, "struct MusicOnMenuStreamEvent") + ";"
generated = "\n\n".join([
    "namespace { enum { KI_EVENT_UNLOAD_OR_STOP=6, KI_EVENT_GUI_CACHE=64, KI_EVENT_VIDEO_FINISHED=510 };",
    definition(source, "struct GuiEventCache") + ";",
    "typedef CgsModule::VariableEventQueue<18432,16> StateInputQueue;",
    definition(source, "template <s32 N>\n        struct GuiCommandEvent16") + ";",
    definition(source, "template <s32 N>\n        void PostCommand16"),
    "const s32 KI_CHANNEL_GUI_OUT=40;", music,
    definition(source, "void CacheSetPostTitlePhase"),
    definition(source, "bool CacheShouldPlayIntroVideo"), "}",
    definition(source, "void PostTitleScreenLoad::HandleIncomingEvents"),
    definition(source, "void PostTitleScreenLoad::Update"),
])
result = compile_and_run(Path(__file__).with_name("PostTitleIntro.cpp"),
    "post_title_intro.inc", generated, "PostTitleIntro", output_dir=args.output_dir,
    extra_files={"CgsStrStream.cpp": tree.read("src/GameShared/GameClasses/Development/CgsStrStream.cpp")},
    extra_sources=[args.output_dir / "CgsStrStream.cpp"])
sys.exit(0 if result and result[1] == 0 else 1)
