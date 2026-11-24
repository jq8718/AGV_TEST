"""Replay measured corners through both C implementations and the shipped reference UI.

Does not connect hardware or change calibration. Uses the existing native C harness.
"""
import ctypes
import json
from pathlib import Path

import cv2
import numpy as np
from PyInstaller.archive.readers import CArchiveReader

from validate_reference_pose_uart import ROOT, REFERENCE, compile_library


def main():
    current = compile_library('yaw_current', ROOT / 'main')
    reference = compile_library('yaw_reference', REFERENCE / 'port/standalone_idf_test/main')
    archive = CArchiveReader(str(REFERENCE / 'dist/P4ScanOV9281Debugger.exe'))
    ui = {}
    exec(archive.open_embedded_archive('PYZ.pyz').extract('geometry'), ui)
    calibration = ui['default_ov9281_calibration']()
    ptr = lambda a: a.ctypes.data_as(ctypes.POINTER(ctypes.c_float))

    def measure(points):
        points = np.ascontiguousarray(points, dtype=np.float32).reshape(8)
        results = []
        for lib in (current, reference):
            result = np.zeros(9, np.float32)
            assert lib.measure(ptr(points), b'012345', ptr(result))
            results.append(result)
        assert np.array_equal(*results), results
        local = ui['estimate_decoded_pose'](points.reshape(4, 2), 1280, 720, calibration, '012345')
        return dict(current_yaw_deg=float(results[0][0]), reference_yaw_deg=float(results[1][0]),
                    packaged_reference_ui_yaw_deg=local.get('yaw_y_deg'),
                    ui_ambiguous=local.get('ambiguous'), corners=points.reshape(4, 2).tolist())

    precise = json.loads((ROOT / 'docs/yaw_precise_cdc_capture.json').read_text())
    measured = [dict(sequence=row['sequence'], **measure(row['corners'])) for row in precise]
    # Controlled local yaw sweep: keep the fitted near-front pose and its
    # observed pixel residual. This checks sign asymmetry in identical inputs,
    # without claiming these synthetic angles are physical measurements.
    analysis = json.loads((ROOT / 'docs/yaw_bias_analysis.json').read_text())
    points = np.array(analysis['corners'], np.float64)
    obj = np.array([[-15, 15, 0], [15, 15, 0], [15, -15, 0], [-15, -15, 0]], np.float64)
    matrix, distortion = calibration['camera_matrix'], calibration['dist_coeffs']
    seeds = cv2.solvePnPGeneric(obj, points, matrix, distortion, flags=cv2.SOLVEPNP_IPPE)
    rv, translation = cv2.solvePnPRefineLM(obj, points, matrix, distortion, seeds[1][1], seeds[2][1])
    rotation = cv2.Rodrigues(rv)[0]
    noise = points - cv2.projectPoints(obj, rv, translation, matrix, distortion)[0].reshape(4, 2)
    sweep = []
    for delta in [-10, -5, -2, -1, -.5, 0, .5, 1, 2, 5, 10]:
        a = np.deg2rad(delta)
        y = np.array([[np.cos(a), 0, np.sin(a)], [0, 1, 0], [-np.sin(a), 0, np.cos(a)]])
        perturbed = cv2.projectPoints(obj, cv2.Rodrigues(y @ rotation)[0], translation,
                                      matrix, distortion)[0].reshape(4, 2) + noise
        sweep.append(dict(synthetic_yaw_delta_deg=delta, **measure(perturbed)))
    report = dict(actual_precise_cdc_cases=len(measured), c_results_bit_identical=True,
                  reference_ui_executable=str(REFERENCE / 'dist/P4ScanOV9281Debugger.exe'),
                  measured=measured, synthetic_residual_yaw_sweep=sweep,
                  limitations='Same-corner host replay; does not prove two physical captures or sensor images are identical.')
    (ROOT / 'docs/yaw_reference_ui_replay.json').write_text(json.dumps(report, indent=2))
    print(json.dumps(dict(cases=len(measured), first=measured[0],
                         sweep=[{k: row[k] for k in ('synthetic_yaw_delta_deg', 'current_yaw_deg',
                                                    'reference_yaw_deg', 'packaged_reference_ui_yaw_deg')}
                                for row in sweep]), indent=2))


if __name__ == '__main__':
    main()
