# Dokumentasi Implementasi Register 16-bit pada Bus dan Pengujian RW

## 1. Ringkasan
Dokumen ini menjelaskan implementasi register 16-bit memory-mapped pada user bus SoC CROC (target PYNQ-Z1), program software untuk baca/tulis register tersebut, serta hasil validasi di board.

Hasil akhir:
- Register 16-bit berhasil ditambahkan pada bus user domain.
- Akses write/read dari software berjalan benar.
- Hasil baca selalu sama dengan data yang ditulis (pola deterministik dan pseudo-random).

## 2. Tujuan
- Menambahkan satu register 16-bit pada peripheral user domain.
- Menyediakan API software untuk read/write register.
- Memverifikasi fungsi register melalui pengujian serial di hardware.

## 3. Lingkungan Sistem
- Board: PYNQ-Z1
- Debug interface: J-Link + OpenOCD + GDB
- Toolchain: riscv32-unknown-elf-gcc
- Build software: `make clean && make FPGA=1 all`
- Serial monitor: minicom 115200 8N1 (no flow control)

## 4. Arsitektur Alamat Register
### 4.1 Base Address User ALU
- `USER_ALU_BASE_ADDR = 0x20001000`

### 4.2 Register Baru
- Nama: `USER_REG16`
- Offset: `0x14` (20 desimal)
- Alamat absolut: `0x20001014`
- Lebar data efektif: 16-bit (`[15:0]`)
- Format data read pada bus 32-bit: `{16'h0000, user_reg16_q}`

## 5. Alur Program Berkesinambungan (End-to-End)
Bagian ini menjelaskan keterkaitan antar program dan file dari awal debug sampai validasi final.

### 5.1 Tahap A - Bring-up UART dan validasi jalur serial
Program yang dipakai:
- `sw/uart_freq_scan.c`
- `sw/serial_hello.c`

Tujuan:
- Menentukan clock efektif UART di hardware (terukur 25 MHz untuk komunikasi stabil 115200 baud).
- Memastikan serial output aktif dan dapat dipantau di minicom.

Dampak ke tahap berikutnya:
- Menetapkan konfigurasi frekuensi software agar semua program test berikutnya dapat menampilkan output dengan benar.

### 5.2 Tahap B - Implementasi register 16-bit di bus (RTL)
File utama RTL:
- `croc/rtl/user_domain/simple_alu.sv`
- `croc_pynq-z1/rtl/user_domain/simple_alu.sv`

Tujuan:
- Menambahkan storage register 16-bit yang dipetakan ke offset 0x14.
- Menambahkan logic write/read melalui bus OBI subordinate.

Dampak ke tahap berikutnya:
- Software dapat mengakses register baru melalui `USER_ALU_BASE_ADDR + 0x14`.

### 5.3 Tahap C - API software untuk akses register baru
File API:
- `sw/lib/inc/simple_alu.h`

Isi perubahan:
- Menambahkan `USER_REG16_OFFSET`.
- Menambahkan helper:
  - `user_reg16_write(uint16_t value)`
  - `user_reg16_read(void)`

Dampak:
- Program uji bisa fokus pada skenario test, bukan detail akses register low-level.

### 5.4 Tahap D - Program uji RW bus 16-bit
Program uji utama:
- `sw/rw_bus16.c`

Alur test:
1. Inisialisasi UART.
2. Menjalankan pola deterministik.
3. Menjalankan pola pseudo-random (LFSR).
4. Membandingkan WR vs RD untuk tiap transaksi.
5. Mencetak status per transaksi dan ringkasan final.

Output final yang diminta:
- Rekap akhir format: `IDX | WR | RD | OK/FAIL`.

### 5.5 Tahap E - Peningkatan formatter debug
File formatter:
- `sw/lib/src/print.c`
- `sw/lib/inc/print.h`

Tujuan:
- Menambah kemampuan `printf` agar debugging lebih mudah dan format output laporan lebih rapi.
- Dukungan: `%x`, `%u`, `%d`, `%c`, `%s`, `%%`, dan zero-padding sederhana (contoh `%04x`).

Dampak:
- Output serial lebih konsisten dan mudah dianalisis.

