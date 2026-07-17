"""
command_format_cut.py - reads .wav files from a source folder, detects where the
spoken word starts and cuts/pads each recording to a fixed 1.2s window

Usage:
    python command_format_cut.py <input_folder> <output_folder> [--margin-db N] [--dry-run]

Examples:
    python command_format_cut.py raw_data dataset
    python command_format_cut.py raw_data dataset --margin-db 10
    python command_format_cut.py raw_data dataset --dry-run
"""

import sys
import os
import argparse
import wave
import numpy as np

MAX_LENGTH = 1.2
LEAD_SILENCE = 0.1
FRAME_MS = 20
HOP_MS = 10
MIN_VOICE_MS = 30         # energy must stay above threshold this long to count as onset
DEFAULT_MARGIN_DB = 15    # default dB above noise floor that counts as "voice"
NOISE_PERCENTILE = 20     # percentile of whole-file energy used as the noise-floor estimate
 
 
def read_wav(path):
    with wave.open(path, 'rb') as wf:
        params = wf.getparams()
        n_frames = wf.getnframes()
        raw = wf.readframes(n_frames)
 
    if params.sampwidth != 2:
        raise ValueError(f"{path}: only 16-bit PCM wav is supported (got "f"{params.sampwidth * 8}-bit)")
 
    samples = np.frombuffer(raw, dtype=np.int16).astype(np.float32)
 
    if params.nchannels > 1:
        # Downmix to mono
        samples = samples.reshape(-1, params.nchannels).mean(axis=1)
 
    return samples, params.framerate, params
 
 
def frame_energy_db(samples, framerate, frame_ms=FRAME_MS, hop_ms=HOP_MS):
    # Computes short-term energy (in dB) over sliding frames. Returns (energy_db array, hop_length_in_samples)

    frame_len = int(framerate * frame_ms / 1000)
    hop_len = int(framerate * hop_ms / 1000)
 
    n_frames = max(1, (len(samples) - frame_len) // hop_len + 1)
    energies = np.empty(n_frames, dtype=np.float64)
 
    for i in range(n_frames):
        start = i * hop_len
        frame = samples[start:start + frame_len].astype(np.float64)
        frame = frame - frame.mean()  # per-frame DC removal
        rms = np.sqrt(np.mean(frame ** 2) + 1e-10)
        energies[i] = 20 * np.log10(rms + 1e-10)
 
    return energies, hop_len
 
 
def detect_onset(samples, framerate, margin_db):
    # Estimate the time (seconds) where voice activity starts, using an energy threshold set relative to the noise floor.
    # Falls back to 0.0 (start of file) if no clear onset is found. Returns (onset_time, noise_floor_db, threshold_db)
    
    energies, hop_len = frame_energy_db(samples, framerate)
    hop_time = hop_len / framerate
 
    noise_floor_db = np.percentile(energies, NOISE_PERCENTILE)
    threshold_db = noise_floor_db + margin_db
 
    min_voice_frames = max(1, int(MIN_VOICE_MS / HOP_MS))
 
    above = energies >= threshold_db
    for i in range(len(above) - min_voice_frames + 1):
        if above[i:i + min_voice_frames].all():
            return i * hop_time, noise_floor_db, threshold_db
 
    # No sustained onset found above threshold -> treat whole file as active
    return 0.0, noise_floor_db, threshold_db
 
 
def cut_segment(samples, framerate, onset_time):
    # Extract a MAX_LENGTH-second segment starting with LEAD_SILENCE seconds before onset_time 

    start_time = max(0.0, onset_time - LEAD_SILENCE)
    start_sample = int(start_time * framerate)
    length_samples = int(MAX_LENGTH * framerate)
    end_sample = start_sample + length_samples
 
    segment = samples[start_sample:end_sample]
 
    if len(segment) < length_samples:
        pad = np.zeros(length_samples - len(segment), dtype=np.float32)
        segment = np.concatenate([segment, pad])
 
    return segment.astype(np.int16)
 
 
def write_wav(path, samples_int16, framerate):
    with wave.open(path, 'wb') as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(framerate)
        wf.writeframes(samples_int16.tobytes())
 
 
def process_file(in_path, out_path, margin_db, dry_run=False):
    # executes sequence of actions of record processing, from read to write

    samples, framerate, params = read_wav(in_path)
    duration = len(samples) / framerate
 
    onset_time, noise_floor_db, threshold_db = detect_onset(samples, framerate, margin_db)
    start_time = max(0.0, onset_time - LEAD_SILENCE)
    lead_kept = min(onset_time, LEAD_SILENCE)
 
    print(f"  {os.path.basename(in_path)}: {duration:.2f}s | "
          f"noise floor {noise_floor_db:.1f}dB, threshold {threshold_db:.1f}dB | "
          f"onset @ {onset_time*1000:.0f}ms -> "
          f"window [{start_time*1000:.0f}ms : {(start_time+MAX_LENGTH)*1000:.0f}ms] "
          f"(lead silence kept: {lead_kept*1000:.0f}ms)"
          + ("  [DRY RUN]" if dry_run else ""))
 
    if not dry_run:
        segment = cut_segment(samples, framerate, onset_time)
        write_wav(out_path, segment, framerate)
 
 
def main():
    parser = argparse.ArgumentParser(description="Cut/pad .wav files to a fixed window around detected voice activity.")
    parser.add_argument("input_folder")
    parser.add_argument("output_folder")
    parser.add_argument("--margin-db", type=float, default=DEFAULT_MARGIN_DB)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
 
    if not os.path.isdir(args.input_folder):
        print(f"Error: input folder '{args.input_folder}' not found")
        sys.exit(1)
 
    if not args.dry_run:
        os.makedirs(args.output_folder, exist_ok=True)
 
    wav_files = [f for f in os.listdir(args.input_folder) if f.lower().endswith('.wav')]
    if not wav_files:
        print(f"No .wav files found in '{args.input_folder}'")
        return
 
    print(f"Processing {len(wav_files)} file(s) from '{args.input_folder}' "
          f"-> '{args.output_folder}' (margin: {args.margin_db}dB)")
    for fname in wav_files:
        in_path = os.path.join(args.input_folder, fname)
        out_path = os.path.join(args.output_folder, fname)
        try:
            process_file(in_path, out_path, args.margin_db, dry_run=args.dry_run)
        except Exception as e:
            print(f"  {fname}: FAILED ({e})")
 
 
if __name__ == "__main__":
    main()