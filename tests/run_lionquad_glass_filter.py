"""The opt-in glass witness must preserve its budget for shatter textures.

Extract the real witness's entry block, before its unchanged bounded sampler.
Each test process sets the option before the first call, like the live harness.
No render path is altered: the early return belongs to the log-only witness.
"""
import argparse
from pathlib import Path
import sys
sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, compile_and_run, report

SOURCE = 'src/GameSource/Effects/Particles/Native/BrnLionBlendRenderer.cpp'

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--rev')
    parser.add_argument('--glass', action='store_true')
    args = parser.parse_args()
    fn = code_only(definition(Tree(args.rev).read(SOURCE), 'void LionQuadWitness('))
    head = fn[fn.index('{') + 1:fn.index('static const u32 KU_LIONQUAD_SLOTS')]
    inc = ('#define GLASS_MODE ' + str(int(args.glass)) + '\n'
           'void Probe(const cParticleMaterial& arMaterial) {\n' + head + '\n++giLogs;\n}\n')
    result = compile_and_run(Path(__file__).with_name('LionQuadGlassFilter.cpp'),
                             'lionquad_filter.inc', inc, 'LionQuadGlassFilter')
    return report('run_lionquad_glass_filter', [], result, 5)

if __name__ == '__main__':
    sys.exit(main())
