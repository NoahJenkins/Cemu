"""Run on Deck. Read-only paused probe of an owned, isolated Cemu process."""
import pathlib,subprocess,shutil,os,json,time
build=pathlib.Path('/home/deck/.local/share/cemu-native-main-build/upstream-0a2b6ff')
root=pathlib.Path('/home/deck/CemuSwapForceTest/upstream-0a2b6ff-release-contract-x11-20261002')
assert not root.exists()
assert subprocess.run(['pgrep','-f','^/home/deck/.*[Cc]emu[^ ]*( |$)'],stdout=subprocess.DEVNULL).returncode==1
source=pathlib.Path('/home/deck/CemuSwapForceTest/upstream-0a2b6ff-multicore-baseline-20261002')
root.mkdir()
for name in ('xdg/config','xdg/data','mlc01','figures'): shutil.copytree(source/name,root/name)
(root/'xdg/cache').mkdir(); (root/'evidence').mkdir()
p=root/'xdg/config/Cemu/settings.xml'; p.write_text(p.read_text().replace(str(source),str(root)))
(root/'xdg/config/Cemu/gameProfiles/0005000010139200.ini').write_text('[CPU]\ncpuMode = 3\n')
for attempt in range(30):
 env=dict(line.split('=',1) for line in subprocess.check_output(['systemctl','--user','show-environment'],text=True).splitlines() if '=' in line)
 if env.get('XDG_SESSION_TYPE')=='x11' and 'XAUTHORITY' in env and pathlib.Path(env['XAUTHORITY']).is_file() and 'DISPLAY' in env: break
 time.sleep(1)
else: raise RuntimeError('Desktop X11 environment unavailable')
xenv={k:env[k] for k in ('DISPLAY','XAUTHORITY','XDG_RUNTIME_DIR','DBUS_SESSION_BUS_ADDRESS') if k in env}
subprocess.run(['xset','dpms','force','off'],env={**os.environ,**xenv},check=True)
assert 'Monitor is Off' in subprocess.check_output(['xset','q'],env={**os.environ,**xenv},text=True)
args=['systemd-run','--user','--unit=swapforce-upstream-release-contract-x11-20261002','--property=Nice=10','--property=RuntimeMaxSec=120']
for key,value in xenv.items(): args+=['--setenv='+key+'='+value]
for key,name in (('XDG_CONFIG_HOME','config'),('XDG_DATA_HOME','data'),('XDG_CACHE_HOME','cache')): args+=['--setenv='+key+'='+str(root/'xdg'/name)]
args+=['--property=StandardOutput=file:'+str(root/'evidence/gdb.log'),'--property=StandardError=inherit','/usr/bin/gdb','-q','-batch','-x',str(build/'release-contract.gdb'),'--args',str(build/'bin/Cemu_baseline_0a2b6ff'),'--mlc',str(root/'mlc01'),'--game','/home/deck/Emulation/roms/wiiu/Skylanders - Swap Force (USA) (EnFrEsPt)/Skylanders - Swap Force (USA) (En,Fr,Es,Pt).wux']
subprocess.run(args,check=True)
