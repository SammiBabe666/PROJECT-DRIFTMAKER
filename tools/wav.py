#!/usr/bin/env python3
"""raw s16 mono 44.1 kHz -> WAV (stdlib only): python3 tools/wav.py IN.raw OUT.wav"""
import sys, wave
data = open(sys.argv[1], 'rb').read()
w = wave.open(sys.argv[2], 'wb'); w.setnchannels(1); w.setsampwidth(2); w.setframerate(44100); w.writeframes(data); w.close()
