set pagination off
set confirm off
set breakpoint pending on
handle SIGSEGV nostop noprint pass
handle SIGPIPE nostop noprint pass
handle SIGUSR1 nostop noprint pass
python
import gdb, json, pathlib, struct
root=pathlib.Path('/home/deck/CemuSwapForceTest/upstream-0a2b6ff-resolve-imports-20261002/evidence')
class BindBreak(gdb.Breakpoint):
 def stop(self):
  tex=int(gdb.parse_and_eval('$rdi'))
  try: surface=bytes(gdb.selected_inferior().read_memory(tex,64))
  except gdb.MemoryError: return False
  width,height=struct.unpack_from('>II',surface,4)
  if struct.unpack_from('>I',surface,20)[0]!=1 or width!=2048 or height!=1024: return False
  mem=int(gdb.parse_and_eval('*(char **)&memory_base'))
  table=int(gdb.parse_and_eval('&s_ppcHleTable'))
  es=[]
  for address in (0xe00950,0xe0062c,0xe00974,0xe0096c,0xe0060c,0xe003ac,0xe00628,0xe004c4,0xe00640,0xe005e8,0xe005f4):
   op=struct.unpack('>I',bytes(gdb.selected_inferior().read_memory(mem+address,4)))[0]
   index=op&0xffff
   target=struct.unpack('<Q',bytes(gdb.selected_inferior().read_memory(table+index*8,8)))[0]
   es.append({'guest_import':hex(address),'opcode':hex(op),'index':index,'native_target':hex(target),'native_symbol':gdb.execute('info symbol '+hex(target),to_string=True).strip()})
  (root/'imports.json').write_text(json.dumps(es,indent=2)+'\n')
  return True
BindBreak('GX2::_GX2SetTexture(GX2::GX2Texture*, Latte::REGADDR, unsigned int)',internal=True)
end
run
kill
quit
