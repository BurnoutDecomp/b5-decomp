import collections
import hashlib
import json
import re
import sys
from pathlib import Path

listing = Path(sys.argv[1])
text = listing.read_text(encoding='utf-8', errors='replace')
begin = '; Begin code for function: ?Prepare@DLCManager@BrnResource@@'
end = '; End code for function: ?Prepare@DLCManager@BrnResource@@'
assert text.count(begin) == 1 and text.count(end) == 1
body = text.split(begin, 1)[1].split(end, 1)[0]
calls = collections.Counter(re.findall(r'^\s+[0-9a-f]+\s+[0-9a-f]+\s+bl\s+([^\s;]+)',
                                       body, re.IGNORECASE | re.MULTILINE))
expected = {
    '?StartDLCMonitor@DLCManager@': 1,
    '?GetStatus@Thread@': 1,
    '?SPrintf@CgsCore@@': 6,
    'GetFileAttributesA': 4,
    '?Open@AsyncOp@': 1,
    '?IsDone@AsyncOp@': 3,
    '?Wait@AsyncOp@': 3,
    '?GetLastOperationResult@AsyncOp@': 3,
    '?GetLastOperationHandle@AsyncOp@': 2,
    '?DLCVersionCheckFailed@DLCManager@': 1,
    '?Read@AsyncOp@': 1,
    '?Close@AsyncOp@': 1,
    'sscanf': 1,
    '?GetPackMask@DLCFeatureAvailability@': 1,
    '?SetPackAvailabilityState@DLCFeatureAvailability@': 1,
}
checks = {}
for prefix, count in expected.items():
    actual = sum(value for name, value in calls.items() if name.startswith(prefix))
    checks[prefix] = {'expected': count, 'actual': actual, 'pass': actual == count}
result = {'listing': str(listing), 'sha256': hashlib.sha256(listing.read_bytes()).hexdigest(),
          'checks': checks, 'calls': calls, 'pass': all(x['pass'] for x in checks.values())}
output = listing.parent / 'prepare-listing-check.json'
output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
print(json.dumps({'pass': result['pass'], 'output': str(output)}, indent=2))
if not result['pass']:
    raise SystemExit(1)
