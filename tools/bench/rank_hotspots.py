#!/usr/bin/env python3
import csv
import json
import sys
from pathlib import Path


def resolve_float(row, keys):
    for key in keys:
        if key in row and row[key] not in ("", None):
            try:
                return float(row[key])
            except ValueError:
                continue
    return 0.0


def resolve_int(row, keys):
    for key in keys:
        if key in row and row[key] not in ("", None):
            try:
                return int(float(row[key]))
            except ValueError:
                continue
    return 0


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: rank_hotspots.py /path/to/hotspots.csv", file=sys.stderr)
        return 1

    csv_path = Path(sys.argv[1])
    with csv_path.open("r", encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))

    ranked = []
    for row in rows:
        symbol = row.get("symbol") or row.get("name") or row.get("function") or "unknown"
        ranked.append(
            {
                "symbol": symbol,
                "self_ms": resolve_float(row, ["self_ms", "self_time_ms", "self"]),
                "total_ms": resolve_float(row, ["total_ms", "total_time_ms", "total"]),
                "calls": resolve_int(row, ["calls", "count"]),
            }
        )

    ranked.sort(key=lambda item: (item["total_ms"], item["self_ms"]), reverse=True)
    print(json.dumps({"path": str(csv_path), "top": ranked[:10]}, indent=2, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
