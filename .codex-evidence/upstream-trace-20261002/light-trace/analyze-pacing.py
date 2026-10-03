"""Summarize metadata. Visual results are reviewed separately in PACING.md."""
import json
import pathlib
import re

root = pathlib.Path(__file__).resolve().parent
pattern = re.compile(r'Diagnostic movie-list pacing (\d+) timestamp=(\d+) delay=(\d+)us elapsed=(\d+)us completed=(true|false) total=(\d+)us incomplete=(\d+)')
keys = ('count', 'timestamp', 'delay_us', 'elapsed_us', 'completed', 'total_us', 'incomplete')
results = []
for variant in ('control-before', 'delay2000', 'retirement', 'delay1000', 'control-after'):
    folder = root / ('pacing-' + variant) / 'evidence'
    log = (folder / 'cemu-log.txt').read_text()
    events = []
    for match in pattern.finditer(log):
        events.append(dict(zip(keys, (v == 'true' if k == 'completed' else int(v)
                                      for k, v in zip(keys, match.groups())))))
    assert 'Multi-core recompiler (gameprofile)' in log
    assert 'duration=50.000000' in (folder / 'media-summary.txt').read_text()
    result = {'variant': variant, 'config': json.loads((folder / 'intervention-config.json').read_text()),
              'binary_sha256': (folder / 'run-hashes.txt').read_text().split()[0], 'events': events}
    if events:
        first480 = next(e for e in events if e['count'] == 480)
        result['first480_mean_us'] = first480['total_us'] / 480
        result['first480_pending_on_return'] = first480['incomplete']
    else:
        assert variant.startswith('control-')
    results.append(result)
assert len({r['binary_sha256'] for r in results}) == 1
(root / 'pacing-results.json').write_text(json.dumps(results, indent=2) + '\n')
for result in results:
    print(result['variant'], {k: v for k, v in result.items() if k.startswith('first480')})
