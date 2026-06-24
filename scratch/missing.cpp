void dfPause();
void dfSetVolume(uint8_t volume);
void processDfPlayerCommand(String payload);
void setVoiceState(VoiceAssistantState state);
void beginVoiceCapture();
void finishVoiceCapture();
void serviceMicrophone();

void setupDFPlayer() {
  dfSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
  delay(800);
  dfReady = dfPlayer.begin(dfSerial, false, true);
  if (!dfReady) {
    Serial.println("DFPlayer ERR: cek VCC 5V, GND, TX->D6, RX->D1, SD /mp3/0001.mp3");
    if (oledReady) drawStatus("DFPLAYER ERR", "TX>D6 RX>D1", "cek SD 0001.mp3");
    delay(700);
    return;
  }

  dfPlayer.volume(dfVolume);
  dfPlaying = false;
  Serial.println("DFPlayer OK on TX D6 / RX D1");
  if (oledReady) {
    drawStatus("DFPLAYER OK", "/mp3/0001.mp3", "siap diputar");
    delay(600);
  }
}

void dfPlayTrack(uint16_t track);
void dfStop();
void dfPause();
void dfSetVolume(uint8_t volume);
void processDfPlayerCommand(String payload);
void setVoiceState(VoiceAssistantState state);
void beginVoiceCapture();
void finishVoiceCapture();
void serviceMicrophone();

void setupDFPlayer() {
  dfSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
  delay(800);
  dfReady = dfPlayer.begin(dfSerial, false, true);
  if (!dfReady) {
    Serial.println("DFPlayer ERR: cek VCC 5V, GND, TX->D6, RX->D1, SD /mp3/0001.mp3");
    if (oledReady) drawStatus("DFPLAYER ERR", "TX>D6 RX>D1", "cek SD 0001.mp3");
    delay(700);
    return;
  }

  dfPlayer.volume(dfVolume);
  dfPlaying = false;
  Serial.println("DFPlayer OK on TX D6 / RX D1");
  if (oledReady) {
    drawStatus("DFPLAYER OK", "/mp3/0001.mp3", "siap diputar");
    delay(600);
  }
}

void setVoiceState(VoiceAssistantState state);
void beginVoiceCapture();
void finishVoiceCapture();
void serviceMicrophone();

void setupDFPlayer() {
  dfSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
  delay(800);
  dfReady = dfPlayer.begin(dfSerial, false, true);
  if (!dfReady) {
    Serial.println("DFPlayer ERR: cek VCC 5V, GND, TX->D6, RX->D1, SD /mp3/0001.mp3");
    if (oledReady) drawStatus("DFPLAYER ERR", "TX>D6 RX>D1", "cek SD 0001.mp3");
    delay(700);
    return;
  }

  dfPlayer.volume(dfVolume);
  dfPlaying = false;
  Serial.println("DFPlayer OK on TX D6 / RX D1");
  if (oledReady) {
    drawStatus("DFPLAYER OK", "/mp3/0001.mp3", "siap diputar");
    delay(600);
  }
}

void processDfPlayerCommand(String payload);
void setVoiceState(VoiceAssistantState state);
void beginVoiceCapture();
void finishVoiceCapture();
void serviceMicrophone();

void setupDFPlayer() {
  dfSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
  delay(800);
  dfReady = dfPlayer.begin(dfSerial, false, true);
  if (!dfReady) {
    Serial.println("DFPlayer ERR: cek VCC 5V, GND, TX->D6, RX->D1, SD /mp3/0001.mp3");
    if (oledReady) drawStatus("DFPLAYER ERR", "TX>D6 RX>D1", "cek SD 0001.mp3");
    delay(700);
    return;
  }

  dfPlayer.volume(dfVolume);
  dfPlaying = false;
  Serial.println("DFPlayer OK on TX D6 / RX D1");
  if (oledReady) {
    drawStatus("DFPLAYER OK", "/mp3/0001.mp3", "siap diputar");
    delay(600);
  }
}

