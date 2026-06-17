# OwiBot

OwiBot is an ESP32-based companion project with a TFT face UI, music playback, chatbot voice, and touch-driven menu navigation.

## Main targets

- `gembot`: ESP32 DOIT DevKit V1 build for the TFT-based bot
- `owibot_esp32_tft`: alternate TFT target
- `owibot_tft_only`: TFT-only test build
- `seeed_xiao_esp32c3`: audio stream target for the XIAO ESP32-C3

## What you need

- PlatformIO
- Node.js
- An ESP32 DOIT DevKit V1 for the main firmware
- Your WiFi and backend secrets in `include/secrets.h`

## Build and upload

```bash
pio run -e gembot
pio run -e gembot -t upload
```

## Local backend

The local web/audio server is `web_serial_server_new.js`.

Run it with:

```bash
node web_serial_server_new.js
```

Then open:

```text
http://127.0.0.1:3001
```

## Audio notes

- Music playback uses server-side audio streaming.
- `MBG`, `Love Story`, and `Test Max` depend on the backend serving the audio files.
- Chatbot voice also depends on the backend audio path and Gemini-related settings.

## Secrets

Copy `include/secrets.example.h` to `include/secrets.h` and fill in your WiFi and backend values.

## Project layout

- `src/gembot.cpp` main ESP32 DOIT firmware
- `web_serial_server_new.js` local web and audio server
- `include/` shared headers and secrets template
- `src/` other firmware variants and test sketches
