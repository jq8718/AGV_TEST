"""Verify decoder sources and runtime stages against the user's OV9281 reference."""
import argparse
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def normalized(path):
    return path.read_bytes().replace(b'\r\n', b'\n')


def section(path, start, end):
    text = normalized(path).decode()
    return text[text.index(start):text.index(end, text.index(start))]


def function(path, name):
    text = normalized(path).decode()
    match = re.search(r'^(?:static )?(?:esp_err_t|void|int) ' + name + r'\(', text, re.M)
    assert match, name
    next_function = re.search(r'\n(?:static )?(?:esp_err_t|void|int) \w+\(', text[match.end():])
    return text[match.start():match.end() + next_function.start()].strip()


def config(path):
    return dict(re.findall(r'^(CONFIG_\w+)=(.*)$', path.read_text(), re.M))


def main():
    args = argparse.ArgumentParser(description=__doc__)
    args.add_argument('--reference', type=Path, default=Path('D:/XC/2026/P4SCAN/ESP32P4_XCF_OV9281'))
    opts = args.parse_args()
    reference = opts.reference
    rows, failures = [], []
    base = Path('port/components/xcr_decoder')
    for source in sorted((reference / base).rglob('*')):
        if not source.is_file() or source.suffix not in ('.c', '.cpp', '.h', '.S', '.txt'):
            continue
        relative = source.relative_to(reference)
        target = ROOT / relative
        equal = target.exists() and normalized(source) == normalized(target)
        rows.append(dict(path=relative.as_posix(), equal=equal,
                         reference_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                         current_sha256=hashlib.sha256(target.read_bytes()).hexdigest() if target.exists() else None))
        if not equal:
            failures.append(relative.as_posix())
    reference_pipeline = reference / 'port/standalone_idf_test/main/agv_pipeline.c'
    current_pipeline = ROOT / 'main/agv_pipeline.c'
    stages = {}
    for name in ('finish_decoded_slot', 'decode_task'):
        stages[name] = function(reference_pipeline, name) == function(current_pipeline, name)
    # Capture stage retains only the E6 brightness controller as an adapter.
    capture_ref = function(reference_pipeline, 'capture_task')
    capture_cur = function(current_pipeline, 'capture_task')
    adapter_start = capture_cur.index('        /* E6 RAW8 brightness feedback')
    adapter_end = capture_cur.index('        slot->begin_us', adapter_start)
    capture_cur = capture_cur[:adapter_start] + capture_cur[adapter_end:]
    comment_start = capture_ref.index('        /* Preview and CDC snapshots')
    comment_end = capture_ref.index('        slot->begin_us', comment_start)
    capture_ref = capture_ref[:comment_start] + capture_ref[comment_end:]
    stages['capture_task_excluding_E6_exposure_adapter'] = capture_ref == capture_cur
    capture = Path('port/components/p4scan_camera_capture/p4scan_camera_capture.c')
    for name in ('p4scan_camera_wait_for_frame', 'p4scan_camera_begin_capture',
                 'p4scan_camera_acquire', 'p4scan_camera_release'):
        stages[name] = function(reference / capture, name) == function(ROOT / capture, name)
    stages['latest_frame_pool'] = normalized(reference_pipeline.with_name('latest_frame.h')) == normalized(ROOT / 'main/latest_frame.h')
    for name, equal in stages.items():
        if not equal:
            failures.append(name)
    reference_config = config(reference / 'sdkconfig')
    current_config = config(ROOT / 'sdkconfig.auto')
    settings = {key: dict(reference=reference_config.get(key), current=current_config.get(key))
                for key in sorted(set(reference_config) | set(current_config))
                if key.startswith(('CONFIG_XCR_', 'CONFIG_CAMERA_OV9281_')) or key in (
                    'CONFIG_P4SCAN_REALTIME_PIPELINE', 'CONFIG_P4SCAN_FRAME_STAGE_PIPELINE',
                    'CONFIG_P4SCAN_FRAME_MAX_AGE_MS', 'CONFIG_P4SCAN_CONTINUOUS_SCAN')}
    for key, values in settings.items():
        if values['reference'] != values['current']:
            failures.append(key)
    report = dict(reference=str(reference), reference_config='sdkconfig (1280x720, not legacy sdkconfig.idf60)',
                  decoder_files_checked=len(rows), decoder_files=rows, stages=stages,
                  settings=settings, failures=failures, passed=not failures,
                  retained_adapters=['AGV power/GPIO35/GPIO36/UART wiring', 'startup sensor detection/native UVC dimensions',
                                     'E6 image exposure and ISP denoise', 'per-sensor measured calibration/NVS',
                                     'IDF6/P4v3.2 platform and CPU400MHz'])
    (ROOT / 'docs/reference_decoder_alignment.json').write_text(json.dumps(report, indent=2), encoding='utf8')
    print(json.dumps(dict(files=len(rows), stages=stages, failures=failures, passed=not failures), indent=2))
    return int(bool(failures))


if __name__ == '__main__':
    raise SystemExit(main())
