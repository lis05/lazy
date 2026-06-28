"""
Compression benchmark script.

Usage:
    python3 benchmark.py /path/to/dataset/folder

Outputs a table grouped by file, sorted by compression ratio within each group.
Compressed artifacts go to ./.benchmark-compressed/
Decompressed artifacts go to ./.benchmark-decompressed/
"""

import sys
import os
import hashlib
import subprocess
import threading
import time
from pathlib import Path
from dataclasses import dataclass, field
from typing import Optional


# ──────────────────────────────────────────────────────────────────────────────
# Compressor configuration
# ──────────────────────────────────────────────────────────────────────────────
# Each entry is a dict with:
#   name               – display name in the table
#   compress           – command as a list; use {input} and {output} as placeholders
#   decompress         – command as a list; use {input} (compressed) and {output}
#   compress_stdout    – (optional bool) if True, stdout is captured → {output}
#   decompress_stdout  – (optional bool) if True, stdout is captured → {output}
#   comp_iters         – how many compression runs to average
#   decomp_iters       – how many decompression runs to average
#
# {input}  is replaced with the actual file path at runtime
# {output} is replaced with the destination path at runtime

COMPRESSORS = [
    {
        "name": "lzmpo -9rc",
        "compress":   ["./build/lzmpo", "-e", "-i",  "{input}", "-o", "{output}",
        "-9", "--turborc"],
        "decompress": ["./build/lzmpo", "-d", "-i", "{input}", "-o", "{output}"],
        "comp_iters":   1,
        "decomp_iters": 5,
    },
]


# ──────────────────────────────────────────────────────────────────────────────
# Memory tracking via /proc/<pid>/status
# VmPeak = peak virtual memory ever allocated by the process (in kB)
# ──────────────────────────────────────────────────────────────────────────────

def _poll_vmpeak(pid: int, result: list, stop_event: threading.Event) -> None:
    """
    Background thread: read /proc/<pid>/status every 5 ms and track the
    maximum VmPeak value seen.  VmPeak is the kernel's own high-water mark
    for virtual address space, so a single reading per tick is sufficient,
    but we track the max across readings to handle any fluctuation.
    """
    peak_kb = 0
    status_path = f"/proc/{pid}/status"
    while not stop_event.is_set():
        try:
            with open(status_path) as fh:
                for line in fh:
                    if line.startswith("VmPeak:"):
                        val = int(line.split()[1])  # always kB on Linux
                        if val > peak_kb:
                            peak_kb = val
                        break
        except FileNotFoundError:
            break  # process already exited
        except Exception:
            pass
        time.sleep(0.005)
    result.append(peak_kb)


def run_timed(
    cmd: list,
    stdout_path: Optional[Path] = None,
) -> tuple:
    """
    Execute *cmd*, optionally streaming stdout to *stdout_path*.

    Returns (elapsed_seconds: float, vmpeak_kb: int).

    When stdout_path is None, stdout is discarded (DEVNULL).
    """
    stdout_target = subprocess.PIPE if stdout_path else subprocess.DEVNULL

    proc = subprocess.Popen(
        cmd,
        stdin=subprocess.DEVNULL,
        stdout=stdout_target,
        stderr=subprocess.DEVNULL,
    )

    # Kick off the memory monitor before we start the clock
    peak_result: list = []
    stop_evt = threading.Event()
    monitor = threading.Thread(
        target=_poll_vmpeak,
        args=(proc.pid, peak_result, stop_evt),
        daemon=True,
    )
    monitor.start()

    t0 = time.perf_counter()

    if stdout_path:
        # Stream stdout to file without accumulating it all in RAM
        with open(stdout_path, "wb") as out_fh:
            while True:
                chunk = proc.stdout.read(1 << 16)  # 64 KiB chunks
                if not chunk:
                    break
                out_fh.write(chunk)
        proc.wait()
    else:
        proc.wait()

    elapsed = time.perf_counter() - t0

    stop_evt.set()
    monitor.join(timeout=0.5)
    vmpeak_kb = peak_result[0] if peak_result else 0

    if proc.returncode != 0:
        raise RuntimeError(
            f"Command exited with code {proc.returncode}: {' '.join(str(t) for t in cmd)}"
        )

    return elapsed, vmpeak_kb


# ──────────────────────────────────────────────────────────────────────────────
# Hashing (for decompression verification)
# ──────────────────────────────────────────────────────────────────────────────

