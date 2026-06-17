#!/usr/bin/env python3
import sys
from collections import defaultdict


def parse_line(line):
    parts = [p.strip() for p in line.split("|")]
    if len(parts) < 7:
        return None

    compressor, file, ratio, ctime, dtime, cmem, dmem = parts[:7]

    try:
        return {
            "compressor": compressor,
            "file": file,
            "ratio": float(ratio),
            "ctime": float(ctime),
            "dtime": float(dtime),
            "cmem": float(cmem),
            "dmem": float(dmem),
        }
    except ValueError:
        return None


def format_table(rows, file_name):
    headers = ["Rank", "Compressor", "Ratio (%)", "C Time", "D Time", "C Mem", "D Mem"]

    widths = [5, 22, 12, 10, 10, 12, 12]

    def sep(char="-"):
        return "+" + "+".join(char * w for w in widths) + "+"

    def row_fmt(cols):
        return "|" + "|".join(str(c).ljust(w) for c, w in zip(cols, widths)) + "|"

    print("\n" + "=" * 90)
    print(f"FILE: {file_name}")
    print("=" * 90)

    print(sep("-"))
    print(row_fmt(headers))
    print(sep("="))

    for i, r in enumerate(rows, 1):
        print(
            row_fmt(
                [
                    i,
                    r["compressor"],
                    f"{r['ratio']:.6f}",
                    f"{r['ctime']:.3f}",
                    f"{r['dtime']:.3f}",
                    f"{r['cmem']:.2f}",
                    f"{r['dmem']:.2f}",
                ]
            )
        )

    print(sep("-"))


def main():
    if len(sys.argv) != 2:
        print("Usage: python3 rank.py <file>")
        sys.exit(1)

    filename = sys.argv[1]
    groups = defaultdict(list)

    with open(filename, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("compressor"):
                continue

            row = parse_line(line)
            if row:
                groups[row["file"]].append(row)

    for file_name in sorted(groups.keys()):
        rows = groups[file_name]

        # PRIMARY: ratio (lower better)
        # TIEBREAK: compression time (lower better)
        rows.sort(key=lambda x: (x["ratio"], x["ctime"]))

        format_table(rows, file_name)


if __name__ == "__main__":
    main()
