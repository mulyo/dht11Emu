


# DHT11 Emulator

Proyek ini adalah emulator sensor DHT11 yang dirancang untuk mensimulasikan keluaran sinyal sensor DHT11 secara real-time tanpa memerlukan sensor fisik asli. Tujuan utamanya adalah membantu pengujian firmware, QA, dan validasi perangkat yang berinteraksi dengan sensor suhu dan kelembapan.

## Tujuan Proyek

Pada banyak sistem embedded, pengujian perangkat dengan sensor lingkungan sering kali menghadapi keterbatasan saat ingin mengecek kondisi ekstrem seperti:

- suhu batas dengan nilai tertentu
- kelembapan ekstrem
- kondisi data error atau korup

Sensor fisik tidak selalu mudah dipakai untuk mengecek skenario seperti ini, terutama saat diperlukan pengujian otomatis, cepat, dan berulang. Oleh karena itu, emulator ini dibuat agar perilaku DHT11 dapat diprogram secara fleksibel sesuai kebutuhan pengujian.

## Latar Belakang

- **Memecahkan Masalah Nyata (Real-World Problem Solving):** Menguji skenario ekstrem (seperti suhu overheat 70°C atau suhu di bawah 0°C) pada sensor fisik memang repot dan tidak praktis. Proyek ini membuktikan pemahanan pain point dalam proses hardware testing/QA.
- **Penguasaan Protokol Low-Level:** DHT11 menggunakan protokol custom single-bus dengan timing mikrodetik (μs). Menirukan (emulate) sinyal ini membutuhkan pemahaman mendalam tentang timer/counter, interrupts, serta GPIO bit-banging pada mikrokontroler.
- **Fitur Simulasi yang Luas:** Bisa mensimulasikan kondisi yang sulit didapat dari sensor asli, seperti:
  - Injeksi data suhu/kelembapan ekstrem secara instan.
  - Simulasi kondisi error (seperti checksum error, timeout, atau data korup) untuk menguji keandalan firmware perangkat utama (DUT - Device Under Test).

## Konsep Kerja

Emulator ini bekerja dengan cara:

1. Menyediakan nilai suhu dan kelembapan yang dapat diubah secara manual melalui potensiometer dan switch.
2. Menghasilkan sinyal DHT11 yang menyerupai protokol sensor asli pada pin keluaran tertentu.
3. Menggunakan interrupt dan timer untuk meniru timing yang sangat presisi.
4. Menyediakan tampilan status pada OLED 128x64 agar parameter simulasi mudah dilihat.

Dengan pendekatan ini, perangkat yang diuji (DUT) dapat berinteraksi dengan emulator seolah-olah ia sedang menerima data dari sensor DHT11 sungguhan.

## Fitur Utama

- Emulasi DHT11 berbasis perangkat keras
- Simulasi mode suhu atau kelembapan
- Nilai bisa diubah dari potensiometer
- Simulasi checksum OK/error
- Tampilan parameter di OLED
- Tanpa dependency library eksternal untuk driver utama

## Keunggulan Proyek

- Sangat cocok untuk pengujian firmware dan QA
- Memungkinkan rekayasa skenario error yang sulit dilakukan dengan sensor asli
- Mengajarkan protokol low-level dan timing mikrokontroler secara praktis
- Dapat dikembangkan untuk simulasi sensor lain di masa depan

## Catatan Penting

Proyek ini dirancang untuk kebutuhan testing dan emulasi sensor, bukan untuk menggantikan sensor DHT11 asli dalam lingkungan produksi. Fungsinya lebih kepada validasi perangkat lunak dan otomatisasi pengujian saat kondisi sensor nyata sulit atau tidak praktis dihasilkan.

## Kesimpulan

DHT11 Emulator ini merupakan contoh penerapan pengujian hardware dan protokol embedded secara nyata. Selain bermanfaat untuk validasi alat, proyek ini juga menunjukkan kemampuan dalam memahami timing sinyal, interrupt, dan komunikasi low-level yang sering menjadi tantangan dalam pengembangan sistem berbasis mikrokontroler.
