# Devandri Suherman - 2026
# Bandung Institute of Technology
# GUI Python untuk mengirim batch data ke board PYNQ-Z1, menerima output FIR
#, dan memplot hasilnya secara real-time. Juga menghitung statistik error selama proses.
# Pastikan untuk install customtkinter, pyserial, matplotlib, dan numpy sebelum menjalankan
# pip install customtkinter pyserial matplotlib numpy

#!/usr/bin/env python3
import customtkinter as ctk
from tkinter import filedialog, messagebox
import serial
import serial.tools.list_ports
import threading
import time
import re
from pathlib import Path
from collections import deque

from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure

# --- Konfigurasi Tema Utama (Light Mode) ---
ctk.set_appearance_mode("Light")
ctk.set_default_color_theme("blue")

class FIRGuiSender(ctk.CTk):
    def __init__(self):
        super().__init__()

        self.title("FIR Batch Data Sender - Devandri Suherman")
        self.geometry("1050x750")

        # Variabel Logika
        self.BATCH_SIZE = 60
        self.NUM_BATCHES = 30
        self.FIR_TAPS = 16
        self.SCALE_FACTOR = 1000.0
        self.READY_RE = re.compile(r"\[READY\]\s+batch\s+(\d+)/(\d+)")
        self.DONE_RE = re.compile(r"\[DONE\]\s+batch\s+(\d+)/(\d+)")
        self.is_running = False
        
        # Buffer plot sekarang disesuaikan persis dengan ukuran batch
        self.plot_max_points = self.BATCH_SIZE
        
        self.plot_x = deque(maxlen=self.plot_max_points)
        self.plot_y_out = deque(maxlen=self.plot_max_points)
        self.plot_y_ref = deque(maxlen=self.plot_max_points)
        self.plot_err = deque(maxlen=self.plot_max_points)
        self.plot_spike_x = deque(maxlen=self.plot_max_points)
        self.plot_spike_y = deque(maxlen=self.plot_max_points)
        
        self.plot_sample_index = 0
        self.total_samples = self.BATCH_SIZE * self.NUM_BATCHES
        self.err_count = 0
        self.err_abs_sum = 0.0
        self.err_sq_sum = 0.0
        self.err_max_abs = 0.0
        self.spike_count = 0
        self.spike_eval_count = 0
        self.spike_ignore_head = self.FIR_TAPS - 1

        self.setup_ui()
        self.refresh_ports()

    def setup_ui(self):
        self.grid_columnconfigure(0, weight=0)
        self.grid_columnconfigure(1, weight=1)
        self.grid_rowconfigure(0, weight=1)

        # --- SIDEBAR (Settings) ---
        self.sidebar = ctk.CTkFrame(self, width=260, corner_radius=0, fg_color="#f8f9fa")
        self.sidebar.grid(row=0, column=0, sticky="nsew")
        self.sidebar.grid_propagate(False)
        self.sidebar.grid_rowconfigure(99, weight=1)
        
        ctk.CTkLabel(self.sidebar, text="SERIAL SETTINGS", font=ctk.CTkFont(size=16, weight="bold"), text_color="#2c3e50").pack(pady=(25, 15))

        # Port Selection
        ctk.CTkLabel(self.sidebar, text="Port:", text_color="#34495e", font=ctk.CTkFont(weight="bold")).pack(anchor="w", padx=20)
        self.port_combo = ctk.CTkComboBox(self.sidebar, values=[])
        self.port_combo.pack(pady=(0, 10), padx=20, fill="x")
        
        ctk.CTkButton(self.sidebar, text="Refresh Ports", command=self.refresh_ports, height=28, fg_color="#3498db", hover_color="#2980b9").pack(padx=20, fill="x")

        # Baud Rate
        ctk.CTkLabel(self.sidebar, text="Baud Rate:", text_color="#34495e", font=ctk.CTkFont(weight="bold")).pack(anchor="w", padx=20, pady=(15,0))
        self.baud_combo = ctk.CTkComboBox(self.sidebar, values=["9600", "38400", "115200", "230400", "921600"])
        self.baud_combo.set("38400")
        self.baud_combo.pack(pady=(0, 10), padx=20, fill="x")

        ctk.CTkFrame(self.sidebar, height=2, fg_color="#e0e0e0", corner_radius=0).pack(pady=20, padx=20, fill="x")

        # File Inputs
        ctk.CTkButton(self.sidebar, text="Select Input TXT", fg_color="transparent", text_color="#2980b9", border_color="#2980b9", border_width=1.5, hover_color="#ecf0f1", command=self.select_input).pack(pady=5, padx=20, fill="x")
        self.input_label = ctk.CTkLabel(self.sidebar, text="No file selected", font=ctk.CTkFont(size=11), text_color="#7f8c8d", wraplength=200)
        self.input_label.pack(padx=20)

        ctk.CTkButton(self.sidebar, text="Select Ref TXT", fg_color="transparent", text_color="#2980b9", border_color="#2980b9", border_width=1.5, hover_color="#ecf0f1", command=self.select_ref).pack(pady=(15,5), padx=20, fill="x")
        self.ref_label = ctk.CTkLabel(self.sidebar, text="No file selected", font=ctk.CTkFont(size=11), text_color="#7f8c8d", wraplength=200)
        self.ref_label.pack(padx=20)

        # Control Buttons
        self.start_btn = ctk.CTkButton(self.sidebar, text="START STREAM", fg_color="#2ecc71", hover_color="#27ae60", height=35, font=ctk.CTkFont(weight="bold"), command=self.start_process)
        self.start_btn.pack(side="bottom", pady=(5, 20), padx=20, fill="x")
        
        self.stop_btn = ctk.CTkButton(self.sidebar, text="STOP", fg_color="#e74c3c", hover_color="#c0392b", height=35, font=ctk.CTkFont(weight="bold"), command=self.stop_process, state="disabled")
        self.stop_btn.pack(side="bottom", pady=5, padx=20, fill="x")

        # --- MAIN CONTENT ---
        self.main_frame = ctk.CTkFrame(self, fg_color="white")
        self.main_frame.grid(row=0, column=1, sticky="nsew", padx=20, pady=20)
        self.main_frame.grid_columnconfigure(0, weight=1)
        self.main_frame.grid_rowconfigure(1, weight=0)
        self.main_frame.grid_rowconfigure(2, weight=8)
        self.main_frame.grid_rowconfigure(3, weight=0)

        # Progress Section
        self.progress_label = ctk.CTkLabel(self.main_frame, text="Status: Ready", font=ctk.CTkFont(size=14, weight="bold"), text_color="#2c3e50")
        self.progress_label.grid(row=0, column=0, sticky="w", pady=(0, 5))

        self.pb = ctk.CTkProgressBar(self.main_frame, progress_color="#3498db")
        self.pb.set(0)
        self.pb.grid(row=0, column=0, sticky="e", padx=(150, 0))

        self.err_stats_label = ctk.CTkLabel(self.main_frame, text="Err MAE: - | RMSE: - | Max|e|: -", font=ctk.CTkFont(size=12, weight="bold"), text_color="#d35400")
        self.err_stats_label.grid(row=0, column=0, padx=(220, 0), sticky="w")

        self.sample_count_label = ctk.CTkLabel(self.main_frame, text=f"Samples: 0/{self.total_samples}", font=ctk.CTkFont(size=12), text_color="#7f8c8d")
        self.sample_count_label.grid(row=0, column=0, sticky="w", padx=(620, 0))

        self.log_toolbar = ctk.CTkFrame(self.main_frame, fg_color="transparent")
        self.log_toolbar.grid(row=1, column=0, sticky="ew", pady=(10, 0))
        self.log_toolbar.grid_columnconfigure(0, weight=1)

        self.spike_threshold_var = ctk.StringVar(value="50")
        self.spike_count_label = ctk.CTkLabel(self.log_toolbar, text="Spikes: 0 (0.00%)", font=ctk.CTkFont(size=12, weight="bold"), text_color="#8e44ad")
        self.spike_count_label.pack(side="left")
        
        self.spike_threshold_entry = ctk.CTkEntry(self.log_toolbar, textvariable=self.spike_threshold_var, width=80, border_color="#bdc3c7")
        self.spike_threshold_entry.pack(side="left", padx=(10, 4))
        
        self.spike_threshold_hint = ctk.CTkLabel(self.log_toolbar, text="|e| threshold", font=ctk.CTkFont(size=11), text_color="#7f8c8d")
        self.spike_threshold_hint.pack(side="left")

        self.copy_log_btn = ctk.CTkButton(self.log_toolbar, text="Copy Log", width=110, fg_color="#ecf0f1", text_color="#2c3e50", hover_color="#bdc3c7", command=self.copy_log)
        self.copy_log_btn.pack(side="right", padx=(8, 0))

        self.clear_log_btn = ctk.CTkButton(self.log_toolbar, text="Clear Log", width=110, fg_color="#ecf0f1", text_color="#2c3e50", hover_color="#bdc3c7", command=self.clear_log)
        self.clear_log_btn.pack(side="right")

        # --- LIVE PLOT PANEL ---
        self.plot_frame = ctk.CTkFrame(self.main_frame, fg_color="white", border_width=1, border_color="#e0e0e0")
        self.plot_frame.grid(row=2, column=0, sticky="nsew", pady=(15, 0))
        self.plot_frame.grid_columnconfigure(0, weight=1)
        self.plot_frame.grid_rowconfigure(0, weight=1)

        plot_title = ctk.CTkLabel(self.plot_frame, text="Live Serial Plot", font=ctk.CTkFont(size=14, weight="bold"), text_color="#2c3e50")
        plot_title.grid(row=0, column=0, sticky="w", padx=10, pady=(10, 0))

        self.fig = Figure(figsize=(8.5, 5.0), dpi=100)
        self.fig.subplots_adjust(left=0.08, right=0.95, top=0.9, bottom=0.1, hspace=0.45)
        
        self.ax_sig = self.fig.add_subplot(211)
        self.ax_err = self.fig.add_subplot(212, sharex=self.ax_sig)

        self.fig.patch.set_facecolor("white")
        self.ax_sig.set_facecolor("white")
        self.ax_err.set_facecolor("white")
        
        self.ax_sig.tick_params(colors="#2c3e50", labelsize=9)
        self.ax_err.tick_params(colors="#2c3e50", labelsize=9)
        
        for spine in self.ax_sig.spines.values():
            spine.set_color("#bdc3c7")
        for spine in self.ax_err.spines.values():
            spine.set_color("#bdc3c7")
            
        self.ax_sig.grid(True, color="#ecf0f1", linestyle='-', linewidth=1)
        self.ax_err.grid(True, color="#ecf0f1", linestyle='-', linewidth=1)

        self.ax_sig.set_title("x / y_out / y_ref", color="#2c3e50", fontsize=11, fontweight="bold")
        self.ax_sig.set_ylabel("Value", color="#2c3e50", fontsize=10)
        
        self.ax_err.set_title("Error = y_out - y_ref", color="#2c3e50", fontsize=11, fontweight="bold") 
        self.ax_err.set_ylabel("Error", color="#2c3e50", fontsize=10)
        self.ax_err.set_xlabel("Sample index", color="#2c3e50", fontsize=10)

        (self.line_x,) = self.ax_sig.plot([], [], color="#3498db", linewidth=1.5, label="x")
        (self.line_y_out,) = self.ax_sig.plot([], [], color="#e67e22", linewidth=1.5, label="y_out")
        (self.line_y_ref,) = self.ax_sig.plot([], [], color="#27ae60", linewidth=1.5, label="y_ref")
        (self.line_err,) = self.ax_err.plot([], [], color="#e74c3c", linewidth=1.5, label="error")
        (self.line_spike,) = self.ax_err.plot([], [], linestyle="", marker="x", color="#8e44ad", markersize=6, label="spike", markeredgewidth=2)
        self.line_thr_pos = self.ax_err.axhline(50.0, color="#8e44ad", linewidth=1.0, linestyle=":", alpha=0.85, label="+threshold")
        self.line_thr_neg = self.ax_err.axhline(-50.0, color="#8e44ad", linewidth=1.0, linestyle=":", alpha=0.85, label="-threshold")
        
        self.ax_err.axhline(0.0, color="#7f8c8d", linewidth=1.2, linestyle="--", alpha=0.8)
        
        self.ax_sig.legend(loc="upper right", frameon=True, facecolor="white", edgecolor="#bdc3c7", fontsize=9)
        self.ax_err.legend(loc="upper right", frameon=True, facecolor="white", edgecolor="#bdc3c7", fontsize=9)

        self.plot_canvas = FigureCanvasTkAgg(self.fig, master=self.plot_frame)
        self.plot_canvas.get_tk_widget().grid(row=1, column=0, sticky="nsew", padx=10, pady=10)

        # Console / Log
        self.console = ctk.CTkTextbox(self.main_frame, font=ctk.CTkFont(family="Courier", size=12), wrap="word", fg_color="#f8f9fa", text_color="#2c3e50", border_width=1, border_color="#e0e0e0")
        self.console.grid(row=3, column=0, sticky="nsew", pady=(15, 0))
        self.console.configure(height=90) 

    # --- LOGIC METHODS ---

    def refresh_ports(self):
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.port_combo.configure(values=ports)
        if ports:
            self.port_combo.set(ports[0])
            self.log(f"[host] ports detected: {', '.join(ports)}")
        else:
            self.log("[host] no serial ports detected")

    def select_input(self):
        path = filedialog.askopenfilename(filetypes=[("Text files", "*.txt")])
        if path: self.input_label.configure(text=Path(path).name); self.input_path = path

    def select_ref(self):
        path = filedialog.askopenfilename(filetypes=[("Text files", "*.txt")])
        if path: self.ref_label.configure(text=Path(path).name); self.ref_path = path

    def log(self, message):
        self.console.insert("end", f"{message}\n")
        self.console.see("end")

    def split_board_chunk(self, raw_line):
        normalized = raw_line.replace("\\r\\n", "\n").replace("\r\n", "\n")
        return [ln.strip() for ln in normalized.split("\n") if ln.strip()]

    def clear_log(self):
        self.console.delete("1.0", "end")

    def copy_log(self):
        text = self.console.get("1.0", "end").strip()
        if not text:
            self.log("[host] log kosong, tidak ada yang dicopy")
            return
        self.clipboard_clear()
        self.clipboard_append(text)
        self.log("[host] log copied to clipboard")

    def get_spike_threshold(self):
        try:
            threshold = abs(float(self.spike_threshold_var.get().strip()))
            if threshold == 0:
                return 1.0
            return threshold
        except ValueError:
            return 50.0

    def prepare_batch_plot(self, start_idx, end_idx):
        """Mempersiapkan grafik untuk batch baru dengan mengunci sumbu X dan mereset garis"""
        self.plot_x.clear()
        self.plot_y_out.clear()
        self.plot_y_ref.clear()
        self.plot_err.clear()
        self.plot_spike_x.clear()
        self.plot_spike_y.clear()

        self.line_x.set_data([], [])
        self.line_y_out.set_data([], [])
        self.line_y_ref.set_data([], [])
        self.line_err.set_data([], [])
        self.line_spike.set_data([], [])
        
        # Kunci Sumbu X sesuai index batch saat ini
        self.ax_sig.set_xlim(start_idx, end_idx)
        self.ax_err.set_xlim(start_idx, end_idx)
        
        self.plot_canvas.draw_idle()

    def add_plot_sample(self, x_value, y_out_value, y_ref_plus_value):
        y_ref_value = y_ref_plus_value
        err_value = y_out_value - y_ref_value

        self.plot_sample_index += 1
        self.plot_x.append(x_value)
        self.plot_y_out.append(y_out_value)
        self.plot_y_ref.append(y_ref_value)
        self.plot_err.append(err_value)

        self.err_count += 1
        abs_e = abs(err_value)
        self.err_abs_sum += abs_e
        self.err_sq_sum += float(err_value) * float(err_value)
        if abs_e > self.err_max_abs:
            self.err_max_abs = abs_e

        sample_count = len(self.plot_x)
        x_axis = list(range(self.plot_sample_index - sample_count + 1, self.plot_sample_index + 1))
        self.line_x.set_data(x_axis, list(self.plot_x))
        self.line_y_out.set_data(x_axis, list(self.plot_y_out))
        self.line_y_ref.set_data(x_axis, list(self.plot_y_ref))
        self.line_err.set_data(x_axis, list(self.plot_err))

        threshold = self.get_spike_threshold()
        self.line_thr_pos.set_ydata([threshold, threshold])
        self.line_thr_neg.set_ydata([-threshold, -threshold])
        sample_pos_in_batch = ((self.plot_sample_index - 1) % self.BATCH_SIZE) + 1
        should_check_spike = sample_pos_in_batch > self.spike_ignore_head
        if should_check_spike:
            self.spike_eval_count += 1
            if abs(err_value) >= threshold:
                self.spike_count += 1
                self.plot_spike_x.append(self.plot_sample_index)
                self.plot_spike_y.append(err_value)
        self.line_spike.set_data(list(self.plot_spike_x), list(self.plot_spike_y))

        # Hanya autoscale sumbu Y agar sumbu X yang dikunci tidak bergeser
        self.ax_sig.relim()
        self.ax_sig.autoscale_view(scalex=False, scaley=True)
        self.ax_err.relim()
        self.ax_err.autoscale_view(scalex=False, scaley=True)
        self.plot_canvas.draw_idle()

        if self.err_count > 0:
            mae = self.err_abs_sum / self.err_count
            rmse = (self.err_sq_sum / self.err_count) ** 0.5
            self.err_stats_label.configure(text=f"Err MAE: {mae:.3f} | RMSE: {rmse:.3f} | Max|e|: {self.err_max_abs:.3f}")
            if self.spike_eval_count > 0:
                spike_rate = (self.spike_count / self.spike_eval_count) * 100.0
            else:
                spike_rate = 0.0
            self.spike_count_label.configure(text=f"Spikes: {self.spike_count} ({spike_rate:.2f}%)")
        self.sample_count_label.configure(text=f"Samples: {self.err_count}/{self.total_samples}")

    def clear_plot(self):
        """Mereset total statistik & menyiapkan layar sebelum proses dimulai (dari titik 0)"""
        self.plot_sample_index = 0
        self.err_count = 0
        self.err_abs_sum = 0.0
        self.err_sq_sum = 0.0
        self.err_max_abs = 0.0
        self.spike_count = 0
        self.spike_eval_count = 0
        
        self.err_stats_label.configure(text="Err MAE: - | RMSE: - | Max|e|: -")
        self.sample_count_label.configure(text=f"Samples: 0/{self.total_samples}")
        self.spike_count_label.configure(text="Spikes: 0 (0.00%)")
        
        self.prepare_batch_plot(0, self.BATCH_SIZE)

    def load_data(self, path):
        vals = []
        with open(path, "r") as f:
            for line in f:
                s = line.strip()
                if s: vals.append(int(round(float(s) * self.SCALE_FACTOR)))
        return vals

    def start_process(self):
        if not hasattr(self, 'input_path') or not hasattr(self, 'ref_path'):
            messagebox.showerror("Error", "Pilih file input dan referensi dulu!")
            return

        if not self.port_combo.get().strip():
            messagebox.showerror("Error", "Pilih serial port dulu!")
            return
        
        self.is_running = True
        self.start_btn.configure(state="disabled")
        self.stop_btn.configure(state="normal")
        self.console.delete("1.0", "end")
        self.clear_plot()
        self.log(f"[host] input : {self.input_path}")
        self.log(f"[host] ref   : {self.ref_path}")
        self.log(f"[host] port  : {self.port_combo.get()} @ {self.baud_combo.get()}")
        self.log("[host] scale : x1 (txt float -> UART integer)")
        self.log("[host] catatan: tutup Serial Plotter/app lain yang memakai port yang sama")
        
        threading.Thread(target=self.serial_worker, daemon=True).start()

    def stop_process(self):
        self.is_running = False
        self.log("!!! STOPPING PROCESS...")

    def serial_worker(self):
        try:
            self.log("[host] loading txt data...")
            xs = self.load_data(self.input_path)
            ys = self.load_data(self.ref_path)
            self.log(f"[host] loaded {len(xs)} input samples and {len(ys)} ref samples")
            
            port = self.port_combo.get()
            baud = int(self.baud_combo.get())

            self.log(f"[host] opening serial port {port} @ {baud}")
            with serial.Serial(port, baud, timeout=0.5) as ser:
                time.sleep(1) 
                ser.reset_input_buffer()
                self.log("[host] waiting for board [READY]...")
                
                for b in range(self.NUM_BATCHES):
                    if not self.is_running: break
                    
                    self.progress_label.configure(text=f"Batch {b+1}/{self.NUM_BATCHES}")
                    self.pb.set((b + 1) / self.NUM_BATCHES)
                    self.log(f"[host] sending batch {b+1}/{self.NUM_BATCHES}")

                    s, e = b * self.BATCH_SIZE, (b + 1) * self.BATCH_SIZE
                    
                    # Bersihkan layar plotter khusus untuk rentang batch yang akan datang
                    self.after(0, self.prepare_batch_plot, s, e)

                    for i in range(s, e):
                        ser.write(f"{xs[i]} {ys[i]}\n".encode("ascii"))
                    ser.flush()

                    self.log(f"[host] batch {b+1} sent, waiting board output...")

                    got_done = False
                    idle_ticks = 0
                    while self.is_running:
                        raw_line = ser.readline().decode("utf-8", errors="replace").strip()
                        if not raw_line:
                            idle_ticks += 1
                            if idle_ticks % 8 == 0:
                                self.log(f"[host] still waiting batch {b+1}/{self.NUM_BATCHES}...")
                            continue
                        idle_ticks = 0

                        lines = self.split_board_chunk(raw_line)
                        for line in lines:
                            parts = line.split()
                            if len(parts) == 3:
                                try:
                                    x_val = int(parts[0])
                                    y_out_val = int(parts[1])
                                    y_ref_val = int(parts[2])
                                    self.after(0, self.add_plot_sample, x_val, y_out_val, y_ref_val)
                                except ValueError:
                                    pass

                            self.log(f"[Board] {line}")

                            if "[DONE]" in line:
                                got_done = True
                                break

                        if got_done:
                            break

                    if not got_done and self.is_running:
                        self.log(f"[warn] batch {b+1} finished loop without [DONE]")
                
                self.log("✅ SELESAI SEMUA BATCH")
        except Exception as e:
            self.log(f"❌ ERROR: {str(e)}")
            self.log("[hint] cek baud (default setup ini 38400), port benar, dan board firmware sudah running")
        finally:
            self.is_running = False
            self.start_btn.configure(state="normal")
            self.stop_btn.configure(state="disabled")

if __name__ == "__main__":
    app = FIRGuiSender()
    app.mainloop()