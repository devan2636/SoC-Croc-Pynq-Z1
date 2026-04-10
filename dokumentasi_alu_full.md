# Dokumentasi Program ALU Full Interaktif (Versi Laporan Final)

## 1. Ringkasan
Program ALU Full digunakan untuk menguji operasi fixed-point melalui UART interaktif pada target PYNQ-Z1.

Status akhir pengujian:
- Operasi tambah `+`, kurang `-`, dan kali `*` tervalidasi `MATCH` antara HW dan SW.
- Operasi bagi `/` belum dijadikan mode utama karena hasil HW belum stabil pada bitstream saat ini.
- Jalur serial sudah stabil untuk penggunaan harian dengan terminal pada `38400` baud.

## 2. Tujuan
- Menyediakan aplikasi uji ALU interaktif berbasis serial.
- Membandingkan hasil hardware dan software secara langsung.
- Menjadi dasar validasi fungsi user-domain ALU pada SoC.

## 3. Lokasi File Terkait
- Program utama: `sw/alu_full.c`
- Uji cepat non-interaktif: `sw/alu_test.c`
- API ALU: `sw/lib/inc/simple_alu.h`
- Referensi SW ALU: `sw/lib/src/simple_alu.c`
- Konfigurasi serial/clock SW: `sw/config.h`
- RTL ALU utama (yang dipakai flow bitstream): `../croc/rtl/user_domain/simple_alu.sv`

## 4. Alur Program
1. Inisialisasi UART.
2. Menampilkan menu operasi.
3. Membaca operator dari serial.
4. Membaca operand A dan B sebagai integer.
5. Clamp input ke rentang Q8.8 (`-128..127`) dan konversi ke format Q8.8.
6. Menjalankan fungsi HW dan SW sesuai operator.
7. Menampilkan raw hex, nilai desimal, dan status `MATCH/MISMATCH`.

## 5. Catatan Format Fixed-Point
- Input disimpan sebagai `int16_t` Q8.8.
- Operasi `+`, `-`, `*` ditampilkan sebagai Q16.16.
- Operasi `/` secara arsitektur masih membutuhkan perhatian tambahan pada implementasi hardware saat ini.

## 6. Hasil Uji Lapangan (April 2026)

### 6.1 Stabilitas Serial
- Setelah bring-up, komunikasi stabil dicapai dengan terminal `minicom -b 38400`.
- Pengujian `serial_hello` berhasil menampilkan string dan echo byte RX.

Contoh output:
```text
Hello, world!
Type a key to test RX echo...
RX byte:
1
```

### 6.2 Validasi ALU Interaktif
Contoh hasil aktual:
- `+` dengan input `1` dan `1` -> `MATCH`
- `-` dengan input `1` dan `4` -> `MATCH`
- `*` dengan input `7` dan `8` -> `MATCH`

Contoh output:
```text
A=7 (Q8.8=0x0700), B=8 (Q8.8=0x0800)
HW raw=0x00380000 dec=56.000
SW raw=0x00380000 dec=56.000
STATUS: MATCH
```

### 6.3 Catatan Operasi Bagi
- Pada uji `alu_test`, hasil `div` HW belum konsisten dengan SW.
- Sementara ini operasi bagi tidak diprioritaskan untuk demo utama.

## 7. Build dan Eksekusi

### 7.1 Build Software
```bash
cd /home/devan/SoC/croc_pynq-z1/sw
make clean && make FPGA=1 all
```

### 7.2 Generate dan Program Bitstream
```bash
cd /home/devan/SoC/croc_pynq-z1/xilinx
./implement_pynqz1.sh
./program_pynqz1.sh out/croc.pynqz1.bit
```

### 7.3 Start OpenOCD
```bash
cd /home/devan/SoC/croc_pynq-z1/xilinx/scripts
./connect_openocd.sh
```

### 7.4 Load ELF
```bash
cd /home/devan/SoC/croc_pynq-z1/sw
riscv32-unknown-elf-gdb bin/alu_full.elf \
  -ex "target extended-remote localhost:3335" \
  -ex "monitor halt" \
  -ex "load" \
  -ex "set {unsigned int}0x03000000 = 0x10000000" \
  -ex "set {unsigned int}0x03000004 = 1" \
  -ex "set $pc = _start" \
  -ex "continue"
```

### 7.5 Monitor Serial
```bash
minicom -o -D /dev/ttyUSB0 -b 38400
```

## 8. Kriteria Keberhasilan Versi Ini
- Serial interaktif responsif.
- Operasi `+`, `-`, `*` memberikan `MATCH` pada test case utama.
- Input invalid ditolak dengan aman.
- Operasi `/` dicatat sebagai known limitation untuk revisi RTL berikutnya.

## 9. Known Limitation
- Timing implementation bitstream saat ini belum ideal untuk semua path komputasi berat.
- Divider hardware belum dijadikan acuan final untuk laporan hasil utama.

## 10. Rekomendasi Lanjutan
- Jadikan `+`, `-`, `*` sebagai scope demo final.
- Jika `/` ingin diaktifkan penuh, lakukan optimasi RTL/pipelining divider dan timing closure ulang.
- Untuk laporan, lampirkan log serial hasil `MATCH` sebagai bukti validasi fungsi inti.
