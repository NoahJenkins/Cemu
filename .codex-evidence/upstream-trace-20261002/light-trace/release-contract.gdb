set pagination off
set confirm off
set breakpoint pending on
handle SIGSEGV nostop noprint pass
handle SIGPIPE nostop noprint pass
handle SIGUSR1 nostop noprint pass
python
import gdb,json,pathlib,struct
root=pathlib.Path('/home/deck/CemuSwapForceTest/upstream-0a2b6ff-release-contract-x11-20261002/evidence')
done=False
class BindBreak(gdb.Breakpoint):
 def stop(self):
  global done
  tex=int(gdb.parse_and_eval('$rdi'))
  try: surface=bytes(gdb.selected_inferior().read_memory(tex,64))
  except gdb.MemoryError: return False
  width,height=struct.unpack_from('>II',surface,4)
  if struct.unpack_from('>I',surface,20)[0]!=1 or width!=2048 or height!=1024: return False
  mem=int(gdb.parse_and_eval('*(char **)&memory_base'))
  table=int(gdb.parse_and_eval('&s_ppcHleTable'))
  def rd(addr,size): return bytes(gdb.selected_inferior().read_memory(mem+addr,size))
  imports=[]
  data=rd(0xe00000,0x3000)
  for off in range(0,len(data),4):
   op=struct.unpack_from('>I',data,off)[0]
   if op&0xffff0000!=0x04000000: continue
   index=op&0xffff
   target=struct.unpack('<Q',bytes(gdb.selected_inferior().read_memory(table+index*8,8)))[0]
   imports.append({'guest_import':hex(0xe00000+off),'native_symbol':gdb.execute('info symbol '+hex(target),to_string=True).strip()})
  (root/'imports.json').write_text(json.dumps(imports,indent=2)+'\n')
  private=root/'private'; private.mkdir(mode=0o700)
  ranges=[(0x02a80000,0x6000),(0x02950000,0x8000),(0x0280b000,0x2000)]
  for addr,size in ranges: (private/(hex(addr)+'.bin')).write_bytes(rd(addr,size))
  (root/'probe.json').write_text(json.dumps({'unmodified_executable':True,'stop':'first matching luma binding','texture_width':width,'texture_height':height,'code_ranges':[{'base':hex(a),'size':n} for a,n in ranges],'imports':len(imports),'timing_evidence':False},indent=2)+'\n')
  done=True;return True
BindBreak('GX2::_GX2SetTexture(GX2::GX2Texture*, Latte::REGADDR, unsigned int)',internal=True)
end
run
python
gdb.execute('kill')
gdb.execute('quit '+('0' if done else '1'))
end
