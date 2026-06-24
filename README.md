# OwiBot / GemBot

OwiBot (juga dikenal sebagai GemBot) adalah proyek robot asisten/pendamping berbasis ESP32. Proyek ini dilengkapi dengan antarmuka TFT (UI wajah), pemutaran musik (I2S dan DFPlayer), chatbot suara, serta navigasi menu berbasis sentuhan.

## Struktur Kode & Pinout (Hardware)

Proyek ini mendukung dua konfigurasi utama, yaitu untuk **ESP32 DOIT DevKit V1** (`src/gembot.cpp`) dan **ESP32-S3 DevKitC-1** (`src/gembot2.cpp`). Berikut adalah tabel penjelasan pinout, modul yang digunakan, dan alur datanya.

### 1. GemBot 1 (ESP32 DOIT DevKit V1 - `src/gembot.cpp`)

| Pin | Modul | Fungsi / Keterangan | Alur Data (Flow) |
|---|---|---|---|
| **19** | TFT Display (ILI9341) | MISO (Master In Slave Out) | Menerima sinyal balasan dari display ke ESP32. |
| **23** | TFT Display (ILI9341) | MOSI (Master Out Slave In) | Mengirim data piksel / gambar dari ESP32 ke display. |
| **18** | TFT Display (ILI9341) | SCLK (Serial Clock) | Sinyal sinkronisasi (clock) untuk pengiriman data layar SPI. |
| **5** | TFT Display (ILI9341) | CS (Chip Select) | Mengaktifkan modul display ketika di-LOW-kan. |
| **2** | TFT Display (ILI9341) | DC (Data/Command) | Memberi tahu layar apakah byte yang dikirim adalah Data atau Perintah. |
| **4** | TFT Display (ILI9341) | RST (Reset) | Mereset status layar secara hardware. |
| **26** | Speaker (MAX98357A) | I2S_BCLK (Bit Clock) | Memberikan sinyal clock data audio digital ke amplifier. |
| **25** | Speaker (MAX98357A) | I2S_LRC (Left/Right Clock) | Menentukan frame channel audio Kiri / Kanan. |
| **27** | Speaker (MAX98357A) | I2S_DOUT (Data Out) | Mengirim stream data audio digital (TTS/Web) untuk dimainkan speaker. |
| **32** | Microphone (INMP441) | I2S_MIC_SCK | Clock serial untuk mikrofon (input suara dari user). |
| **33** | Microphone (INMP441) | I2S_MIC_WS | Pemilih channel (Word Select) untuk mikrofon I2S. |
| **34** | Microphone (INMP441) | I2S_MIC_SD | Data audio digital dari mikrofon masuk ke memori ESP32. |
| **16** | DFPlayer Mini | RX ESP32 (UART2) | Menerima feedback/status dari DFPlayer TX. |
| **17** | DFPlayer Mini | TX ESP32 (UART2) | Mengirim perintah (play/pause/volume) ke DFPlayer RX. |
| **13** | Touch Sensor (TTP223) | Input Digital | User menyentuh sensor -> Sinyal HIGH masuk ke ESP32 -> Memicu aksi/menu. |
| **14** | Sensor Suhu (DHT22) | Data One-Wire | Sensor membaca suhu/kelembapan -> Data dikirim via satu pin ke ESP32. |
| **21** | Sensor Gerak (MPU6050)| I2C SDA (Data) | Jalur data I2C untuk membaca akselerometer & giroskop. |
| **22** | Sensor Gerak (MPU6050)| I2C SCL (Clock) | Jalur clock I2C untuk MPU6050. |

---

### 2. GemBot 2 (ESP32-S3 DevKitC-1 - `src/gembot2.cpp`)

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
- **Visual (Layar TFT)**: Menggunakan pustaka `TFT_eSPI` melalui antarmuka SPI. `TFT_MOSI` dan `TFT_SCLK` bertugas merender gambar ekspresi wajah (mata bergerak) dan menu. Konfigurasi pin didefinisikan pada `platformio.ini`.
- **Audio Output (I2S vs UART)**:
  - **I2S (MAX98357A)** digunakan untuk memutar *Voice TTS* dari AI (balasan suara) secara streaming menggunakan pin `I2S_DOUT` dari ESP.
  - **UART (DFPlayer Mini)** digunakan khusus memutar musik MP3 dari SD Card secara independen (tidak membebani WiFi/RAM ESP32). ESP32 mengirim perintah Serial ke DFPlayer (misal "Play Track 1").
- **Audio Input (INMP441)**: Mikrofon membaca suara user via I2S, dan data digital dikirim masuk ke memori ESP32 melalui `I2S_MIC_SD`. Data ini dibuffer lalu dikirim melalui antarmuka WebSockets ke server lokal/backend Node.js untuk di-*speech-to-text* oleh AI.
- **Sensor (Touch, Suhu, & MPU)**:
  - **Touch** di-polling setiap loop. Jika ditahan, bot masuk mode *Listening* (mendengarkan suara). Jika di-tap ringan, akan mengembalikan ke menu utama.
  - **DHT22** dibaca secara asinkron setiap beberapa detik untuk mengupdate suhu di mode menu "Suhu".
  - **MPU6050** membaca kemiringan/posisi robot menggunakan bus I2C. Data ini dipakai untuk merespon jika robot sedang diputar/dijatuhkan.