## 6. Daftar File yang Diubah dan Fungsinya
| No | File | Jenis | Perubahan | Dampak |
|---|---|---|---|---|
| 1 | `croc/rtl/user_domain/simple_alu.sv` | RTL | Tambah register 16-bit + mode offset 0x14 untuk write/read | Register baru aktif di desain RTL utama |
| 2 | `croc_pynq-z1/rtl/user_domain/simple_alu.sv` | RTL | Sinkronisasi perubahan register 16-bit seperti RTL utama | Konsistensi RTL di repo kerja |
| 3 | `sw/lib/inc/simple_alu.h` | SW API | Tambah `USER_REG16_OFFSET`, `user_reg16_write`, `user_reg16_read` | Antarmuka software ke register baru |
| 4 | `sw/rw_bus16.c` | SW Test | Program validasi RW deterministik + pseudo-random + rekap tabel | Verifikasi fungsi register di hardware |
| 5 | `sw/lib/src/print.c` | SW Lib | Perluasan formatter `printf` | Output debug lebih baik |
| 6 | `sw/lib/inc/print.h` | SW Lib Header | Update deklarasi `printf(const char *fmt, ...)` | Kompatibilitas API formatter baru |
| 7 | `sw/config.h` | SW Config | Penyesuaian frekuensi efektif (`TB_FREQUENCY`) sesuai hasil bring-up UART | Serial stabil saat test |
| 8 | `sw/uart_freq_scan.c` | SW Diagnostic | Program scan divisor/clock UART | Menentukan konfigurasi serial yang valid |
| 9 | `sw/serial_hello.c` | SW Diagnostic | Program sanity check serial runtime | Validasi jalur serial sebelum test bus |

## 7. Detail Implementasi RTL Register 16-bit
Implementasi pada `simple_alu.sv` meliputi:
- Penambahan state/address selector `UserReg16 = 20`.
- Path write:
  - Saat write ke mode `UserReg16`, data `wdata[15:0]` disimpan ke register internal.
- Path read:
  - Saat read di mode `UserReg16`, bus mengembalikan nilai register pada bit `[15:0]`.

## 8. Detail Implementasi Software Test
### 8.1 Pola deterministik
Data uji:
- `0x0000`, `0x1234`, `0xABCD`, `0xFFFF`, `0x00F0`, `0x5AA5`

### 8.2 Pola pseudo-random
- Menggunakan LFSR 16-bit untuk menghasilkan 8 data tambahan.

### 8.3 Kriteria lulus
- Untuk setiap indeks transaksi: `RD == WR`.
- Ringkasan akhir seluruh indeks menunjukkan status `OK`.

## 9. Prosedur Build dan Eksekusi
### 9.1 Build software
```bash
cd /home/devan/SoC/croc_pynq-z1/sw
make clean && make FPGA=1 all
```

Output penting:
- `bin/rw_bus16.elf`
- `bin/rw_bus16.dump`
- `bin/rw_bus16.hex`

### 9.2 Run via OpenOCD + GDB
Contoh urutan command:
```gdb
target remote localhost:3335
monitor reset halt
load
set {unsigned int}0x03000000 = 0x10000000
set {unsigned int}0x03000004 = 1
set $pc = 0x10000000
continue
```

### 9.3 Pantau serial
```bash
minicom -o -D /dev/ttyUSB0 -b 115200
```

## 10. Hasil Pengujian (Ringkas)
- Seluruh pola deterministik: `OK`.
- Seluruh pola pseudo-random: `OK`.
- Rekap akhir `IDX | WR | RD | OK/FAIL` menunjukkan tidak ada mismatch.

Kesimpulan:
- Register 16-bit pada offset `0x14` berfungsi benar untuk operasi read/write melalui bus.

## 11. Kendala dan Solusi Selama Integrasi
1. Serial gibberish di fase awal
- Penyebab: mismatch clock/divisor UART.
- Solusi: tuning konfigurasi frekuensi software berdasarkan hasil scan.

2. Output debug sempat terpotong/acak saat format belum cocok
- Penyebab: formatter `printf` awal sangat terbatas.
- Solusi: perluasan fitur formatter dan penyederhanaan format output test.