void updateTouch() {
  unsigned long now = millis();
  webSocket.loop();
  bool reading = digitalRead(TOUCH_PIN) == HIGH;
  bool audioNoiseWindow =
      audioStream != nullptr ||
      playing ||
      localTonePlaying ||
      dfPlaying ||
      now < touchMutedUntilMs;

  // Moving jumper wires and shared 3V3/GND noise can briefly pull a digital
  // touch module HIGH. Only block a new press; an accepted press can still
  // release normally.
  bool motionRecentlyActive =
      now - lastPhysicalMotionMs < 550UL ||
      shakeSmooth > 0.14f ||
      spinSmooth > 0.12f;
  if (!touchDown && motionRecentlyActive) {
    reading = false;
  }

  if (audioNoiseWindow) {
    reading = false; // Ignore touch during audio to prevent I2S/speaker noise false triggers
  }

  if (reading != lastTouchReading) {
    lastTouchReading = reading;
    touchChangedMs = now;
  }
  const unsigned long stableMs = reading ? 320UL : 120UL;
  if (now - touchChangedMs < stableMs) return;
  if (reading == touchDown) {
    if (touchDown && currentState == STATE_FACE && !voiceRecording &&
        now - touchDownMs >= VOICE_HOLD_MS) {
      beginVoiceCapture();
    }
    if (voiceRecording && now - voiceStartedMs >= VOICE_MAX_MS) {
      finishVoiceCapture();
      touchDown = false;
    }
    return;
  }

  touchDown = reading;
  if (touchDown) {
    touchDownMs = now;
    return;
  }

  unsigned long held = now - touchDownMs;
  if (held < 70UL) return;

  if (voiceRecording) {
    finishVoiceCapture();
  } else if (currentState == STATE_FACE) {
    if (held < VOICE_HOLD_MS) {
      currentState = STATE_MENU;
      menuCursor = 0;
    }
  } else if (currentState == STATE_MENU) {
    if (held > 600UL) {
      if (menuCursor == 0) currentState = STATE_GAMES;
      else if (menuCursor == 1) currentState = STATE_SENSOR;
      else if (menuCursor == 2) currentState = STATE_REMINDER;
      else if (menuCursor == 3) {
        currentState = STATE_MUSIC;
        musicCursor = 0;
      }
      else if (menuCursor == 4) enterDrawMode(true);
      else if (menuCursor == 5) currentState = STATE_FACE;
      if (currentState == STATE_GAMES) gamePhase = GAME_IDLE;
    } else {
      menuCursor = (menuCursor + 1) % 6;
    }
  } else if (currentState == STATE_MUSIC) {
    if (held > 600UL) {
      if (musicCursor == 0) {
        currentState = STATE_FACE;
        req_lovestory = true;
        req_song = 1;
      } else if (musicCursor == 1) {
        currentState = STATE_FACE;
        req_song = 2;
      } else if (musicCursor == 2) {
        currentState = STATE_FACE;
        req_song = 3;
      } else if (musicCursor == 3) {
        dfPlayTrack(1);
      } else {
        currentState = STATE_MENU;
        menuCursor = 3;
      }
    } else {
      musicCursor = (musicCursor + 1) % 5;
    }
  } else if (currentState == STATE_GAMES) {
    if (held > 600UL) {
      // Long hold = back to FACE as requested
      currentState = STATE_FACE;
      gamePhase = GAME_IDLE;
    } else {
      // Short tap = start game or hit
      if (gamePhase == GAME_IDLE || gamePhase == GAME_OVER) {
        initPingpong(true);
      } else if (gamePhase == GAME_PLAYING) {
        // Option: short tap to give the ball a random spin if it's on player's side
        if (ballX < 64.0f) {
           ballVY += (float)random(-10, 11) * 0.1f;
        }
      }
    }
  } else if (currentState == STATE_SENSOR || currentState == STATE_REMINDER ||
             currentState == STATE_DRAW || currentState == STATE_CHAT ||
             currentState == STATE_ASSISTANT) {
    if (held > 600UL) {
      currentState = STATE_FACE;
    } else {
      currentState = STATE_MENU;
    }
  }
}

void updateDHT() {
  if (!dhtReady) return;
  unsigned long now = millis();
  if (now - lastDhtMs < 2200UL) return;
  lastDhtMs = now;

  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    Serial.println("DHT read failed");
    return;
  }

  tempC = t;
  humPct = h;

  float hot = clampFloat((t - 28.0f) / 7.0f, 0.0f, 1.0f);
  float cold = clampFloat((22.0f - t) / 7.0f, 0.0f, 1.0f);
  float targetTempMood = hot - cold;
  float targetHumidMood = clampFloat((h - 68.0f) / 25.0f, 0.0f, 1.0f);

  tempMood += (targetTempMood - tempMood) * 0.35f;
  humidMood += (targetHumidMood - humidMood) * 0.35f;
  
  if (t > 31.0f) {
      pantUntilMs = now + 5000UL;
  }

  Serial.print("DHT T=");
  Serial.print(t, 1);
  Serial.print("C H=");
  Serial.print(h, 0);
  Serial.println("%");
  Serial.print("WiFi: ");
  Serial.print(WiFi.status() == WL_CONNECTED ? "OK " : "FAIL ");
  Serial.println(WiFi.localIP().toString());
  Serial.print("WS: ");
  Serial.println(webSocket.isConnected() ? "OK" : "FAIL");
}

