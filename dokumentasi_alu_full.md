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

## 11. Detail Sangat Lengkap Program yang Berkaitan dan Perubahannya

Bagian ini menjawab pertanyaan utama laporan: **program mana saja yang berkaitan langsung dengan proyek ALU dan diubah apa saja**.

### 11.1 Kelompok A - Program Inti ALU (wajib untuk ALU berjalan)

1. `sw/alu_full.c`
   - Peran: aplikasi interaktif utama untuk demo ALU melalui UART.
   - Perubahan/fungsi penting yang terlihat di kode:
     - Menambahkan antarmuka interaktif berbasis serial: pilih operator, input A/B, tampilkan hasil.
     - Menambahkan parser angka manual `parse_int32()` agar input serial robust untuk tanda `+/-` dan digit.
     - Menambahkan `clamp_to_q8_8()` untuk membatasi input ke rentang Q8.8 yang aman (`-128..127`).
     - Menambahkan utilitas konversi/tampilan fixed-point:
       - `q16_16_to_milli()`
       - `print_q16_16()`
     - Menambahkan pembanding hasil HW vs SW dengan status final `MATCH/MISMATCH`.
     - Menonaktifkan operasi `/` dari menu utama (menu hanya `+ - *`) untuk menjaga demo stabil.
     - Menyediakan opsi debug checkpoint melalui macro `ALU_DEBUG_CHECKPOINTS`.
   - Dampak: file ini adalah pusat validasi akhir karena langsung menampilkan konsistensi hasil antara hardware dan software.

2. `sw/lib/inc/simple_alu.h`
   - Peran: API low-level untuk mengakses register ALU user-domain.
   - Perubahan/fungsi penting:
     - Definisi helper `pack_inputs(num1, num2)` untuk packing dua operand 16-bit ke 32-bit payload.
     - Mapping register operasi ALU:
       - offset `0` add, `4` sub, `8` mult, `12` div, baca hasil di `16`.
     - Penambahan akses register uji tambahan:
       - `USER_REG16_OFFSET = 20`
       - `ADD2_INPUT_OFFSET = 24`
       - `ADD2_OUTPUT_OFFSET = 28`
     - Penambahan helper `user_reg16_write/read()` dan `add2_input_write/read()` + `add2_output_read()`.
   - Dampak: ini menjadi jembatan utama software ke hardware ALU; tanpa pemetaan offset benar, hasil ALU tidak mungkin valid.

3. `sw/lib/src/simple_alu.c`
   - Peran: golden reference software untuk pembanding hasil hardware.
   - Perubahan/fungsi penting:
     - `add_sw()` dan `sub_sw()` mengembalikan hasil Q16.16 dengan shift `<< 8`.
     - `mult_sw()` mempertahankan hasil Q16.16 dari perkalian Q8.8.
     - `div_sw()` memakai sentinel `0xCAFEF00D` saat pembagi nol agar perilaku SW konsisten dengan RTL.
   - Dampak: memastikan pembandingan HW vs SW fair dan deterministik.

4. `rtl/user_domain/simple_alu.sv`
   - Peran: implementasi hardware ALU di user-domain (OBI slave).
   - Perubahan/fungsi penting:
     - Menyediakan mode register ALU dan register uji tambahan:
       - `Add=0`, `Substract=4`, `Multiply=8`, `Divide=12`, `ReadResult=16`
       - `UserReg16=20`, `Add2Input=24`, `Add2Output=28`
     - Implementasi fungsi fixed-point:
       - `add`: `(in1 + in2) <<< 8`
       - `sub`: `(in1 - in2) <<< 8`
       - `mult`: `in1 * in2`
       - `div`: `(in1 <<< 8) / in2`
     - Penanganan bagi nol: mengembalikan `32'shCAFEF00D`.
     - Menambahkan jalur register `add2` untuk validasi bus/register sederhana.
   - Dampak: ini adalah sumber hasil HW yang dibandingkan pada `alu_full.c`.

### 11.2 Kelompok B - Program Stabilitas Serial/UART (kunci agar ALU interaktif bisa dipakai)

