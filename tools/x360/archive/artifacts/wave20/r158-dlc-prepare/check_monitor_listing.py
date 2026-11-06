import hashlib
import json
import re
import sys
from pathlib import Path

listing = Path(sys.argv[1])
text = listing.read_text(encoding='utf-8', errors='replace')
begin = '; Begin code for function: ?StartDLCMonitor@DLCManager@BrnResource@@'
end = '; End code for function: ?StartDLCMonitor@DLCManager@BrnResource@@'
assert text.count(begin) == 1 and text.count(end) == 1
body = text.split(begin, 1)[1].split(end, 1)[0]
name = body.find('DLCNotifyThread?$AA@')
store = re.search(r'\bstw\s+r11,lParameters\$\d+\+14h\(r1\)', body)
thread_begin = re.search(r'\bbl\s+\?Begin@Thread@', body)
checks = {'name_literal': name >= 0,
          'parameter_name_store': bool(store),
          'thread_begin': bool(thread_begin),
          'store_before_begin': bool(store and thread_begin and name < store.start() < thread_begin.start())}
out = {'listing': str(listing), 'sha256': hashlib.sha256(listing.read_bytes()).hexdigest(),
       'checks': checks, 'pass': all(checks.values())}
output = listing.parent / 'monitor-listing-check.json'
output.write_text(json.dumps(out, indent=2) + '\n', encoding='utf-8')
print(json.dumps({'pass': out['pass'], 'output': str(output)}, indent=2))
if not out['pass']:
    raise SystemExit(1)
