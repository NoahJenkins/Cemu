set pagination off
set confirm off
set breakpoint pending on
handle SIGSEGV nostop noprint pass
handle SIGPIPE nostop noprint pass
handle SIGUSR1 nostop noprint pass
python
import gdb, json, pathlib, struct
root=pathlib.Path('/home/deck/CemuSwapForceTest/upstream-0a2b6ff-job-packets5-20261002/evidence')
out=(root/'job-packets.jsonl').open('w')
planes=set();armed=False;count=0
class PacketBreak(gdb.Breakpoint):
 def stop(self):
  global count
  cpu=int(gdb.parse_and_eval('$rdi'))
  context=bytes(gdb.selected_inferior().read_memory(cpu,720))
  lr=struct.unpack_from('<I',context,696)[0]
  if lr!=0x2bb873c: return False
  mem=int(gdb.parse_and_eval('*(char **)&memory_base'))
  gprs=list(struct.unpack_from('<32I',context,4))
  def rd(addr,size): return bytes(gdb.selected_inferior().read_memory(mem+addr,size))
  try: payload=rd(gprs[28],min(gprs[29],8))
  except gdb.MemoryError: return False
  words=[struct.unpack_from('>I',payload,i)[0] for i in range(0,len(payload)-3,4)]
  e={'core':struct.unpack_from('<I',context,708)[0],'lr':hex(lr),'queue_base':hex(gprs[27]),'payload_pointer':hex(gprs[28]),'payload_size':gprs[29], 'guest_sp':hex(gprs[1]), 'cached_gpr27':hex(gprs[27]), 'cached_gpr28':hex(gprs[28]), 'cached_gpr29':hex(gprs[29]),'words':[hex(w) for w in words],'plane_addresses':[hex(p) for p in sorted(planes)]}
  queue=gprs[3]-0x80
  if 0x10000000<=queue<0x3ffff800:
   write=struct.unpack('>I',rd(queue+0x108,4))[0]
   published=queue+0x110+((write-4)&0xff)*4
   ringword=struct.unpack('>I',rd(published,4))[0]
   e['native_mutex']=hex(gprs[3]);e['derived_queue']=hex(queue);e['write_index']=write;e['derived_payload']=hex(published);e['ring_word']=hex(ringword)
  if words:
   job=words[0]&~7
   if 0x10000000<=job<0x3ffff800:
    data=rd(job,0x800)
    (root/'private').mkdir(exist_ok=True)
    (root/'private'/('job-'+hex(job)+'.bin')).write_bytes(data)
    matches=[]
    for off in range(0,len(data)-3,4):
     val=struct.unpack_from('>I',data,off)[0]
     if val in planes: matches.append({'offset':hex(off),'plane':hex(val)})
    e['job_address']=hex(job);e['direct_plane_pointer_matches']=matches
    dest=struct.unpack_from('>I',data,0xe0)[0]
    if 0x10000000<=dest<0x3ffff800:
     header=rd(dest,0xa0)
     (root/'private'/('output-header-'+hex(dest)+'.bin')).write_bytes(header)
     e['output_descriptor_address']=hex(dest)
     e['output_descriptor_words']=[hex(struct.unpack_from('>I',header,i)[0]) for i in range(0,len(header),4)]
     e['job_ec']=hex(struct.unpack_from('>I',data,0xec)[0])
  out.write(json.dumps(e)+'\n');out.flush();count+=1
  return count>=4
class BindBreak(gdb.Breakpoint):
 def stop(self):
  global armed
  tex=int(gdb.parse_and_eval('$rdi'))
  try: surface=bytes(gdb.selected_inferior().read_memory(tex,64))
  except gdb.MemoryError: return False
  width,height=struct.unpack_from('>II',surface,4)
  if struct.unpack_from('>I',surface,20)[0]!=1 or width not in (1024,2048) or height not in (512,1024): return False
  planes.add(struct.unpack_from('>I',surface,36)[0])
  if not armed:
   mem=int(gdb.parse_and_eval('*(char **)&memory_base'));table=int(gdb.parse_and_eval('&s_ppcHleTable'))
   index=struct.unpack('>I',bytes(gdb.selected_inferior().read_memory(mem+0xe00974,4)))[0]&0xffff
   target=struct.unpack('<Q',bytes(gdb.selected_inferior().read_memory(table+index*8,8)))[0]
   PacketBreak('*'+hex(target),internal=True);armed=True
  return False
BindBreak('GX2::_GX2SetTexture(GX2::GX2Texture*, Latte::REGADDR, unsigned int)',internal=True)
end
run
python
out.close()
gdb.execute('kill')
gdb.execute('quit '+('0' if count==4 else '1'))
end
