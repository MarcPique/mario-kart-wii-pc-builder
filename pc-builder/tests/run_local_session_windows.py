"""Build and exercise the real launcher with two harmless Win32 game stand-ins.

Usage: python pc-builder/tests/run_local_session_windows.py --cxx PATH --out DIR
The directory must be empty. All artifacts stay there for review; no ROM needed.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tomllib


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cxx', required=True)
    parser.add_argument('--out', required=True)
    args = parser.parse_args()
    if sys.platform != 'win32':
        raise SystemExit('This integration test requires Windows.')
    repo = Path(__file__).resolve().parents[2]
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    if any(out.iterdir()):
        raise SystemExit('Use an empty output directory for this test.')
    os.environ['TEMP'] = os.environ['TMP'] = str(out)
    root = out / "Prueba d'idioma 日本語"
    game = root / 'GameData/Install/RetroRewind/RetroRewind.exe'
    game.parent.mkdir(parents=True)
    (root / 'GameData/portable-data.txt').write_text('test fixture only\n')
    (root / 'fixture-owned.txt').write_text('Local launcher integration fixture\n')
    (root / 'UserData/Saves').mkdir(parents=True)
    (root / 'UserData/Saves/sentinel').write_bytes(b'primary-private-data')
    (root / 'UserData/Config.toml').write_text('[system]\nlanguage = "ca"\n[video]\nmsaa_samples = 4\n')
    common = [args.cxx, '-std=c++20', '-O2', '-static', '-municode', '-mwindows', '-I' + str(repo / 'runtime/include')]
    subprocess.run(common + [str(repo / 'pc-builder/tests/local_session_fixture.cpp'), '-o', str(game)], check=True)
    launcher = root / 'Mario Kart Wii.exe'
    subprocess.run(common + [str(repo / 'pc-builder/worker/portable_launcher.cpp'), '-lole32', '-lshell32', '-o', str(launcher)], check=True)
    result = subprocess.run([str(launcher)], cwd=root, timeout=40)
    config = tomllib.loads((root / 'UserData/Profiles/online-pair-2/Config.toml').read_text(encoding='utf-8'))
    assert Path(config['paths']['dvd_root']) == root / 'GameData/Install/GameAssets/DATA'
    assert Path(config['paths']['retro_rewind_root']) == root / 'GameData/RetroRewind6'
    assert config['system']['language'] == 'ca' and config['video']['msaa_samples'] == 4
    assert config['audio']['muted'] and config['video']['display_mode'] == 'windowed'
    failures = {p.name: p.read_text() for p in root.glob('fixture-*-failure.txt')}
    if (root / 'fixture-failure.txt').exists():
        failures['primary'] = (root / 'fixture-failure.txt').read_text()
    report = {'exit_code': result.returncode, 'failures': failures,
              'result': (root / 'fixture-result.txt').read_text() if (root / 'fixture-result.txt').exists() else '',
              'launcher_log': (root / 'UserData/Logs/PC-launcher.log').read_text()}
    (out / 'result.json').write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding='utf-8')
    print(json.dumps(report, indent=2, ensure_ascii=True))
    if result.returncode or failures or not report['result'].startswith('PASS:'):
        raise SystemExit(1)


if __name__ == '__main__':
    main()
