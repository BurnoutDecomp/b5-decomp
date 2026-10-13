"""Exercise the whole LinearMalloc provider and its actual IO-stack consumer.

--source-rev compiles the entire provider from a Git revision, for a numeric
before/after comparison. The current header retains the same public/member ABI.
"""
import argparse
from pathlib import Path
import fxgs_common as common

parser = argparse.ArgumentParser()
parser.add_argument('--source-rev')
args = parser.parse_args()
provider = 'src/GameShared/GameClasses/Memory/CgsLinearMalloc.cpp'
if args.source_rev:
    sources = ['linear_provider.cpp']
    generated = {'linear_provider.cpp': common.Tree(args.source_rev).read(provider)}
else:
    sources = [common.REPO / provider]
    generated = None
numeric = common.compile_and_run(Path(__file__).with_name('PCLinearMallocBorrowed.cpp'),
    'unused.inc', '', 'PCLinearMallocBorrowed', extra_sources=sources,
    extra_files=generated)
raise SystemExit(common.report('run_pc_linear_malloc_borrowed', [], numeric, 10))
