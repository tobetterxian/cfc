#!/usr/bin/env python3
import csv
import json
import statistics
import sys
from pathlib import Path


def ratio(num: float, denom: float) -> float:
    if denom == 0:
        return 0.0
    return num / denom


def load_rows(path: Path):
    with path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle))


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: summarize_session.py /path/to/frames.csv", file=sys.stderr)
        return 1

    csv_path = Path(sys.argv[1])
    rows = load_rows(csv_path)
    if not rows:
        print(json.dumps({"error": "empty csv", "path": str(csv_path)}, indent=2))
        return 0

    durations_us = [int(row["frame_duration_us"]) for row in rows]
    backlogs = [int(row["backlog"]) for row in rows]
    last = rows[-1]

    decoded = int(last["decoded"])
    scanned = int(last["scanned"])
    perfect = int(last["perfect"])
    bytes_total = int(last["bytes"])

    started_ns = int(rows[0]["frame_started_ns"])
    finished_ns = int(rows[-1]["frame_finished_ns"])
    duration_s = max((finished_ns - started_ns) / 1_000_000_000.0, 0.001)

    summary = {
        "frames_seen": len(rows),
        "duration_ms": round(duration_s * 1000, 2),
        "avg_frame_duration_ms": round(statistics.fmean(durations_us) / 1000.0, 3),
        "max_frame_duration_ms": round(max(durations_us) / 1000.0, 3),
        "avg_backlog": round(statistics.fmean(backlogs), 3),
        "decoder": {
            "calls": int(last["calls"]),
            "scanned": scanned,
            "decoded": decoded,
            "perfect": perfect,
            "bytes": bytes_total,
            "scan_ms": int(last["scan_ms"]),
            "extract_ms": int(last["extract_ms"]),
            "decode_ms": int(last["decode_ms"]),
            "files_in_flight": int(last["files_in_flight"]),
            "files_decoded": int(last["files_decoded"]),
            "mode": int(last["requested_mode"]),
            "detected_mode": int(last["detected_mode"]),
        },
        "metrics": {
            "goodput_kbps": round((bytes_total * 8.0) / duration_s / 1000.0, 3),
            "scan_success_ratio": round(ratio(decoded, scanned), 6),
            "decode_success_ratio": round(ratio(perfect, max(decoded, 1)), 6),
            "perfect_frame_ratio": round(ratio(perfect, len(rows)), 6),
        },
        "paths": {
            "frames_csv": str(csv_path),
        },
    }

    print(json.dumps(summary, indent=2, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
