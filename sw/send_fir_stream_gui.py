#!/usr/bin/env python3
"""GUI for FIR streaming sender.

This app sends 30 batches x 60 samples to firmware
`referensi_matlab_fir_prog1_1800.c` using the same protocol as send_fir_stream.py.
"""

from __future__ import annotations

import queue
import re
import threading
import time
from collections import deque
from pathlib import Path
from typing import Optional

import tkinter as tk
from tkinter import filedialog, messagebox, ttk

import serial
import serial.tools.list_ports

from send_fir_stream import BATCH_SIZE, NUM_BATCHES, load_scaled_ints

try:
    from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
    from matplotlib.figure import Figure

    MATPLOTLIB_AVAILABLE = True
except Exception:
    MATPLOTLIB_AVAILABLE = False


READY_RE = re.compile(r"\[READY\]\s+batch\s+(\d+)/(\d+)")
DONE_RE = re.compile(r"\[DONE\]\s+batch\s+(\d+)/(\d+)")


class FirStreamGui(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title("FIR Stream Sender")
        self.geometry("1080x760")

        self.worker_thread: Optional[threading.Thread] = None
        self.stop_event = threading.Event()
        self.msg_queue: queue.Queue[tuple[str, object]] = queue.Queue()

        self.port_var = tk.StringVar(value="")
        self.baud_var = tk.StringVar(value="115200")
        self.input_var = tk.StringVar(value=str((Path(__file__).parent / "../matlab_file/data_asli_kolom.txt").resolve()))
        self.ref_var = tk.StringVar(value=str((Path(__file__).parent / "../matlab_file/data_filter_kolom.txt").resolve()))
        self.log_var = tk.StringVar(value=str((Path(__file__).parent / "fir_stream_out.txt").resolve()))
        self.quiet_var = tk.BooleanVar(value=True)
        self.status_var = tk.StringVar(value="Idle")
        self.progress_var = tk.IntVar(value=0)

        self.plot_max_points = 500
        self.sample_index = 0
        self.x_hist: deque[int] = deque(maxlen=self.plot_max_points)
        self.y_out_hist: deque[int] = deque(maxlen=self.plot_max_points)
        self.y_ref_hist: deque[int] = deque(maxlen=self.plot_max_points)
        self.plot_update_counter = 0

        self._build_ui()
        self.refresh_ports()
        self.after(100, self._drain_queue)

    def _build_ui(self) -> None:
        root = ttk.Frame(self, padding=10)
        root.pack(fill=tk.BOTH, expand=True)

        cfg = ttk.LabelFrame(root, text="Konfigurasi", padding=10)
        cfg.pack(fill=tk.X)

        ttk.Label(cfg, text="Serial Port").grid(row=0, column=0, sticky="w")
        self.port_combo = ttk.Combobox(cfg, textvariable=self.port_var, width=48)
        self.port_combo.grid(row=0, column=1, sticky="ew", padx=6)
        ttk.Button(cfg, text="Refresh", command=self.refresh_ports).grid(row=0, column=2)

        ttk.Label(cfg, text="Baud").grid(row=1, column=0, sticky="w", pady=(8, 0))
        ttk.Combobox(
            cfg,
            textvariable=self.baud_var,
            values=["9600", "19200", "38400", "57600", "115200", "230400"],
            width=12,
        ).grid(row=1, column=1, sticky="w", padx=6, pady=(8, 0))

        ttk.Label(cfg, text="Input TXT").grid(row=2, column=0, sticky="w", pady=(8, 0))
        ttk.Entry(cfg, textvariable=self.input_var).grid(row=2, column=1, sticky="ew", padx=6, pady=(8, 0))
        ttk.Button(cfg, text="Browse", command=self.pick_input).grid(row=2, column=2, pady=(8, 0))

        ttk.Label(cfg, text="Ref TXT").grid(row=3, column=0, sticky="w", pady=(8, 0))
        ttk.Entry(cfg, textvariable=self.ref_var).grid(row=3, column=1, sticky="ew", padx=6, pady=(8, 0))
        ttk.Button(cfg, text="Browse", command=self.pick_ref).grid(row=3, column=2, pady=(8, 0))

        ttk.Label(cfg, text="Output Log").grid(row=4, column=0, sticky="w", pady=(8, 0))
        ttk.Entry(cfg, textvariable=self.log_var).grid(row=4, column=1, sticky="ew", padx=6, pady=(8, 0))
        ttk.Button(cfg, text="Browse", command=self.pick_log).grid(row=4, column=2, pady=(8, 0))

        ttk.Checkbutton(
            cfg,
            text="Quiet samples (hanya progress batch)",
            variable=self.quiet_var,
        ).grid(row=5, column=1, sticky="w", pady=(8, 0))

        cfg.columnconfigure(1, weight=1)

        ctl = ttk.Frame(root, padding=(0, 10, 0, 10))
        ctl.pack(fill=tk.X)
        self.start_btn = ttk.Button(ctl, text="Start", command=self.start_stream)
        self.start_btn.pack(side=tk.LEFT)
        self.stop_btn = ttk.Button(ctl, text="Stop", command=self.stop_stream, state=tk.DISABLED)
        self.stop_btn.pack(side=tk.LEFT, padx=8)
        ttk.Label(ctl, textvariable=self.status_var).pack(side=tk.LEFT, padx=16)

        pbar = ttk.Progressbar(root, mode="determinate", maximum=NUM_BATCHES, variable=self.progress_var)
        pbar.pack(fill=tk.X)

        # Keep log and plot both visible using a vertical split panel.
        split = ttk.Panedwindow(root, orient=tk.VERTICAL)
        split.pack(fill=tk.BOTH, expand=True, pady=(10, 0))

        log_frame = ttk.LabelFrame(split, text="Log", padding=8)
        split.add(log_frame, weight=1)
        self.log_text = tk.Text(log_frame, height=12, wrap="word")
        self.log_text.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scr = ttk.Scrollbar(log_frame, orient="vertical", command=self.log_text.yview)
        scr.pack(side=tk.RIGHT, fill=tk.Y)
        self.log_text.configure(yscrollcommand=scr.set)

        plot_frame = ttk.LabelFrame(split, text="Live Plot", padding=8)
        split.add(plot_frame, weight=3)
        if MATPLOTLIB_AVAILABLE:
            self.fig = Figure(figsize=(8.5, 3.0), dpi=100)
            self.ax = self.fig.add_subplot(111)
            self.ax.set_title("Streaming Output")
            self.ax.set_xlabel("Sample index (window)")
            self.ax.set_ylabel("Value")
            self.ax.grid(True, alpha=0.25)
            self.line_x, = self.ax.plot([], [], label="x", linewidth=1.0)
            self.line_y_out, = self.ax.plot([], [], label="y_out", linewidth=1.2)
            self.line_y_ref, = self.ax.plot([], [], label="y_ref+1000", linewidth=1.0)
            self.ax.legend(loc="upper right")
            self.canvas = FigureCanvasTkAgg(self.fig, master=plot_frame)
            self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)
        else:
            ttk.Label(
                plot_frame,
                text="Matplotlib belum tersedia. Install: pip install matplotlib",
            ).pack(anchor="w")

        plot_ctl = ttk.Frame(root)
        plot_ctl.pack(fill=tk.X, pady=(6, 0))
        ttk.Button(plot_ctl, text="Clear Plot", command=self._clear_plot).pack(side=tk.LEFT)

    def append_log(self, msg: str) -> None:
        self.log_text.insert(tk.END, msg + "\n")
        self.log_text.see(tk.END)

    def refresh_ports(self) -> None:
        ports = sorted(serial.tools.list_ports.comports(), key=lambda p: p.device)
        values = [p.device for p in ports]
        self.port_combo["values"] = values
        if values and (self.port_var.get() not in values):
            self.port_var.set(values[0])
        self.append_log("[host] port list: " + (", ".join(values) if values else "(none)"))

    def pick_input(self) -> None:
        p = filedialog.askopenfilename(title="Pilih file input txt")
        if p:
            self.input_var.set(p)

    def pick_ref(self) -> None:
        p = filedialog.askopenfilename(title="Pilih file referensi txt")
        if p:
            self.ref_var.set(p)

    def pick_log(self) -> None:
        p = filedialog.asksaveasfilename(title="Simpan output log", defaultextension=".txt")
        if p:
            self.log_var.set(p)

    def start_stream(self) -> None:
        if self.worker_thread and self.worker_thread.is_alive():
            return

        port = self.port_var.get().strip()
        if not port:
            messagebox.showerror("Error", "Serial port belum dipilih")
            return

        try:
            baud = int(self.baud_var.get().strip())
        except ValueError:
            messagebox.showerror("Error", "Baud tidak valid")
            return

        input_path = Path(self.input_var.get().strip())
        ref_path = Path(self.ref_var.get().strip())
        log_path = Path(self.log_var.get().strip()) if self.log_var.get().strip() else None

        try:
            xs = load_scaled_ints(input_path)
            ys = load_scaled_ints(ref_path)
        except Exception as exc:
            messagebox.showerror("Error loading data", str(exc))
            return

        self.progress_var.set(0)
        self._clear_plot()
        self.status_var.set("Running")
        self.start_btn.configure(state=tk.DISABLED)
        self.stop_btn.configure(state=tk.NORMAL)
        self.stop_event.clear()
        self.append_log("[host] Start streaming")
        self.append_log("[host] catatan: tutup Serial Plotter/app lain yang buka port yang sama")
        self.append_log(f"[host] config: port={port}, baud={baud}")
        if baud != 115200:
            self.append_log("[warn] baud bukan 115200. Firmware default project ini biasanya 115200.")

        self.worker_thread = threading.Thread(
            target=self._worker,
            args=(port, baud, xs, ys, log_path, self.quiet_var.get()),
            daemon=True,
        )
        self.worker_thread.start()

    def stop_stream(self) -> None:
        self.stop_event.set()
        self.status_var.set("Stopping...")
        self.append_log("[host] Stop requested")

    def _worker(
        self,
        port: str,
        baud: int,
        xs: list[int],
        ys: list[int],
        log_path: Optional[Path],
        quiet_samples: bool,
    ) -> None:
        log_file = None
        try:
            if log_path is not None:
                log_file = log_path.open("w", encoding="utf-8")

            with serial.Serial(port, baud, timeout=0.2) as ser:
                time.sleep(0.5)
                ser.reset_input_buffer()
                self.msg_queue.put(("log", f"[host] port={port} baud={baud}"))
                self.msg_queue.put(("log", "[host] menunggu [READY] dari board..."))

                for b in range(NUM_BATCHES):
                    if self.stop_event.is_set():
                        raise InterruptedError("Stopped by user")

                    self._wait_ready(ser, b + 1)
                    s = b * BATCH_SIZE
                    e = s + BATCH_SIZE
                    for i in range(s, e):
                        ser.write(f"{xs[i]} {ys[i]}\n".encode("ascii"))
                    ser.flush()

                    sample_count = self._read_until_done(ser, b + 1, quiet_samples, log_file)
                    if sample_count != BATCH_SIZE:
                        self.msg_queue.put(("log", f"[warn] batch {b+1}: sample {sample_count}/{BATCH_SIZE}"))
                    self.msg_queue.put(("progress", str(b + 1)))

                self.msg_queue.put(("log", "[host] selesai 30 batch"))
        except InterruptedError as exc:
            self.msg_queue.put(("log", f"[host] {exc}"))
        except Exception as exc:
            self.msg_queue.put(("log", f"[error] {exc}"))
        finally:
            if log_file is not None:
                log_file.close()
            self.msg_queue.put(("done", ""))

    def _wait_ready(self, ser: serial.Serial, expected_batch: int) -> None:
        last_rx = time.time()
        next_ping = time.time() + 2.0
        while True:
            if self.stop_event.is_set():
                raise InterruptedError("Stopped by user")

            raw = ser.readline()
            if raw:
                last_rx = time.time()
                line = raw.decode("utf-8", errors="replace").strip()
                m = READY_RE.search(line)
                if m:
                    batch = int(m.group(1))
                    total = int(m.group(2))
                    if batch != expected_batch:
                        self.msg_queue.put(("log", f"[warn] board ready {batch}, expected {expected_batch}"))
                    if total != NUM_BATCHES:
                        self.msg_queue.put(("log", f"[warn] board total {total}, host {NUM_BATCHES}"))
                    self.msg_queue.put(("log", f"[host] ready batch {batch}/{total}"))
                    return
                if line:
                    self.msg_queue.put(("log", f"[board] {line}"))
            elif time.time() >= next_ping:
                self.msg_queue.put(("log", "[host] masih menunggu [READY]..."))
                next_ping = time.time() + 2.0
            elif time.time() - last_rx > 30.0:
                raise TimeoutError("Timeout menunggu [READY] (cek baud, firmware sudah jalan, dan port benar)")

    def _read_until_done(
        self,
        ser: serial.Serial,
        batch: int,
        quiet_samples: bool,
        log_file,
    ) -> int:
        sample_count = 0
        last_rx = time.time()
        next_ping = time.time() + 2.0

        while True:
            if self.stop_event.is_set():
                raise InterruptedError("Stopped by user")

            raw = ser.readline()
            if raw:
                last_rx = time.time()
                line = raw.decode("utf-8", errors="replace").strip()

                if DONE_RE.search(line):
                    self.msg_queue.put(("log", f"[host] selesai batch {batch}/{NUM_BATCHES}"))
                    return sample_count

                if line.startswith("ERR"):
                    self.msg_queue.put(("log", f"[board] {line}"))
                    continue

                parts = line.split()
                if len(parts) == 3:
                    sample_count += 1
                    try:
                        x = int(parts[0])
                        y_out = int(parts[1])
                        y_ref = int(parts[2])
                        self.msg_queue.put(("sample", (x, y_out, y_ref)))
                    except ValueError:
                        self.msg_queue.put(("log", f"[warn] sample parse gagal: {line}"))
                    if log_file is not None:
                        log_file.write(line + "\n")
                    if not quiet_samples:
                        self.msg_queue.put(("log", line))
                elif line:
                    self.msg_queue.put(("log", f"[board] {line}"))
            elif time.time() >= next_ping:
                self.msg_queue.put(("log", f"[host] menunggu output batch {batch}..."))
                next_ping = time.time() + 2.0
            elif time.time() - last_rx > 30.0:
                raise TimeoutError(
                    f"Timeout menunggu output batch {batch} (kemungkinan baud tidak cocok atau board belum running)"
                )

    def _drain_queue(self) -> None:
        while True:
            try:
                kind, payload = self.msg_queue.get_nowait()
            except queue.Empty:
                break

            if kind == "log":
                self.append_log(str(payload))
            elif kind == "progress":
                self.progress_var.set(int(str(payload)))
            elif kind == "sample":
                x, y_out, y_ref = payload  # type: ignore[misc]
                self._add_plot_point(int(x), int(y_out), int(y_ref))
            elif kind == "done":
                self.status_var.set("Idle")
                self.start_btn.configure(state=tk.NORMAL)
                self.stop_btn.configure(state=tk.DISABLED)

        self.after(100, self._drain_queue)

    def _clear_plot(self) -> None:
        self.sample_index = 0
        self.plot_update_counter = 0
        self.x_hist.clear()
        self.y_out_hist.clear()
        self.y_ref_hist.clear()

        if MATPLOTLIB_AVAILABLE:
            self.line_x.set_data([], [])
            self.line_y_out.set_data([], [])
            self.line_y_ref.set_data([], [])
            self.ax.relim()
            self.ax.autoscale_view()
            self.canvas.draw_idle()

    def _add_plot_point(self, x: int, y_out: int, y_ref: int) -> None:
        if not MATPLOTLIB_AVAILABLE:
            return

        self.sample_index += 1
        self.x_hist.append(x)
        self.y_out_hist.append(y_out)
        self.y_ref_hist.append(y_ref)

        n = len(self.x_hist)
        x_axis = list(range(max(0, self.sample_index - n), self.sample_index))
        self.line_x.set_data(x_axis, list(self.x_hist))
        self.line_y_out.set_data(x_axis, list(self.y_out_hist))
        self.line_y_ref.set_data(x_axis, list(self.y_ref_hist))

        self.ax.relim()
        self.ax.autoscale_view()

        # Limit redraw frequency to keep UI responsive.
        self.plot_update_counter += 1
        if self.plot_update_counter >= 8:
            self.canvas.draw_idle()
            self.plot_update_counter = 0


def main() -> int:
    app = FirStreamGui()
    app.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