5. `sw/lib/src/uart.c`
   - Peran: inisialisasi dan operasi driver UART.
   - Perubahan/fungsi penting:
     - Penyesuaian rumus divisor saat kompilasi `FPGA`:
       - `#define UART_DIVISOR(freq, baud) (((freq) + ((baud) << 3)) / ((baud) << 4))`
       - Tujuan: mengurangi mismatch baudrate antara implementasi FPGA dan ekspektasi software.
     - `UART_MODEM_CONTROL_REG_OFFSET` diset `0x00` (tanpa RTS/CTS autoflow) agar cocok adapter USB-UART 3-wire.
   - Dampak: komunikasi serial menjadi lebih stabil untuk input/output program ALU.

6. `sw/config.h`
   - Peran: parameter clock/baud untuk firmware.
   - Isi konfigurasi saat ini:
     - `TB_FREQUENCY = 95000000`
     - `TB_BAUDRATE = 115200`
   - Catatan laporan:
     - Walau konstanta ini `115200`, pengujian lapangan menunjukkan sesi interaktif stabil di terminal `38400` pada setup tertentu.
   - Dampak: parameter ini mempengaruhi divisor UART dan keterbacaan log ALU.

7. `sw/serial_hello.c`
   - Peran: program diagnosis serial sederhana untuk validasi TX/RX sebelum menjalankan ALU.
   - Perubahan/fungsi penting:
     - Menambahkan output periodik `Hello, world!`.
     - Menambahkan uji RX echo (`uart_read_ready()` + baca byte + tampilkan balik).
     - Menambahkan indikator LED aktivitas TX/RX melalui GPIO (`LD0_TX`, `LD1_RX`).
   - Dampak: memisahkan masalah serial dari masalah logika ALU.

8. `sw/uart_freq_scan.c`
   - Peran: memindai kandidat divisor UART untuk identifikasi clock efektif sistem.
   - Perubahan/fungsi penting:
     - Menambahkan tabel probe rentang tinggi (`70MHz` s.d. `120MHz`) dengan divisor 38-65.
     - Menampilkan label tiap probe berulang agar mudah dilihat di terminal.
     - Menggunakan LED GPIO sebagai indikator state scan.
   - Dampak: membantu menemukan kombinasi baud/divisor yang menghasilkan output terbaca.

9. `sw/lib/inc/print.h` dan `sw/lib/src/print.c`
   - Peran: utilitas `printf` ringan untuk output diagnostik.
   - Perubahan/fungsi penting:
     - Mendukung `%d`, `%u`, `%x`, `%c`, `%s`, `%%`.
     - Mendukung optional width dan zero-padding (contoh `%04x`) yang penting untuk format heksadesimal register.
   - Dampak: membuat laporan serial ALU konsisten dan mudah dibaca.

### 11.3 Kelompok C - Program Uji Tambahan Register/BUS (mendukung verifikasi integrasi ALU)

10. `sw/rw_bus16.c`
   - Peran: uji read/write register 16-bit pada offset `USER_REG16`.
   - Perubahan/fungsi penting:
     - Menulis pola tetap dan pola pseudo-random (LFSR), lalu membaca balik dan cek `OK/FAIL`.
     - Menyimpan log uji per kasus dan merangkum hasil akhir.
   - Dampak: memverifikasi jalur akses register sebelum menarik kesimpulan dari uji ALU.

11. `sw/add2_ip_test.c`
   - Peran: uji jalur register `ADD2_INPUT/ADD2_OUTPUT` pada hardware.
   - Perubahan/fungsi penting:
     - Menulis beberapa pattern 16-bit, baca input-back dan output (`+2`), lalu verifikasi.
     - Menampilkan format hex dan desimal untuk audit.
   - Dampak: validasi integrasi OBI/regfile yang juga dipakai modul ALU.

12. `sw/btn_led_ctrl.c` dan `sw/blink.c`
   - Peran: sanity check GPIO/timing dasar board.
   - Dampak tidak langsung:
     - Membantu memastikan board clock/periferal dasar hidup normal sebelum debug ALU.

### 11.4 Kelompok D - Script Bring-up yang Membuat ALU Bisa Dijalankan Stabil di Board

13. `xilinx/scripts/upload_program.sh`
   - Perubahan/fungsi penting:
     - Menambahkan urutan write register boot/fetch:
       - `0x03000000 = 0x10000000` (boot address SRAM)
       - `0x03000004 = 1` (fetch enable)
     - Menetapkan port OpenOCD via `OCD_GDB_PORT` (default `3335`).
   - Dampak: program ELF ALU benar-benar dieksekusi dari SRAM setelah load.

