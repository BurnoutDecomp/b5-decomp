"""Whole Race Car debug Update with an INI-origin loading/attachment witness."""
import argparse
from pathlib import Path
import fxgs_common as common

parser = argparse.ArgumentParser()
parser.add_argument('--rev')
args = parser.parse_args()
repo = Path(__file__).resolve().parents[1]
common.WORKFLOW = next(p for p in repo.parents if (p / 'tools/build/msvc_flags.txt').exists())
name = 'src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleDebugComponent.cpp'
current = common.Tree().read(name)
update = common.Tree(args.rev).read(name)
inc = 'namespace BrnWorld { namespace {\n'
inc += common.definition(current, 'bool IniPlayerUnavailablePC(')
inc += '\n}\n' + common.definition(update, 'void RaceCarEntityModuleDebugComponent::Update(') + '\n}\n'
result = common.compile_and_run(repo / 'tests/PCDebugIniStartup.cpp',
    'IniStartup.inc', inc, 'PCDebugIniStartup', extra_flags=f'/I"{repo / "src"}"')
raise SystemExit(result is None or bool(result[1]))
