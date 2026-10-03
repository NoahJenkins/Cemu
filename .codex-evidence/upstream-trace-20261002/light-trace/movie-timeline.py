"""Extract queue and GPU timing for the first complete binding triplet in a window."""
import json
import pathlib
import sys
p=pathlib.Path(sys.argv[1])
es=json.loads((p/'merged-events.json').read_text())
first=next(e for e in es if e['kind']=='bind')
start=first['us']
selected=[]
for e in es:
    if not start <= e['us'] < start+3000: continue
    if e['kind'] not in ('bind','upload','draw','submit','retire','wait-exit') and e['lr'] not in (0x2bb8680,0x2bb873c,0x2bb8abc,0x2bb8b48): continue
    selected.append({**e,'since_first_bind_us':e['us']-start})
(p/'first-triplet-timeline.json').write_text(json.dumps(selected,indent=2)+'\n')
