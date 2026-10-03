import json
import pathlib
import collections
import subprocess

for mode in ['multicore', 'singlecore']:
    p = pathlib.Path(f'/home/deck/CemuSwapForceTest/upstream-0a2b6ff-{mode}-trace-20261002/evidence/trace')
    es = [json.loads(x) for x in (p / 'events.jsonl').read_text().splitlines()]
    c = collections.Counter()
    changed = []
    for e in es:
        if e['kind'] != 'upload' or not e['decoded']:
            continue
        def read(label):
            return (p / f"{e['id']:08}_upload_{label}.r8").read_bytes()
        a, b, d = read('before'), read('decoded'), read('after')
        c[(a == b, b == d, a == d)] += 1
        if a != b or b != d:
            changed.append({k: e[k] for k in ['id', 'frame', 'draw', 'phys', 'width']})
    report = {'mode': mode, 'events': len(es), 'dropped': max(e['dropped'] for e in es),
              'upload_equalities_before_decoded_after': {str(k): v for k, v in c.items()}, 'changed': changed}
    uploads = {}
    groups = collections.defaultdict(dict)
    found = []
    for e in es:
        if e['kind'] == 'upload':
            uploads[(e['texture'], e['reload'])] = e
        if e['kind'] == 'draw' and e['stage'] == 1:
            groups[(e['frame'], e['draw'])][e['unit']] = uploads.get((e['texture'], e['reload']))
    for key, g in groups.items():
        if set(g) == {0, 1, 2} and all(g[i] and g[i]['decoded'] for i in range(3)):
            found.append((key, [g[i]['id'] for i in range(3)]))
    out = p.parent / 'reconstructed'
    out.mkdir(exist_ok=True)
    def png(data, target):
        subprocess.run(['ffmpeg', '-v', 'error', '-y', '-f', 'rawvideo', '-pixel_format', 'yuv420p',
                        '-video_size', '2048x1024', '-i', 'pipe:0', '-frames:v', '1',
                        '-vf', 'crop=1280:720:0:0,scale=640:360', str(target)], input=data, check=True)
    for n, (key, ids) in enumerate(found):
        if n % max(1, len(found) // 16):
            continue
        data = b''.join((p / f'{i:08}_upload_decoded.r8').read_bytes() for i in ids)
        target = out / f"draw-{key[0]}-{key[1]}-ids-{'-'.join(map(str, ids))}.yuv"
        target.write_bytes(data)  # Original unit order Y,V,U, retained as diagnostic bytes.
        y = 2048 * 1024
        c = y // 4
        png(data[:y] + data[y+c:] + data[y:y+c], target.with_suffix('.png'))
    bind_groups = []
    g = {}
    for e in es:
        if e['kind'] != 'bind':
            continue
        if e['unit'] == 0:
            g = {}
        g[e['unit']] = e
        if e['unit'] == 2 and set(g) == {0, 1, 2} and all(g[i]['before'] for i in range(3)):
            bind_groups.append(dict(g))
    bind_out = p.parent / 'bind-reconstructed'
    bind_out.mkdir(exist_ok=True)
    for n, g in enumerate(bind_groups):
        if n % max(1, len(bind_groups) // 20):
            continue
        data = b''.join((p / f"{g[i]['id']:08}_bind_before.r8").read_bytes() for i in [0, 2, 1])
        png(data, bind_out / f"bind-{g[0]['id']}.png")
    report['complete_sampled_bind_triplets'] = len(bind_groups)
    (out / 'draws.json').write_text(json.dumps(found))
    report['complete_sampled_draws'] = found
    (p.parent / 'analysis.json').write_text(json.dumps(report, indent=2))
    print(mode, report['events'], 'dropped', report['dropped'], dict(c), 'changed', changed[:10])
    print('complete draws', len(found), 'first', found[:3], 'last', found[-3:])
