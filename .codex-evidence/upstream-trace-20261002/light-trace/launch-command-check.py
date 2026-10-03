"""Deck-only diagnostic: fresh copies, same binary, command snapshot check; no pacing intervention."""
import json
import os
import pathlib
import shutil
import subprocess
import sys

variant = sys.argv[1]
assert variant in ('enabled', 'disabled')
build = pathlib.Path('/home/deck/.local/share/cemu-native-main-build/upstream-0a2b6ff')
binary = build / 'bin/Cemu_command_check_0a2b6ff'
assert binary.is_file()
root = pathlib.Path('/home/deck/CemuSwapForceTest/upstream-0a2b6ff-command-check-' + variant + '-20261002')
source = pathlib.Path('/home/deck/CemuSwapForceTest/upstream-0a2b6ff-multicore-baseline-20261002')
assert not root.exists()
assert subprocess.run(['pgrep', '-f', '^/home/deck/.*[Cc]emu[^ ]*( |$)'], stdout=subprocess.DEVNULL).returncode == 1
assert subprocess.run(['pgrep', '-x', 'cemu'], stdout=subprocess.DEVNULL).returncode == 1
env = dict(line.split('=', 1) for line in subprocess.check_output(
    ['systemctl', '--user', 'show-environment'], text=True).splitlines() if '=' in line)
assert env.get('XDG_SESSION_TYPE') == 'x11'
xenv = {k: env[k] for k in ('DISPLAY', 'XAUTHORITY', 'XDG_RUNTIME_DIR', 'DBUS_SESSION_BUS_ADDRESS') if k in env}
assert pathlib.Path(xenv['XAUTHORITY']).is_file()
root.mkdir()
for name in ('xdg/config', 'xdg/data', 'mlc01', 'figures'):
    shutil.copytree(source / name, root / name)
(root / 'xdg/cache').mkdir()
(root / 'evidence').mkdir()
p = root / 'xdg/config/Cemu/settings.xml'
p.write_text(p.read_text().replace(str(source), str(root)))
subprocess.run(['xset', 'dpms', 'force', 'on'], env={**os.environ, **xenv}, check=True)
args = ['systemd-run', '--user', '--unit=swapforce-command-check-' + variant + '-20261002',
        '--property=Nice=10', '--property=RuntimeMaxSec=100']
for key, value in xenv.items():
    args += ['--setenv=' + key + '=' + value]
switches = {}
if variant == 'enabled':
    switches['CEMU_SWAPFORCE_COMMAND_CHECK'] = '1'
for key, value in switches.items():
    args += ['--setenv=' + key + '=' + value]
(root / 'evidence/intervention-config.json').write_text(json.dumps({
    'variant': variant, 'switches': switches, 'trace': False, 'cpu_mode': 3,
    'capture_fps': 24, 'duration': 50}, indent=2) + '\n')
args += ['/bin/bash', str(build / 'record-intro.sh'), str(root), str(binary), '3', '50', '24']
subprocess.run(args, check=True)
