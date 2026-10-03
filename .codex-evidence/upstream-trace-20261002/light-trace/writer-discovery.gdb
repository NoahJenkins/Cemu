set pagination off
set confirm off
set breakpoint pending on
handle SIGSEGV nostop noprint pass
handle SIGPIPE nostop noprint pass
handle SIGUSR1 nostop noprint pass
python
import gdb, pathlib, json, struct, time
root = pathlib.Path('/home/deck/CemuSwapForceTest/upstream-0a2b6ff-writer-discovery-20261002/evidence')
private = root / 'private-guest-code'
private.mkdir(exist_ok=True)
out = (root / 'watchpoints.jsonl').open('w')
watched = set()
count = 0
start = time.monotonic()

def read(address, size):
    return bytes(gdb.selected_inferior().read_memory(address, size))

def emit(event):
    out.write(json.dumps(event) + '\n'); out.flush()

def mappings():
    result = []
    for line in (root / 'trace/events.jsonl').read_text().splitlines():
        try: e = json.loads(line)
        except ValueError: continue
        if e.get('kind') == 'compile': result.append(e)
        elif 'extra_for' in e and result and result[-1]['id'] == e['extra_for']:
            result[-1].update(e)
    return result

class WriterWatch(gdb.Breakpoint):
    def __init__(self, address, plane, guest, membase):
        super().__init__(f'*(unsigned char*){address}', type=gdb.BP_WATCHPOINT, wp_class=gdb.WP_WRITE, internal=True)
        self.plane, self.guest, self.membase = plane, guest, membase
    def stop(self):
        global count
        pc = int(gdb.parse_and_eval('$pc'))
        event = {'kind': 'write', 'seconds': time.monotonic()-start, 'plane_width': self.plane,
                 'guest_address': self.guest, 'host_pc': pc, 'thread': gdb.selected_thread().global_num}
        mapped = [e for e in mappings() if e['texture'] <= pc < e['texture'] + e.get('value', 0)]
        event['compile_ranges'] = mapped
        if mapped:
            # x64 JIT reserves RSP for PPCInterpreter_t. Offsets come from PPCState.h.
            try:
                context = read(int(gdb.parse_and_eval('$rsp')), 720)
                event['cached_guest_ip'] = struct.unpack_from('<I',context,0)[0]
                event['cached_guest_sp'] = struct.unpack_from('<I',context,8)[0]
                event['cached_guest_lr'] = struct.unpack_from('<I',context,696)[0]
                event['core'] = struct.unpack_from('<I',context,708)[0]
                sp = event['cached_guest_sp']; stack=[]
                for n in range(12):
                    if sp < 0x10000000 or sp > 0x3ffffff8 or sp & 3: break
                    next = struct.unpack('>I',read(self.membase+sp,4))[0]
                    if next <= sp or next-sp > 0x10000 or next > 0x3ffffff8: break
                    lr = struct.unpack('>I',read(self.membase+next+4,4))[0]
                    stack.append(lr); sp=next
                event['cached_stack'] = stack
            except gdb.MemoryError: event['context_unreadable'] = True
            for e in mapped:
                entry = e['phys']; size = min(e['width'], 65536)
                path = private / f'writer-{entry:08x}-{size:x}.ppc'
                if not path.exists(): path.write_bytes(read(self.membase + entry, size))
        event['host_instructions'] = gdb.execute(f'x/12i {pc-24}', to_string=True)
        emit(event)
        count += 1
        if count >= 24: return True
        return False

class BindBreak(gdb.Breakpoint):
    def stop(self):
        tex = int(gdb.parse_and_eval('$rdi'))
        try: surface = read(tex, 64)
        except gdb.MemoryError: return False
        width,height = struct.unpack_from('>II', surface, 4)
        fmt = struct.unpack_from('>I',surface,20)[0]
        image = struct.unpack_from('>I',surface,36)[0]
        pitch = struct.unpack_from('>I',surface,60)[0]
        if fmt != 1 or width not in (1024,2048) or height not in (512,1024) or width in watched:
            return False
        # Three texture units: arm both chroma buffers by their first unique address.
        slot = (width,image)
        if slot in watched or len(watched) >= 3: return False
        membase = int(gdb.parse_and_eval('*(char **)&memory_base'))
        x,y = (500,300) if width==2048 else (250,150)
        guest = image + y*pitch + x
        WriterWatch(membase+guest,width,guest,membase)
        watched.add(slot)
        emit({'kind':'armed','width':width,'height':height,'image':image,'pitch':pitch,'guest_address':guest})
        if len(watched)==3: self.enabled=False
        return False

BindBreak('GX2::_GX2SetTexture(GX2::GX2Texture*, Latte::REGADDR, unsigned int)', internal=True)
end
run
python
out.close()
end
kill
quit
