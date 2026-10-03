"""Run on the Deck; roots must already contain isolated settings and copies."""
import os
import pathlib
import shutil
import subprocess
import sys

variant='baseline-repeat'
mode='multicore'
build=pathlib.Path('/home/deck/.local/share/cemu-native-main-build/upstream-0a2b6ff')
binary=build/'bin/Cemu_baseline_0a2b6ff'
root=pathlib.Path('/home/deck/CemuSwapForceTest/upstream-0a2b6ff-baseline-repeat-20261002')
if not root.exists():
    source=pathlib.Path('/home/deck/CemuSwapForceTest/upstream-0a2b6ff-control-before-post-movie-20261002')
    shutil.copytree(source,root)
    shutil.rmtree(root/'evidence'); (root/'evidence').mkdir()
    shutil.rmtree(root/'xdg/cache'); (root/'xdg/cache').mkdir()
    for name in ('xdg/config/Cemu/settings.xml',):
        p=root/name; p.write_text(p.read_text().replace(str(source),str(root)))
assert (root/'evidence').is_dir() and not (root/'evidence/intro.mp4').exists()
assert subprocess.run(['pgrep','-f','^/home/deck/.*[Cc]emu[^ ]*( |$)'],stdout=subprocess.DEVNULL).returncode==1
env=dict(line.split('=',1) for line in subprocess.check_output(['systemctl','--user','show-environment'],text=True).splitlines() if '=' in line)
xenv={k:env[k] for k in ('DISPLAY','XAUTHORITY','XDG_RUNTIME_DIR','DBUS_SESSION_BUS_ADDRESS') if k in env}
subprocess.run(['xset','dpms','force','on'],env={**os.environ,**xenv},check=True)
args=['systemd-run','--user','--unit=swapforce-upstream-'+variant+'-post-movie-20261002','--property=Nice=10']
for key,value in xenv.items(): args+=['--setenv='+key+'='+value]
if variant=='enabled': args+=['--setenv=CEMU_SWAPFORCE_POST_MOVIE_SUBMIT_WAIT=1']
(root/'evidence/intervention-config.json').write_text(__import__('json').dumps({'post_movie_submit_wait':variant=='enabled','trace':False,'cpu_mode':3})+'\n')
args+=['/bin/bash',str(build/'record-intro.sh'),str(root),str(binary),'1' if mode=='singlecore' else '3','50','24']
print(subprocess.check_output(['sha256sum',str(binary)],text=True).strip())
subprocess.run(args,check=True)
