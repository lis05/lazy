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


def format_table(rows, title):
    headers = ["Rank", "Compressor", "Ratio (%)", "C Time", "D Time", "C Mem", "D Mem"]

    widths = [5, 22, 12, 10, 10, 12, 12]

    def sep(char="-"):
        return "+" + "+".join(char * w for w in widths) + "+"

    def row_fmt(cols):
        return "|" + "|".join(str(c).ljust(w) for c, w in zip(cols, widths)) + "|"

    print("\n" + "=" * 90)
    print(title)
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
    if len(sys.argv) != 3:
        print("Usage: python3 rank.py <file> <top_count>")
        sys.exit(1)

    filename = sys.argv[1]
    try:
        top_count = int(sys.argv[2])
        if top_count <= 0:
            raise ValueError
    except ValueError:
        print("Error: <top_count> must be a positive integer.")
        sys.exit(1)

    groups = defaultdict(list)
    comp_totals = defaultdict(
        lambda: {
            "ratio": 0.0,
            "ctime": 0.0,
            "dtime": 0.0,
            "cmem": 0.0,
            "dmem": 0.0,
            "count": 0,
        }
    )

    with open(filename, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("compressor"):
                continue

            row = parse_line(line)
            if row:
                groups[row["file"]].append(row)

                # Accumulate values for total aggregation
                c = row["compressor"]
                comp_totals[c]["ratio"] += row["ratio"]
                comp_totals[c]["ctime"] += row["ctime"]
                comp_totals[c]["dtime"] += row["dtime"]
                comp_totals[c]["cmem"] += row["cmem"]
                comp_totals[c]["dmem"] += row["dmem"]
                comp_totals[c]["count"] += 1

    # Print individual file tables
    for file_name in sorted(groups.keys()):
        rows = groups[file_name]

        # PRIMARY: ratio (lower better)
        # TIEBREAK: compression time (lower better)
        rows.sort(key=lambda x: (x["ratio"], x["ctime"]))

        format_table(rows[:top_count], f"FILE: {file_name}")

    # Process and print aggregated totals table
    total_rows = []
    for compressor, data in comp_totals.items():
        total_rows.append(
            {
                "compressor": compressor,
                "ratio": data["ratio"],
                "ctime": data["ctime"],
                "dtime": data["dtime"],
                "cmem": data["cmem"],
                "dmem": data["dmem"],
            }
        )

    total_rows.sort(key=lambda x: (x["ratio"], x["ctime"]))
    format_table(total_rows[:top_count], "TOTAL PERFORMANCE ACROSS ALL FILES (SUMMED)")


if __name__ == "__main__":
    main()
