"""Run on the Deck; roots must already contain isolated settings and copies."""
import os
import pathlib
import shutil
import subprocess
import sys

mode=sys.argv[1]
assert mode in ('singlecore','multicore')
build=pathlib.Path('/home/deck/.local/share/cemu-native-main-build/upstream-0a2b6ff')
unit='swapforce-upstream-job-ring-window-build-20261002.service'
def property_value(name):
    return subprocess.check_output(['systemctl','--user','show',unit,'-p',name,'--value'],text=True).strip()
assert property_value('ActiveState')=='inactive' and property_value('ExecMainStatus')=='0'
binary=build/'bin/Cemu_job_ring_window_trace_0a2b6ff'
if not binary.exists(): shutil.copy2(build/'bin/Cemu_release',binary)
root=pathlib.Path('/home/deck/CemuSwapForceTest/upstream-0a2b6ff-'+mode+'-job-ring-window-20261002')
assert (root/'evidence/trace').is_dir() and not (root/'evidence/intro.mp4').exists()
assert subprocess.run(['pgrep','-f','^/home/deck/.*[Cc]emu[^ ]*( |$)'],stdout=subprocess.DEVNULL).returncode==1
env=dict(line.split('=',1) for line in subprocess.check_output(['systemctl','--user','show-environment'],text=True).splitlines() if '=' in line)
xenv={k:env[k] for k in ('DISPLAY','XAUTHORITY','XDG_RUNTIME_DIR','DBUS_SESSION_BUS_ADDRESS') if k in env}
subprocess.run(['xset','dpms','force','on'],env={**os.environ,**xenv},check=True)
args=['systemd-run','--user','--unit=swapforce-upstream-'+mode+'-job-ring-window-20261002','--property=Nice=10']
for key,value in xenv.items(): args+=['--setenv='+key+'='+value]
args+=['--setenv=CEMU_SWAPFORCE_TRACE='+str(root/'evidence/trace'),'--setenv=CEMU_SWAPFORCE_TRACE_START_US=25000000','--setenv=CEMU_SWAPFORCE_TRACE_END_US=25250000','/bin/bash',str(build/'record-intro.sh'),str(root),str(binary),'1' if mode=='singlecore' else '3','50','24']
print(subprocess.check_output(['sha256sum',str(binary)],text=True).strip())
subprocess.run(args,check=True)
