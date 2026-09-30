"""Production relocator job byte-copy tests, including the original oversized-overlap bug."""
import argparse
from pathlib import Path
from fxgs_common import Tree, REPO, compile_and_run, report

parser=argparse.ArgumentParser()
parser.add_argument('--original-loop',action='store_true')
args=parser.parse_args()
source=Tree('e2d854af' if args.original_loop else None).read('src/GameShared/Jobs/Relocator/RelocatorJob.cpp')
numeric=compile_and_run(Path(__file__).with_name('PCRelocatorCopy.cpp'),
    'pc_relocator_unused.inc','', 'PCRelocatorCopy',
    extra_files={'pc_relocator_job.cpp':source},extra_sources=['pc_relocator_job.cpp'])
raise SystemExit(report('run_pc_relocator_copy',[],numeric,24))
