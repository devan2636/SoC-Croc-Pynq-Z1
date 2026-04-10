# SoC-Croc-Pynq-Z1

## Deskripsi Proyek

Proyek ini merupakan implementasi **CROC (Configurable RISC-V Open Core)** SoC pada papan FPGA **Pynq Z1** yang dikembangkan berbasis platform Xilinx Zynq-7000. CROC adalah sebuah desain System-on-Chip (SoC) open-source berbasis arsitektur RISC-V 32-bit yang dirancang oleh Integrated Systems Laboratory (IIS) ETH Zürich untuk keperluan penelitian dan edukasi.

## Tentang CROC

CROC merupakan SoC RISC-V berukuran kecil yang cocok diimplementasikan pada FPGA berkapasitas rendah hingga menengah. SoC ini mencakup:

- **Prosesor**: Core RISC-V 32-bit (RV32IMC) berbasis RI5CY/CV32E40P
- **Memori**: Instruksi RAM (IRAM) dan Data RAM (DRAM) terintegrasi
- **Peripheral**: UART, GPIO, Timer, dan antarmuka SPI
- **Interkoneksi**: Bus OBI (Open Bus Interface) untuk komunikasi antar modul
- **Debug**: Dukungan JTAG untuk debugging dan pemrograman

## Platform Target: Pynq Z1

**Pynq Z1** adalah papan pengembangan berbasis Xilinx Zynq-7020 APSoC (All Programmable System-on-Chip) yang menggabungkan:

- **Processing System (PS)**: Dual-core ARM Cortex-A9 @ 650 MHz
- **Programmable Logic (PL)**: FPGA Artix-7 dengan 85K Logic Cells
- **Memori**: 512 MB DDR3 RAM
- **Antarmuka**: USB, Ethernet, HDMI, MicroSD, Arduino/Raspberry Pi header
- **Framework**: PYNQ (Python Productivity for Zynq) untuk pemrograman berbasis Python

## Tujuan Proyek

1. Mengimplementasikan desain CROC SoC ke dalam Programmable Logic (PL) Pynq Z1
2. Memvalidasi fungsionalitas CROC pada platform FPGA nyata
3. Menyediakan lingkungan pengembangan dan pengujian SoC berbasis RISC-V
4. Memanfaatkan framework PYNQ untuk mempermudah interaksi antara PS (ARM) dan PL (CROC RISC-V)

## Struktur Proyek

```
SoC-Croc-Pynq-Z1/
├── README.md          # Dokumentasi proyek
├── rtl/               # Desain RTL (Register Transfer Level) CROC SoC
├── constraints/       # File XDC untuk pin assignment Pynq Z1
├── scripts/           # Script Tcl untuk build Vivado
├── sw/                # Perangkat lunak dan firmware untuk CROC
└── notebooks/         # Jupyter Notebook PYNQ untuk demonstrasi
```

## Cara Penggunaan

### Prasyarat

- **Perangkat Keras**: Papan Pynq Z1
- **Perangkat Lunak**:
  - Xilinx Vivado 2022.1 atau lebih baru (untuk sintesis dan implementasi)
  - PYNQ image v3.x untuk Pynq Z1
  - RISC-V GNU Toolchain (untuk kompilasi firmware)

### Build Bitstream

1. Buka Xilinx Vivado
2. Jalankan script build:
   ```tcl
   source scripts/build.tcl
   ```
3. Lakukan sintesis, implementasi, dan generate bitstream
4. Transfer file `.bit` ke Pynq Z1

### Menjalankan di Pynq Z1

1. Boot Pynq Z1 dengan PYNQ image
2. Upload bitstream ke papan melalui Jupyter Notebook:
   ```python
   from pynq import Overlay
   overlay = Overlay("croc_top.bit")
   ```
3. Interaksi dengan CROC SoC melalui antarmuka PYNQ

## Referensi

- [CROC SoC - ETH Zürich IIS](https://github.com/pulp-platform/croc)
- [PYNQ Framework](http://www.pynq.io/)
- [Pynq Z1 Board](https://www.tulembedded.com/FPGA/ProductsPYNQ-Z1.html)
- [RISC-V Specification](https://riscv.org/specifications/)

## Lisensi

Proyek ini mengikuti lisensi open-source sesuai dengan lisensi CROC SoC dari ETH Zürich IIS. Silakan merujuk pada file `LICENSE` untuk informasi lebih lanjut.