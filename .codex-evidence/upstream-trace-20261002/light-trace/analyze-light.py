import json
import pathlib
import collections
import statistics
import sys

p = pathlib.Path(sys.argv[1])
events = {}
for line in (p / 'events.jsonl').read_text().splitlines():
    e = json.loads(line)
    if 'extra_for' in e:
        events[e.pop('extra_for')].update(e)
    else:
        events[e['id']] = e
es = sorted(events.values(), key=lambda e: e['us'])
counts = collections.Counter(e['kind'] for e in es)
print('Counts', dict(counts), 'Dropped', max((e['dropped'] for e in es), default=0))
for kind in counts:
    if kind in ['bind', 'draw', 'upload', 'retire', 'compile', 'guestcode']:
        continue
    callers = collections.Counter((e['core'], hex(e['lr']), tuple(hex(x) for x in e['stack'])) for e in es if e['kind'] == kind)
    print(kind, 'callers', callers.most_common(8))
for kind in ['bind', 'upload']:
    durations = [e['endUs'] - e['us'] for e in es if e['kind'] == kind]
    print(kind, 'sample duration us median,p95,max', statistics.median(durations), sorted(durations)[int(.95 * len(durations))], max(durations))
uploads = [e for e in es if e['kind'] == 'upload']
print('Upload sampled hash changed before/decoded', sum(e['beforeHash'] != e['decodedHash'] for e in uploads),
      'decoded/after', sum(e['decodedHash'] != e['afterHash'] for e in uploads), 'of', len(uploads))
print('First bindings', [(e['id'], hex(e['phys']), hex(e['lr']), e['stack']) for e in es if e['kind'] == 'bind'][:3])
(p.parent / 'merged-events.json').write_text(json.dumps(es))

# A logged marker may occur before the submit event is queued. Compare by timestamp value.
retired = {}
for e in es:
    if e['kind'] == 'retire': retired.setdefault(e['value'], e)
waits = [e for e in es if e['kind'] == 'wait-exit' and e['value'] in retired]
early = [e['id'] for e in waits if e['us'] < retired[e['value']]['us']]
summary = {'counts': dict(counts), 'dropped': max((e['dropped'] for e in es), default=0),
           'uploads': len(uploads), 'input_vs_decoded_changes': sum(e['beforeHash'] != e['decodedHash'] for e in uploads),
           'decoded_vs_after_changes': sum(e['decodedHash'] != e['afterHash'] for e in uploads),
           'matched_waits': len(waits), 'early_wait_ids': early}
print('Matched waits', len(waits), 'early return IDs', early)
# bind.value is the guest command write address only in the command-window build.
if any(e['kind'] == 'bind' and e['value'] for e in es):
    associations = []
    submits = [e for e in es if e['kind'] == 'submit']
    for bind in (e for e in es if e['kind'] == 'bind'):
        containing = [e for e in submits if e['us'] >= bind['us'] and e['phys'] <= bind['value'] < e['phys'] + e['width']]
        if not containing: continue
        submit = containing[0]
        marker = retired.get(submit['value'])
        intervening = [e['id'] for e in waits if bind['us'] < e['us'] < submit['us']]
        associations.append({'bind_id': bind['id'], 'command_address': hex(bind['value']),
             'is_display_list': bool(bind['reload']), 'submit_id': submit['id'],
             'timestamp': submit['value'], 'retire_id': marker['id'] if marker else None,
             'wait_exit_ids_before_this_submission': intervening})
    summary['command_associations'] = associations
(p.parent / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
