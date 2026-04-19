#!/usr/bin/env python3
"""Send 30x60 FIR batches to referensi_matlab_fir_prog1_1800 firmware.

Important:
- Only one app can open the UART port at a time.
- Do not run Arduino Serial Plotter and this script on the same port simultaneously.
"""

from __future__ import annotations

import argparse
import re
import sys
import time
from pathlib import Path

try:
    import serial
    import serial.tools.list_ports
except ImportError as exc:  # pragma: no cover
    raise SystemExit(
        "pyserial belum terpasang. Install dulu: pip install pyserial"
    ) from exc


BATCH_SIZE = 60
NUM_BATCHES = 30
TOTAL_SAMPLES = BATCH_SIZE * NUM_BATCHES
SCALE_FACTOR = 1000.0

READY_RE = re.compile(r"\[READY\]\s+batch\s+(\d+)/(\d+)")
DONE_RE = re.compile(r"\[DONE\]\s+batch\s+(\d+)/(\d+)")


def load_scaled_ints(path: Path) -> list[int]:
    vals: list[int] = []
    with path.open("r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            s = line.strip()
            if not s:
                continue
            vals.append(int(round(float(s) * SCALE_FACTOR)))
    if len(vals) != TOTAL_SAMPLES:
        raise ValueError(f"{path} berisi {len(vals)} sampel, expected {TOTAL_SAMPLES}")
    return vals


def read_line(ser: serial.Serial, timeout_s: float = 10.0) -> str:
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        raw = ser.readline()
        if raw:
            return raw.decode("utf-8", errors="replace").strip()
    raise TimeoutError("Timeout menunggu data UART")


def wait_ready(ser: serial.Serial) -> tuple[int, int]:
    while True:
        line = read_line(ser, timeout_s=30.0)
        m = READY_RE.search(line)
        if m:
            return int(m.group(1)), int(m.group(2))
        # Tampilkan banner/info dari board agar terlihat status saat startup.
        if line:
            print(f"[board] {line}")


def stream_batches(
    ser: serial.Serial,
    xs: list[int],
    ys: list[int],
    output_log: Path | None,
    quiet_sample_echo: bool,
) -> None:
    log_f = output_log.open("w", encoding="utf-8") if output_log else None
    try:
        for b in range(NUM_BATCHES):
            ready_batch, ready_total = wait_ready(ser)
            if ready_batch != b + 1:
                print(
                    f"[warn] board ready batch {ready_batch}, host expected {b+1}",
                    file=sys.stderr,
                )
            if ready_total != NUM_BATCHES:
                print(
                    f"[warn] board total {ready_total}, host {NUM_BATCHES}",
                    file=sys.stderr,
                )

            s = b * BATCH_SIZE
            e = s + BATCH_SIZE
            for i in range(s, e):
                ser.write(f"{xs[i]} {ys[i]}\n".encode("ascii"))
            ser.flush()

            sample_count = 0
            while True:
                line = read_line(ser, timeout_s=30.0)
                if not line:
                    continue

                if DONE_RE.search(line):
                    print(f"[host] selesai batch {b+1}/{NUM_BATCHES}")
                    break

                if line.startswith("ERR"):
                    print(f"[board] {line}", file=sys.stderr)
                    continue

                # Sample line format: x y_out y_ref_plus_1000
                parts = line.split()
                if len(parts) == 3:
                    sample_count += 1
                    if log_f is not None:
                        log_f.write(line + "\n")
                    if not quiet_sample_echo:
                        print(line)
                else:
                    print(f"[board] {line}")

            if sample_count != BATCH_SIZE:
                print(
                    f"[warn] batch {b+1}: sample diterima {sample_count}, expected {BATCH_SIZE}",
                    file=sys.stderr,
                )
    finally:
        if log_f is not None:
            log_f.close()


def resolve_port(port_arg: str) -> str:
    if port_arg:
        return port_arg

    ports = sorted(p.device for p in serial.tools.list_ports.comports())
    if not ports:
        raise SystemExit(
            "Tidak ada serial port terdeteksi. Sambungkan board lalu jalankan ulang dengan --port /dev/ttyUSB0"
        )
    if len(ports) == 1:
        print(f"[host] auto-detect port: {ports[0]}")
        return ports[0]

    joined = ", ".join(ports)
    raise SystemExit(
        f"Terdeteksi lebih dari satu port: {joined}. Pilih salah satu dengan --port"
    )


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Kirim data FIR 30x60 dari file txt ke board via UART"
    )
    ap.add_argument(
        "--port",
        default="",
        help="Port serial, contoh: /dev/ttyUSB0 (opsional jika hanya ada satu port)",
    )
    ap.add_argument("--baud", type=int, default=38400, help="Baud rate (default: 38400)")
    ap.add_argument(
        "--input-txt",
        default="../matlab_file/data_asli_kolom.txt",
        help="Path data input txt (default: ../matlab_file/data_asli_kolom.txt)",
    )
    ap.add_argument(
        "--ref-txt",
        default="../matlab_file/data_filter_kolom.txt",
        help="Path data referensi txt (default: ../matlab_file/data_filter_kolom.txt)",
    )
    ap.add_argument(
        "--log-output",
        default="",
        help="Opsional file log output board (x y_out y_ref_plus_1000)",
    )
    ap.add_argument(
        "--quiet-samples",
        action="store_true",
        help="Jangan echo semua sample ke terminal (hanya progress batch)",
    )
    args = ap.parse_args()

    input_path = Path(args.input_txt).resolve()
    ref_path = Path(args.ref_txt).resolve()
    output_log = Path(args.log_output).resolve() if args.log_output else None

    xs = load_scaled_ints(input_path)
    ys = load_scaled_ints(ref_path)

    port = resolve_port(args.port)

    print("[host] data siap:")
    print(f"[host] input : {input_path}")
    print(f"[host] ref   : {ref_path}")
    print(f"[host] scale : x{int(SCALE_FACTOR)} (float txt -> integer UART)")
    print(f"[host] port  : {port} @ {args.baud}")
    print("[host] catatan: tutup Serial Plotter saat script ini jalan.")

    with serial.Serial(port, args.baud, timeout=0.2) as ser:
        # Beri waktu board reset/autoboot jika DTR memicu reset.
        time.sleep(0.5)
        ser.reset_input_buffer()
        stream_batches(ser, xs, ys, output_log, args.quiet_samples)

    print("[host] selesai 30 batch")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
