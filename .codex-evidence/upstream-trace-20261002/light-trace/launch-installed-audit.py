"""Run on Deck: installed executable, isolated copies, no diagnostic switches."""
import pathlib,subprocess,shutil,os,sys
mode=sys.argv[1]
assert mode in ('singlecore','multicore')
root=pathlib.Path('/home/deck/CemuSwapForceTest/upstream-0a2b6ff-installed-audit-'+mode+'-20261002')
build=pathlib.Path('/home/deck/.local/share/cemu-native-main-build/upstream-0a2b6ff')
source=pathlib.Path('/home/deck/CemuSwapForceTest/upstream-0a2b6ff-multicore-baseline-20261002')
assert not root.exists()
assert subprocess.run(['pgrep','-f','^/home/deck/.*[Cc]emu[^ ]*( |$)'],stdout=subprocess.DEVNULL).returncode==1
assert subprocess.run(['pgrep','-x','cemu'],stdout=subprocess.DEVNULL).returncode==1
root.mkdir()
for name in ('xdg/config','xdg/data','mlc01','figures'): shutil.copytree(source/name,root/name)
(root/'xdg/cache').mkdir(); (root/'evidence').mkdir()
p=root/'xdg/config/Cemu/settings.xml'; p.write_text(p.read_text().replace(str(source),str(root)))
env=dict(line.split('=',1) for line in subprocess.check_output(['systemctl','--user','show-environment'],text=True).splitlines() if '=' in line)
assert env.get('XDG_SESSION_TYPE')=='x11'
xenv={k:env[k] for k in ('DISPLAY','XAUTHORITY','XDG_RUNTIME_DIR','DBUS_SESSION_BUS_ADDRESS') if k in env}
assert pathlib.Path(xenv['XAUTHORITY']).is_file()
subprocess.run(['xset','dpms','force','on'],env={**os.environ,**xenv},check=True)
args=['systemd-run','--user','--unit=swapforce-installed-audit-'+mode+'-20261002','--property=Nice=10','--property=RuntimeMaxSec=100']
for key,value in xenv.items(): args+=['--setenv='+key+'='+value]
args+=['/bin/bash',str(build/'record-intro.sh'),str(root),'/home/deck/Applications/Cemu.AppImage','1' if mode=='singlecore' else '3','50','24']
subprocess.run(args,check=True)
