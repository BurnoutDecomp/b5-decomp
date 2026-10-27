"""Observe prop/world circles through extracted production render bodies."""
from pathlib import Path
import argparse, json, os, re, sys
from fxgs_common import Tree, definition, compile_and_run, report
os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
sys.dont_write_bytecode = True
args=argparse.ArgumentParser()
args.add_argument('--rev')
args.add_argument('--prop-source', type=Path, help='explicit production prop-render source file')
args.add_argument('--world-source', type=Path, help='explicit production world-render source file')
args.add_argument('--output-dir', type=Path, help='new or empty retained fxgs_common receipt directory; omitted preserves the existing temporary-output mode')
args.add_argument('--expect-circle-regression', action='store_true', help='require exact-base legacy pass plus new circle-only failures')
options=args.parse_args()
here=Path(__file__).resolve().parent
tree=Tree(options.rev)
def read_source(explicit, relative):
    return explicit.read_text(encoding='utf-8-sig') if explicit else tree.read(relative)
prop=read_source(options.prop_source, 'src/GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModule_Render.cpp')
world=read_source(options.world_source, 'src/GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModule.cpp')
def scalar(source, kind, name):
    match=re.search(r'(?m)^static\s+'+re.escape(kind)+r'\s+'+re.escape(name)+r'\s*=\s*[^;]+;', source)
    return match.group(0) if match else ''
track=scalar(world,'s32','siCurrentTrackUnitIndex')
world_flag=scalar(world,'bool','sbDrawInstanceCircles')
prop_flag=scalar(prop,'bool','sbDrawPropCircles')
prop_distance=scalar(prop,'f32','sfPropCircleDrawDistance')
if not track or not world_flag:
    raise SystemExit('Required production world debug globals were not found.')
has_prop_circle_data=bool(prop_flag and prop_distance)
if bool(prop_flag) != bool(prop_distance):
    raise SystemExit('Prop circle flag/distance initializers are incomplete.')
maximum_scale=definition(world,'GetMaximumScale(')
prop_body='bool '+definition(prop,'PropEntityModule::RenderModel(')
world_body='void '+definition(world,'WorldEntityModule::RenderInstance(')
code=('#define BRN_HAS_PROP_CIRCLE_IMAGE_DATA '+('1' if has_prop_circle_data else '0')+'\n'
      'namespace BrnWorld {\n'+track+'\n'+world_flag+'\n'+prop_flag+'\n'+prop_distance+'\n'
      'static f32 '+maximum_scale+'\n'+prop_body+'\n'+world_body+'\n}\n')
numeric=compile_and_run(here/'PCPropReflectionLod.cpp','pc_prop_reflection_lod.inc',code,'PCPropReflectionLod',output_dir=options.output_dir)
if options.expect_circle_regression:
    if options.output_dir is None:
        raise SystemExit('--expect-circle-regression requires --output-dir')
    output=options.output_dir/'run.stdout.txt'
    receipt=options.output_dir/'result.json'
    if numeric is None or not output.is_file() or not receipt.is_file():
        raise SystemExit('expected-baseline run did not compile and execute')
    stdout=output.read_text(encoding='utf-8',errors='replace')
    legacy=re.search(r'PCReflectionBaseline: (\d+) checks, (\d+) failures',stdout)
    circles=re.search(r'PCDebugCircles: (\d+) checks, (\d+) failures',stdout)
    run=json.loads(receipt.read_text(encoding='utf-8'))
    if (not legacy or not circles or int(legacy.group(2)) != 0 or
            int(circles.group(1)) == 0 or int(circles.group(2)) == 0 or
            run.get('process_exit_code') != 1):
        raise SystemExit('exact-base source did not preserve reflection checks and fail the new circle assertions')
    print('EXPECTED_BASELINE_RED: original reflection checks pass; new circle assertions fail')
    raise SystemExit(0)
raise SystemExit(report('run_pc_prop_reflection_lod',[],numeric,1))