3. False diagnosis hang pada fase awal
- Penyebab: penggunaan delay berbasis timer interrupt dapat mengaburkan titik macet.
- Solusi: gunakan checkpoint logging berurutan dan delay ringan berbasis busy-loop saat isolasi masalah.

## 12. Kesimpulan Akhir
Tugas "membuat register 16-bit di bus dan membuat program RW bus tersebut" telah selesai, tervalidasi di hardware, dan didukung dokumentasi implementasi end-to-end.

Deliverables final:
- RTL register 16-bit memory-mapped di offset `0x14`
- API software read/write register 16-bit
- Program uji `rw_bus16.c` dengan verifikasi deterministik + pseudo-random
- Rekap output akhir format `IDX | WR | RD | OK/FAIL`

## 13. Saran Pengembangan
- Menambahkan register map user peripheral ke dokumentasi resmi SoC.
- Menambahkan regression test otomatis untuk RW register user domain.
- Menambahkan dukungan formatter lanjutan (opsional) jika kebutuhan logging makin kompleks.

## 14. Diagram Arsitektur Hubungan Program dan File
Diagram berikut menunjukkan hubungan antar file yang diubah, alur build, alur debug, dan alur validasi output serial.

```mermaid
flowchart LR
  subgraph HW[RTL / Hardware Domain]
    A1[croc/rtl/user_domain/simple_alu.sv]
    A2[croc_pynq-z1/rtl/user_domain/simple_alu.sv]
    A3[Register USER_REG16 @ 0x20001014]
    A1 -->|Implementasi register| A3
    A2 -->|Sinkronisasi RTL lokal| A3
  end

  subgraph SWAPI[Software API Domain]
    B1[sw/lib/inc/simple_alu.h]
    B2[user_reg16_write/read]
    B1 -->|Expose API akses| B2
  end

  subgraph SWAPP[Application / Test Domain]
    C1[sw/rw_bus16.c]
    C2[Pola Deterministik]
    C3[Pola Pseudo-random LFSR]
    C4[Rekap IDX | WR | RD | OK/FAIL]
    C1 -->|Jalankan test set tetap| C2
    C1 -->|Jalankan stress random| C3
    C2 -->|Hasil transaksi| C4
    C3 -->|Hasil transaksi| C4
  end

  subgraph UARTDBG[UART / Debug Domain]
    D1[sw/config.h]
    D2[sw/lib/src/uart.c]
    D3[sw/uart_freq_scan.c]
    D4[sw/serial_hello.c]
    D5[sw/lib/src/print.c + sw/lib/inc/print.h]
    D1 -->|Clock/baud config| D2
    D3 -->|Kalibrasi frekuensi UART| D1
    D4 -->|Sanity check TX/RX| D2
    D5 -->|Format output test| C1
    D5 -->|Format output serial app| D4
  end

  subgraph FLOW[Build & Run Flow]
    E1[make clean && make FPGA=1 all]
    E2[bin/rw_bus16.elf]
    E3[OpenOCD + GDB load/continue]
    E4[minicom 115200]
    E1 -->|Generate binary| E2
    E2 -->|Load ke SRAM target| E3
    E3 -->|Monitor output runtime| E4
  end

  A3 -->|Diakses lewat MMIO| B2
  B2 -->|Dipanggil oleh test app| C1
  A2 -->|Menjadi sumber build bitstream| E1
  D2 -->|Transport UART untuk log| C1
  C4 -->|Ringkasan ditampilkan| E4
```

## 15. Panduan Lanjutan (Kalau Menambah Register Baru Lagi)
Untuk pengembangan berikutnya, urutan kerja yang direkomendasikan:

1. Tambah mapping register di RTL (`simple_alu.sv`) dengan offset baru.
2. Tambah API software di `sw/lib/inc/simple_alu.h`.
3. Tambah/ubah test app di `sw/rw_bus16.c` (atau buat app baru).
4. Build software, load via GDB, dan validasi serial.
5. Catat hasil dengan format rekap yang sama agar mudah dibandingkan antar versi.
