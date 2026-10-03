"""Read-only Deck check. Output only hashes and scoped process/unit state."""
import hashlib
import json
import pathlib
import subprocess

def digest(path):
    with open(path, 'rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

paths = {
    'binary': '/home/deck/Applications/Cemu.AppImage',
    'profile': '/home/deck/.config/Cemu/gameProfiles/0005000010139200.ini',
    'figure': '/run/media/deck/sdCardOnDeck/Emulation/saves/skylanders-figures/Dark Stealth Elf.sky',
}
out = {key: digest(path) for key, path in paths.items()}
save = pathlib.Path('/home/deck/Emulation/roms/wiiu/mlc01/usr/save/00050000/10139200')
out['saves'] = {str(p.relative_to(save)): digest(p) for p in sorted(save.rglob('*')) if p.is_file()}
out['older_source_patch'] = hashlib.sha256(subprocess.check_output(
    ['git', 'diff', '--binary'], cwd='/home/deck/.local/share/cemu-native-main-build/source')).hexdigest()
out['processes'] = [line for line in subprocess.check_output(['ps', '-eo', 'pid,comm'], text=True).splitlines()
                    if any(word in line.lower() for word in ('cemu', 'ffmpeg', 'gdb', 'kwin', 'gamescope'))]
out['running_test_units'] = subprocess.check_output(
    ['systemctl', '--user', 'list-units', '--state=running', '--no-pager', 'swapforce*'], text=True)
out['containers'] = subprocess.check_output(['podman', 'ps', '--format', '{{.Names}}'], text=True)
print(json.dumps(out, indent=2))