def file_sha256(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


# ──────────────────────────────────────────────────────────────────────────────
# Result container
# ──────────────────────────────────────────────────────────────────────────────

@dataclass
class BenchResult:
    name: str
    original_bytes: int
    compressed_bytes: int = 0
    comp_times:   list = field(default_factory=list)   # seconds
    decomp_times: list = field(default_factory=list)
    comp_peaks:   list = field(default_factory=list)   # kB
    decomp_peaks: list = field(default_factory=list)
    error: Optional[str] = None

    @property
    def ratio(self) -> float:
        if self.original_bytes == 0:
            return 0.0
        return self.compressed_bytes / self.original_bytes

    @property
    def ratio_pct(self) -> float:
        return self.ratio * 100.0

    @property
    def comp_speed_mbs(self) -> float:
        if not self.comp_times:
            return 0.0
        avg_s = sum(self.comp_times) / len(self.comp_times)
        return (self.original_bytes / (1 << 20)) / avg_s if avg_s > 0 else 0.0

    @property
    def decomp_speed_mbs(self) -> float:
        if not self.decomp_times:
            return 0.0
        avg_s = sum(self.decomp_times) / len(self.decomp_times)
        return (self.original_bytes / (1 << 20)) / avg_s if avg_s > 0 else 0.0

    @property
    def comp_mem_mb(self) -> float:
        if not self.comp_peaks:
            return 0.0
        return (sum(self.comp_peaks) / len(self.comp_peaks)) / 1024.0

    @property
    def decomp_mem_mb(self) -> float:
        if not self.decomp_peaks:
            return 0.0
        return (sum(self.decomp_peaks) / len(self.decomp_peaks)) / 1024.0


# ──────────────────────────────────────────────────────────────────────────────
# Command helpers
# ──────────────────────────────────────────────────────────────────────────────

def _substitute(template: list, input_path: Path, output_path: Path) -> list:
    """Replace {input} and {output} tokens in a command template."""
    return [
        tok.replace("{input}", str(input_path)).replace("{output}", str(output_path))
        for tok in template
    ]


# ──────────────────────────────────────────────────────────────────────────────
# Core benchmark logic for one (compressor, file) pair
# ──────────────────────────────────────────────────────────────────────────────

def bench_compressor(
    cfg: dict,
    src_file: Path,
    comp_dir: Path,
    decomp_dir: Path,
) -> BenchResult:
    name = cfg["name"]
    # Build a filesystem-safe suffix from the config name
    safe_name = name.replace(" ", "_").replace("/", "-").replace("--", "")
    original_bytes = src_file.stat().st_size
    result = BenchResult(name=name, original_bytes=original_bytes)

    # Dedicate a unique path per (file, compressor) so runs don't clobber each other
    comp_out   = comp_dir   / f"{src_file.name}__{safe_name}"
    decomp_out = decomp_dir / f"{src_file.name}__{safe_name}"

    use_comp_stdout   = cfg.get("compress_stdout",   False)
    use_decomp_stdout = cfg.get("decompress_stdout", False)

    try:
        # ── Compression ───────────────────────────────────────────────────────
        # When the tool writes to stdout, {output} is a dummy in the cmd list
        # (the token won't appear); we pass stdout_path= to run_timed instead.
        if use_comp_stdout:
            comp_cmd = [
                tok.replace("{input}", str(src_file))
                for tok in cfg["compress"]
                # drop any {output} token – stdout mode doesn't use it
                if "{output}" not in tok
            ]
        else:
            comp_cmd = _substitute(cfg["compress"], src_file, comp_out)

        for _ in range(cfg.get("comp_iters", 1)):
            # Remove stale output so compressors that refuse to overwrite work
            if comp_out.exists():
                comp_out.unlink()
            if use_comp_stdout:
                elapsed, peak = run_timed(comp_cmd, stdout_path=comp_out)
            else:
                elapsed, peak = run_timed(comp_cmd)
            result.comp_times.append(elapsed)
            result.comp_peaks.append(peak)

        if not comp_out.exists():
            raise RuntimeError(f"Compressed output missing: {comp_out}")
        result.compressed_bytes = comp_out.stat().st_size

        # ── Decompression ─────────────────────────────────────────────────────
        if use_decomp_stdout:
            decomp_cmd = [
                tok.replace("{input}", str(comp_out))
                for tok in cfg["decompress"]
                if "{output}" not in tok
            ]
        else:
            decomp_cmd = _substitute(cfg["decompress"], comp_out, decomp_out)

        for _ in range(cfg.get("decomp_iters", 1)):
            if decomp_out.exists():
                decomp_out.unlink()
            if use_decomp_stdout:
                elapsed, peak = run_timed(decomp_cmd, stdout_path=decomp_out)
            else:
                elapsed, peak = run_timed(decomp_cmd)
            result.decomp_times.append(elapsed)
            result.decomp_peaks.append(peak)

        # ── Verification (not timed) ──────────────────────────────────────────
        if not decomp_out.exists():
            raise RuntimeError(f"Decompressed output missing: {decomp_out}")
        if file_sha256(src_file) != file_sha256(decomp_out):
            raise RuntimeError("Decompressed output does not match original (SHA-256 mismatch)")

    except Exception as exc:
        result.error = str(exc)

    return result


# ──────────────────────────────────────────────────────────────────────────────
# Table formatting
# ──────────────────────────────────────────────────────────────────────────────

COL_HEADERS = [
    "Rank",
    "Compressor",
    "Ratio",
    "C.Speed MB/s",
    "C.Mem MB",
    "D.Speed MB/s",
    "D.Mem MB",
]


def _fmt_row(cells: list, widths: list) -> str:
    return "  ".join(str(c).ljust(widths[i]) for i, c in enumerate(cells))


def format_table(file_results: list) -> str:
    """
    Build the full results table.  One section per file, separated by a
    blank line.  Within each section rows are sorted by compression ratio
    (ascending = better compression first) and numbered 1-based.
    Errored rows appear at the bottom, unnumbered.
    """
    # First pass: build all cell strings so we can size the columns
    all_sections: list = []   # list of (filename, header_cells, data_rows)
    all_data_rows: list = []

    for filename, results in file_results:
        valid   = sorted([r for r in results if not r.error], key=lambda r: r.ratio)
        errored = [r for r in results if r.error]

        rows: list = []
        for rank, r in enumerate(valid, 1):
            rows.append([
                str(rank),
                r.name,
                f"{r.ratio_pct:.5f}%",
                f"{r.comp_speed_mbs:.2f}",
                f"{r.comp_mem_mb:.1f}",
                f"{r.decomp_speed_mbs:.2f}",
                f"{r.decomp_mem_mb:.1f}",
            ])
        for r in errored:
            rows.append([
                "ERR",
                r.name,
                "–",
                "–",
                "–",
                "–",
                f"ERROR: {r.error}",
            ])
        all_sections.append((filename, rows))
        all_data_rows.extend(rows)

    # Determine column widths across all data + headers
    n_cols = len(COL_HEADERS)
    widths = [len(h) for h in COL_HEADERS]
    for row in all_data_rows:
        for i in range(min(n_cols, len(row))):
            widths[i] = max(widths[i], len(str(row[i])))

    sep    = "  ".join("─" * w for w in widths)
    header = _fmt_row(COL_HEADERS, widths)

    lines: list = []
    for filename, rows in all_sections:
        lines.append(f"File: {filename}")
        lines.append(sep)
        lines.append(header)
        lines.append(sep)
        for row in rows:
            lines.append(_fmt_row(row, widths))
        lines.append("")   # blank line between file groups

    return "\n".join(lines)


# ──────────────────────────────────────────────────────────────────────────────
# Entry point
# ──────────────────────────────────────────────────────────────────────────────

def main() -> None:
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <dataset-folder>", file=sys.stderr)
        sys.exit(1)

    dataset_dir = Path(sys.argv[1]).resolve()
    if not dataset_dir.is_dir():
        print(f"Error: '{dataset_dir}' is not a directory.", file=sys.stderr)
        sys.exit(1)

    # Collect files (non-recursive; symlinks to files are included)
    src_files = sorted(p for p in dataset_dir.iterdir() if p.is_file())
    if not src_files:
        print(f"No files found in '{dataset_dir}'.", file=sys.stderr)
        sys.exit(1)

    # Work directories created in the current working directory
    comp_dir   = Path(".benchmark-compressed")
    decomp_dir = Path(".benchmark-decompressed")
    comp_dir.mkdir(exist_ok=True)
    decomp_dir.mkdir(exist_ok=True)

    file_results: list = []
    total = len(src_files) * len(COMPRESSORS)
    done  = 0

    for src_file in src_files:
        print(f"\n{'═' * 62}", flush=True)
        print(f"  {src_file.name}  ({src_file.stat().st_size:,} bytes)", flush=True)
        print(f"{'═' * 62}", flush=True)

        results: list = []
        for cfg in COMPRESSORS:
            done += 1
            print(f"  [{done:>{len(str(total))}}/{total}] {cfg['name']} ... ",
                  end="", flush=True)
            r = bench_compressor(cfg, src_file, comp_dir, decomp_dir)
            results.append(r)
            if r.error:
                print(f"FAILED – {r.error}", flush=True)
            else:
                print(
                    f"ratio={r.ratio_pct:.3f}%  "
                    f"c={r.comp_speed_mbs:.1f} MB/s  "
                    f"d={r.decomp_speed_mbs:.1f} MB/s",
                    flush=True,
                )

        file_results.append((src_file.name, results))

    # ── Final table ───────────────────────────────────────────────────────────
    print(f"\n\n{'═' * 62}")
    print("  RESULTS")
    print(f"{'═' * 62}\n")
    print(format_table(file_results))


if __name__ == "__main__":
    main()
