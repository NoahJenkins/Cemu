"""Reconstruct one observed MC draw and its immediately preceding guest bindings.

Unit order Y,V,U was checked against the displayed scene. This is a diagnostic
YUV conversion, not Cemu's exact shader. Snapshots are not atomic.
"""
import pathlib
import json
import subprocess
import hashlib

p = pathlib.Path('/home/deck/CemuSwapForceTest/upstream-0a2b6ff-multicore-trace-20261002/evidence/trace')
out = p.parent / 'boundary-pair'
out.mkdir(exist_ok=True)
binds = [3602, 3603, 3604]
uploads = [3605, 3607, 3609]
es = {e['id']: e for e in map(json.loads, (p / 'events.jsonl').read_text().splitlines())}

def read(i, label):
    return (p / f"{i:08}_{es[i]['kind']}_{label}.r8").read_bytes()

for label, ids, suffix in [('guest-bind', binds, 'before'), ('upload-before', uploads, 'before'),
                          ('actual-decoded', uploads, 'decoded'), ('upload-after', uploads, 'after')]:
    data = b''.join(read(ids[i], suffix) for i in [0, 2, 1])
    subprocess.run(['ffmpeg', '-v', 'error', '-y', '-f', 'rawvideo', '-pixel_format', 'yuv420p',
                    '-video_size', '2048x1024', '-i', 'pipe:0', '-frames:v', '1',
                    '-vf', 'crop=1040:584:0:0', str(out / f'{label}.png')], input=data, check=True)

report = {'bind_events': [es[i] for i in binds], 'upload_events': [es[i] for i in uploads], 'planes': []}
for b, u in zip(binds, uploads):
    samples = {'bind-before': read(b, 'before'), 'bind-after': read(b, 'after'),
               'upload-before': read(u, 'before'), 'actual-decoded': read(u, 'decoded'),
               'upload-after': read(u, 'after')}
    pairs = {}
    for a, d in [('bind-before', 'bind-after'), ('bind-before', 'upload-before'),
                 ('upload-before', 'actual-decoded'), ('actual-decoded', 'upload-after')]:
        width = es[u]['width']
        differences = [n for n, (x, y) in enumerate(zip(samples[a], samples[d])) if x != y]
        pairs[f'{a}_vs_{d}'] = {'different_bytes': len(differences),
                                'different_rows': len({n // width for n in differences})}
    report['planes'].append({'unit': es[b]['unit'], 'phys': es[b]['phys'],
                            'sha256': {k: hashlib.sha256(v).hexdigest() for k, v in samples.items()},
                            'differences': pairs})
(out / 'comparison.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report['planes'], indent=2))
