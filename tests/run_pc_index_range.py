"""Native unsigned index reductions, sorted oracles and guarded byte ranges."""
from pathlib import Path
import argparse,os
from fxgs_common import Tree,compile_and_run,report
os.environ.pop('NoDefaultCurrentDirectoryInExePath',None)
p=argparse.ArgumentParser()
p.add_argument('--truncate-wide',action='store_true')
p.add_argument('--ignore-tail',action='store_true')
p.add_argument('--overread-tail',action='store_true')
a=p.parse_args()
path='src/pc/gcm/renderengine/IndexRangePCLeaf.h'
source=Tree().read(path)
if a.truncate_wide:
    needle='std::memcpy(&ltValue, lpBytes + lu * sizeof(T), sizeof(T));'
    assert source.count(needle)==1
    source=source.replace(needle,needle+'\n            ltValue = static_cast<std::uint16_t>(ltValue);')
if a.ignore_tail:
    needle='lu < luCount';assert source.count(needle)==1
    source=source.replace(needle,'lu + 1 < luCount')
if a.overread_tail:
    needle='lu < luCount';assert source.count(needle)==1
    source=source.replace(needle,'lu <= luCount')
result=compile_and_run(Path(__file__).with_name('PCIndexRange.cpp'),'unused.inc','',
    'PCIndexRange',shadow={path:source})
raise SystemExit(report('run_pc_index_range',[],result,8504))
