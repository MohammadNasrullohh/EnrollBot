const { GoogleGenAI } = require("@google/genai");
const fs = require("fs");
require("dotenv").config();

async function run() {
  const key = process.env.GEMINI_API_KEY;
  const ai = new GoogleGenAI({ apiKey: key });
  const text = "Hai juga, Bos! GemBot di sini, siap menemani kamu. Ada yang bisa GemBot bantu hari ini?";
  const response = await ai.models.generateContent({
    model: process.env.TTS_MODEL || "gemini-2.0-flash-exp",
    contents: [{
      parts: [{
        text: `Say in Indonesian with a cute cheerful small desktop robot voice, clear and not too fast: ${text}`,
      }],
    }],
    config: {
      responseModalities: ["AUDIO"],
      speechConfig: {
        voiceConfig: {
          prebuiltVoiceConfig: { voiceName: "Aoede" },
        },
      },
    },
  });
  const data = response.candidates?.[0]?.content?.parts?.find((part) => part.inlineData)?.inlineData?.data;
  if (data) {
    fs.writeFileSync("test_tts.wav", Buffer.from(data, "base64"));
    console.log("Saved test_tts.wav, length:", Buffer.from(data, "base64").length);
  } else {
    console.log("No audio data");
  }
}
run();
