import json
import pathlib
import sys
p=pathlib.Path(sys.argv[1])
es=json.loads((p/'merged-events.json').read_text())
summary=json.loads((p/'summary.json').read_text())
byid={e['id']:e for e in es}
associations={a['bind_id']:a for a in summary.get('command_associations',[])}
bindings=[]
current={}
for e in es:
    if e['kind']=='bind':
        current[e['unit']]=e
        if e['unit']==2 and all(u in current for u in (0,1,2)):
            bindings.append([current[u] for u in (0,1,2)])
results=[]
for e in es:
    if not e.get('job'): continue
    previous=[bs for bs in bindings if bs[-1]['us']<e['us']]
    if not previous: continue
    bs=previous[-1]
    a=associations.get(bs[0]['id'])
    marker=byid.get(a['retire_id']) if a and a['retire_id'] else None
    output=[e.get('jobY',0),e.get('jobV',0),e.get('jobU',0)]
    results.append({'job_event':e['id'],'job':hex(e['job']),
        'job_dimensions':[e['width'],e['height']], 'frame_index':e.get('frameIndex'),
        'requested_output_index':e.get('outputIndex'),
        'job_output_planes':[hex(x) for x in output], 'bind_ids':[b['id'] for b in bs],
        'bound_planes':[hex(b['phys']) for b in bs], 'same_plane_triplet':output==[b['phys'] for b in bs],
        'movie_submit_id':a['submit_id'] if a else None, 'movie_retire_id':marker['id'] if marker else None,
        'queued_before_movie_retirement':e['us']<marker['us'] if marker else None,
        'job_time_us':e['us'],'movie_retire_time_us':marker['us'] if marker else None})
(p/'job-overlap.json').write_text(json.dumps(results,indent=2)+'\n')
print('Tagged jobs',len(results),'same triplet',sum(r['same_plane_triplet'] for r in results),
      'before retirement',sum(r['queued_before_movie_retirement'] is True for r in results))
