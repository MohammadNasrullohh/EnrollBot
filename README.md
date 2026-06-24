# OwiBot / GemBot (ESP32-S3)

OwiBot (juga dikenal sebagai GemBot) adalah proyek robot asisten/pendamping berbasis ESP32. Proyek ini dilengkapi dengan antarmuka TFT (UI wajah), pemutaran musik (I2S dan DFPlayer), chatbot suara, serta navigasi menu berbasis sentuhan.

Repositori ini secara utama dikonfigurasi untuk menggunakan **ESP32-S3 DevKitC-1** melalui kode program di `src/gembot2.cpp`.

## Struktur Kode & Pinout (Hardware)

Berikut adalah tabel penjelasan pinout, modul yang digunakan, dan alur data khusus untuk konfigurasi **ESP32-S3 DevKitC-1** (`src/gembot2.cpp`).

| Pin | Modul | Fungsi / Keterangan | Alur Data (Flow) |
|---|---|---|---|
| **13** | TFT Display (ILI9341) | MISO (Master In Slave Out) | Komunikasi dari display ke ESP32-S3. |
| **11** | TFT Display (ILI9341) | MOSI (Master Out Slave In) | Mengirim data layar (UI wajah) ke display. |
| **12** | TFT Display (ILI9341) | SCLK (Serial Clock) | Clock sinkronisasi SPI layar. |
| **10** | TFT Display (ILI9341) | CS (Chip Select) | Mengaktifkan/menonaktifkan modul layar di bus SPI. |
| **9** | TFT Display (ILI9341) | DC (Data/Command) | Penentu status data atau command untuk layar. |
| **14** | TFT Display (ILI9341) | RST (Reset) | Reset layar secara hardware. |
| **15** | Speaker (MAX98357A) | I2S_BCLK | Clock audio digital dari S3 ke amplifier. |
| **16** | Speaker (MAX98357A) | I2S_LRC | Left/Right Clock untuk audio speaker. |
| **17** | Speaker (MAX98357A) | I2S_DOUT | Output data audio stream / TTS dari S3 ke speaker. |
| **4** | Microphone (INMP441) | I2S_MIC_SCK | Clock sinkronisasi I2S untuk mikrofon. |
| **5** | Microphone (INMP441) | I2S_MIC_WS | Pemilih kata/channel untuk mikrofon. |
| **6** | Microphone (INMP441) | I2S_MIC_SD | Data audio masuk dari mikrofon ke S3 (menangkap suara user). |
| **18** | DFPlayer Mini | RX ESP32-S3 | Pin ini membaca data dari pin TX DFPlayer. |
| **21** | DFPlayer Mini | TX ESP32-S3 | ESP32-S3 mengirimkan perintah kontrol (play, stop) ke RX DFPlayer. |
| **7** | Touch Sensor (TTP223) | Input Digital | Input sentuhan user untuk bernavigasi ke menu utama. |
| **8** | Sensor Suhu (DHT22) | Data One-Wire | Mengambil data suhu & kelembapan ruangan. |
| **1** | Sensor Gerak (MPU6050)| I2C_SDA (Data) | Jalur I2C Data kustom untuk MPU6050. |
| **2** | Sensor Gerak (MPU6050)| I2C_SCL (Clock) | Jalur I2C Clock kustom untuk MPU6050. |


## Penjelasan Alur (Flow) Utama
- **Visual (Layar TFT)**: Menggunakan pustaka `TFT_eSPI` melalui antarmuka SPI. `TFT_MOSI` dan `TFT_SCLK` bertugas merender gambar ekspresi wajah (mata bergerak) dan menu. Konfigurasi pin didefinisikan pada `platformio.ini` khusus env ESP32-S3.
- **Audio Output (I2S vs UART)**:
  - **I2S (MAX98357A)** digunakan untuk memutar *Voice TTS* dari AI (balasan suara) secara streaming menggunakan pin `I2S_DOUT` dari ESP32-S3.
  - **UART (DFPlayer Mini)** digunakan khusus memutar musik MP3 dari SD Card secara independen (tidak membebani WiFi/RAM). ESP32-S3 mengirim perintah Serial ke DFPlayer (misal "Play Track 1").
- **Audio Input (INMP441)**: Mikrofon membaca suara user via I2S, dan data digital dikirim masuk ke memori ESP32-S3 melalui `I2S_MIC_SD`. Data ini dibuffer lalu dikirim melalui antarmuka WebSockets ke server lokal/backend Node.js untuk di-*speech-to-text* oleh AI.
- **Sensor (Touch, Suhu, & MPU)**:
  - **Touch** di-polling setiap loop. Jika ditahan, bot masuk mode *Listening* (mendengarkan suara). Jika di-tap ringan, akan mengembalikan ke menu utama.
  - **DHT22** dibaca secara asinkron setiap beberapa detik untuk mengupdate suhu di mode menu "Suhu".
  - **MPU6050** membaca kemiringan/posisi robot menggunakan bus I2C. Data ini dipakai untuk merespon jika robot sedang diputar/dijatuhkan.