14. `sw/gdb_run_with_fetch.gdb`
   - Peran: script GDB non-interaktif yang menstandardkan urutan load.
   - Isi penting:
     - `monitor reset halt`, `load`, set boot address/fetch enable, set `$pc`, `continue`.
   - Dampak: mengurangi gagal eksekusi akibat langkah manual yang terlewat.

15. `xilinx/scripts/connect_openocd.sh`
   - Perubahan/fungsi penting:
     - Default ke profil kompatibel `openocd2.jlink.tcl`.
     - Menangani stale process OpenOCD di port debug (3333/3335/4444/4446/6666/6667).
   - Dampak: sesi debug/upload lebih stabil dan tidak bentrok port.

16. `xilinx/scripts/openocd2.jlink.tcl` dan `xilinx/scripts/openocd.jlink.tcl`
   - Perubahan/fungsi penting:
     - Menetapkan target RISC-V di port `3335`.
     - Profil kompatibel memakai `-defer-examine` + retry examine/halt.
     - Kecepatan adaptor konservatif (`adapter speed 50`) pada profil kompatibel untuk stabilitas attach.
   - Dampak: memperbesar peluang GDB attach/load sukses saat debugging ALU.

17. `xilinx/program_pynqz1.sh`
   - Peran: wrapper pemrograman bitstream ke FPGA.
   - Perubahan/fungsi penting:
     - Auto-discovery environment Vivado.
     - Validasi file bitstream dan parameter hardware server/device.
   - Dampak: memastikan bitstream yang memuat RTL ALU benar-benar terprogram ke board.

18. `xilinx/src/pynqz1.xdc`
   - Peran: constraints pin dan timing board PYNQ-Z1.
   - Poin penting untuk ALU interaktif:
     - UART diarahkan ke PMODB:
       - `uart_tx_o -> W14`
       - `uart_rx_i -> Y14` + `PULLUP true`
   - Dampak: jalur serial fisik sesuai wiring adapter UART yang dipakai saat uji ALU.

### 11.5 Catatan Keterkaitan Antar Program (rantai sebab-akibat)

Agar ALU interaktif benar-benar bekerja di board, rangkaiannya adalah:

1. RTL ALU benar (`rtl/user_domain/simple_alu.sv`) + bitstream terprogram (`xilinx/program_pynqz1.sh`).
2. Jalur UART benar secara fisik (`xilinx/src/pynqz1.xdc`) dan driver (`sw/lib/src/uart.c`).
3. Program termuat dan core jalan (`xilinx/scripts/upload_program.sh` atau `sw/gdb_run_with_fetch.gdb`).
4. Aplikasi interaktif ALU dijalankan (`sw/alu_full.c`) dengan referensi SW (`sw/lib/src/simple_alu.c`).
5. Program diagnosis (`sw/serial_hello.c`, `sw/uart_freq_scan.c`, `sw/rw_bus16.c`, `sw/add2_ip_test.c`) dipakai untuk isolasi masalah bila ada mismatch.

Dengan kata lain, keberhasilan ALU bukan dari satu file saja, tetapi dari kombinasi **RTL + firmware + UART + script bring-up + validasi bertahap**.

## 12. Ringkasan Jawaban untuk Laporan

Jika ditulis singkat di bagian kesimpulan laporan:

- Program inti yang membuat ALU bekerja: `sw/alu_full.c`, `sw/lib/inc/simple_alu.h`, `sw/lib/src/simple_alu.c`, `rtl/user_domain/simple_alu.sv`.
- Program pendukung yang membuat pengujian ALU stabil: `sw/lib/src/uart.c`, `sw/config.h`, `sw/serial_hello.c`, `sw/uart_freq_scan.c`, `xilinx/scripts/upload_program.sh`, `xilinx/scripts/connect_openocd.sh`, `xilinx/scripts/openocd2.jlink.tcl`, `xilinx/src/pynqz1.xdc`.
- Program verifikasi integrasi tambahan: `sw/rw_bus16.c`, `sw/add2_ip_test.c`, `sw/btn_led_ctrl.c`, `sw/blink.c`.
