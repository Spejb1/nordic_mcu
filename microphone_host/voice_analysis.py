"""
voice_analysis.py — opens .wav audio file and portrays content

Usage:
    python voice_analysis.py <file_name>
"""

import sys
from pathlib import Path
import numpy as np
import matplotlib.pyplot as plt
from scipy.io import wavfile

def main():
    if len(sys.argv) != 2:
        print("Usage: python voice_analysis.py <filename.wav>")
        sys.exit(1)

    filename = Path(sys.argv[1])

    if not filename.is_file():
        print("No such file found")
        sys.exit(1)

    fs, y = wavfile.read(filename)

    # Convert to float in [-1, 1] if needed
    if np.issubdtype(y.dtype, np.integer):
        max_int = np.iinfo(y.dtype).max
        y = y.astype(np.float64) / max_int
    else:
        y = y.astype(np.float64)

    # Handle stereo/multichannel by converting to mono
    if y.ndim > 1:
        y = np.mean(y, axis=1)

    print(f"File: {filename}")
    print(f"Sample rate: {fs} Hz")
    print(f"Duration: {len(y) / fs:.2f} s")
    print(f"Max abs amplitude: {np.max(np.abs(y)):.4f} (1.0 = full scale)")

    t = np.arange(len(y)) / fs

    fig, axes = plt.subplots(2, 1, figsize=(12, 6), num="voice inspection")

    # Full waveform
    axes[0].plot(t, y, linewidth=0.8)
    axes[0].set_xlabel("Time (s)")
    axes[0].set_ylabel("Amplitude")
    axes[0].set_title("Full waveform")
    axes[0].set_ylim([-1, 1])
    axes[0].grid(True)

    # Spectrogram
    axes[1].specgram(y, NFFT=256, Fs=fs, noverlap=200, cmap="viridis")
    axes[1].set_title("Spectrogram")
    axes[1].set_xlabel("Time (s)")
    axes[1].set_ylabel("Frequency (Hz)")

    fig.suptitle(filename, y=0.98)
    fig.tight_layout()

    plt.show()

if __name__ == "__main__":
    main()
