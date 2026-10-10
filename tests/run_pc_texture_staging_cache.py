"""Production staging-cache ownership, keys, limits and concurrent transfers."""
import argparse
import os
from pathlib import Path
from fxgs_common import Tree, compile_and_run, report

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
os.environ['BRN_TEXTURE_STAGING_CACHE'] = '1'
parser = argparse.ArgumentParser()
parser.add_argument('--ignore-format', action='store_true')
args = parser.parse_args()
source = Tree().read('src/pc/gcm/renderengine/TextureStagingCache.h')
if args.ignore_format:
    needle = '&& meFormat == lrOther.meFormat'
    assert source.count(needle) == 1
    source = source.replace(needle, '')
result = compile_and_run(Path(__file__).with_name('PCTextureStagingCache.cpp'),
                         'pc_texture_staging_cache.inc', source, 'PCTextureStagingCache')
raise SystemExit(report('run_pc_texture_staging_cache', [], result, 22))
