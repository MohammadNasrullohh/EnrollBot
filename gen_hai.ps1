Add-Type -AssemblyName System.Speech
$synth = New-Object System.Speech.Synthesis.SpeechSynthesizer
$synth.SelectVoiceByHints('Female')
$synth.SetOutputToWaveFile('hai.wav')
$synth.Speak('Hai')
$synth.Dispose()
