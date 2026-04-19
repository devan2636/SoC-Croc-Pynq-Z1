#!/usr/bin/env python3
"""Plot FIR output error against MATLAB reference from stream logs.

Input expected to contain sample lines in format:
    x y_out y_ref_plus_1000

The script is robust to logs that include prefixes such as:
    [Board] ...
and literal escaped newlines like "\\r\\n" inside one line.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

OFFSET = 1000
TRIPLET_RE = re.compile(r"(?<!\S)(-?\d+)\s+(-?\d+)\s+(-?\d+)(?!\S)")


def normalize_log_text(text: str) -> str:
    # Firmware/log pipeline sometimes emits literal "\\r\\n" tokens.
    return text.replace("\\r\\n", "\n").replace("\r\n", "\n")


def parse_triplets(text: str) -> np.ndarray:
    matches = TRIPLET_RE.findall(normalize_log_text(text))
    if not matches:
        raise ValueError("Tidak ditemukan data triplet 'x y_out y_ref_plus_1000' pada file log.")
    arr = np.array(matches, dtype=np.int64)
    return arr


def compute_metrics(err: np.ndarray) -> dict[str, float]:
    abs_err = np.abs(err)
    return {
        "mean": float(np.mean(err)),
        "std": float(np.std(err)),
        "mae": float(np.mean(abs_err)),
        "rmse": float(np.sqrt(np.mean(err.astype(np.float64) ** 2))),
        "max_abs": float(np.max(abs_err)),
    }


def plot_error(x: np.ndarray, y_out: np.ndarray, y_ref: np.ndarray, err: np.ndarray, title: str) -> None:
    idx = np.arange(len(x))
    m = compute_metrics(err)

    fig, axes = plt.subplots(2, 1, figsize=(14, 8), sharex=True)

    axes[0].plot(idx, y_out, label="y_out (FIR PYNQ)", linewidth=1.1)
    axes[0].plot(idx, y_ref, label="y_ref (MATLAB)", linewidth=1.1)
    axes[0].set_ylabel("Value")
    axes[0].set_title(title)
    axes[0].grid(alpha=0.25)
    axes[0].legend()

    axes[1].plot(idx, err, label="error = y_out - (y_ref_plus_1000 - 1000)", linewidth=1.0)
    axes[1].axhline(0, color="black", linewidth=0.8, alpha=0.7)
    axes[1].set_xlabel("Sample index")
    axes[1].set_ylabel("Error")
    axes[1].grid(alpha=0.25)
    axes[1].legend(loc="upper right")

    txt = (
        f"mean={m['mean']:.3f} | std={m['std']:.3f} | "
        f"MAE={m['mae']:.3f} | RMSE={m['rmse']:.3f} | max|e|={m['max_abs']:.3f}"
    )
    axes[1].text(0.01, 0.98, txt, transform=axes[1].transAxes, va="top")

    plt.tight_layout()
    plt.show()


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Plot error FIR PYNQ vs referensi MATLAB dari file log stream"
    )
    ap.add_argument(
        "--log",
        required=True,
        help="Path file log yang berisi output stream (x y_out y_ref_plus_1000)",
    )
    ap.add_argument(
        "--save-csv",
        default="",
        help="Opsional simpan data hasil parse + error ke CSV",
    )
    args = ap.parse_args()

    log_path = Path(args.log).resolve()
    text = log_path.read_text(encoding="utf-8", errors="ignore")
    arr = parse_triplets(text)

    x = arr[:, 0]
    y_out = arr[:, 1]
    y_ref_plus = arr[:, 2]
    y_ref = y_ref_plus - OFFSET
    err = y_out - y_ref

    print(f"Parsed samples : {len(x)}")
    print(f"Formula error  : y_out - (y_ref_plus_1000 - {OFFSET})")
    m = compute_metrics(err)
    print(
        f"mean={m['mean']:.6f}, std={m['std']:.6f}, "
        f"MAE={m['mae']:.6f}, RMSE={m['rmse']:.6f}, max|e|={m['max_abs']:.6f}"
    )

    if args.save_csv:
        out_csv = Path(args.save_csv).resolve()
        header = "x,y_out,y_ref_plus_1000,y_ref,error"
        data = np.column_stack([x, y_out, y_ref_plus, y_ref, err])
        np.savetxt(out_csv, data, delimiter=",", header=header, comments="", fmt="%d")
        print(f"CSV saved      : {out_csv}")

    plot_error(x, y_out, y_ref, err, title=f"FIR Error Analysis - {log_path.name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
