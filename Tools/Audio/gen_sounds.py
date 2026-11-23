"""Generates the game's sounds as 16-bit mono WAV files in SourceAssets/Audio (no samples are needed: everything is synthesised).

Run: python3 Tools/Audio/gen_sounds.py [output_dir]

Loops (names ending in _loop) are exactly periodic: noise is filtered in the frequency domain (circular), and every tone and
modulation has a whole number of cycles in the length, so the end joins the start without a click. The files are imported by
Tools/UE/import_sounds.py as /Game/Audio/SW_<Name>; the game plays them from Source/DoItAsphalt/Game/DIAAudio.*.
"""
import os
import sys
import wave

import numpy as np

SR = 44100
RNG = np.random.default_rng(20261123)


def seconds(t):
    return int(round(t * SR))


def time_axis(n):
    return np.arange(n) / SR


def noise(n):
    return RNG.standard_normal(n)


def band(x, lo, hi):
    """Circular band-pass by FFT (keeps a loop periodic)."""
    spectrum = np.fft.rfft(x)
    freqs = np.fft.rfftfreq(len(x), 1.0 / SR)
    mask = (freqs >= lo) & (freqs <= hi)
    # Soft edges, so that the filter does not ring.
    edge = np.clip(np.minimum(freqs - lo, hi - freqs) / max(1.0, (hi - lo) * 0.25), 0.0, 1.0)
    spectrum = spectrum * np.where(mask, edge, 0.0)
    return np.fft.irfft(spectrum, len(x))


def lowpass(x, hi):
    return band(x, 0.0, hi)


def periodic_lfo(n, cycles, phase=0.0):
    return np.sin(2 * np.pi * (cycles * np.arange(n) / n + phase))


def smooth_noise_lfo(n, hi_hz):
    """Slowly wandering 0..1 curve that is periodic over n samples."""
    x = band(noise(n), 0.5, hi_hz)
    x = x / (np.max(np.abs(x)) + 1e-9)
    return 0.5 + 0.5 * x


def normalize(x, peak=0.85):
    return x / (np.max(np.abs(x)) + 1e-9) * peak


def fade(x, fade_in=0.004, fade_out=0.01):
    n = len(x)
    a = min(n // 2, seconds(fade_in))
    b = min(n // 2, seconds(fade_out))
    x = x.copy()
    if a:
        x[:a] *= np.linspace(0, 1, a)
    if b:
        x[-b:] *= np.linspace(1, 0, b)
    return x


def envelope(n, attack, decay_tau):
    t = time_axis(n)
    env = np.exp(-t / decay_tau)
    a = seconds(attack)
    if a:
        env[:a] *= np.linspace(0, 1, a)
    return env


def write(directory, name, x):
    x = np.clip(x, -1.0, 1.0)
    data = (x * 32767).astype("<i2")
    path = os.path.join(directory, name + ".wav")
    with wave.open(path, "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(SR)
        f.writeframes(data.tobytes())
    print("DIA_OK", path, "%.2fs" % (len(x) / SR))


# ------------------------------------------------------------------ loops


def ambient_loop():
    n = seconds(14)
    wind = band(noise(n), 120, 900) * (0.35 + 0.65 * smooth_noise_lfo(n, 0.6))
    air = band(noise(n), 2000, 6000) * 0.08 * (0.3 + 0.7 * smooth_noise_lfo(n, 0.4))
    traffic = band(noise(n), 40, 160) * 0.5 * (0.6 + 0.4 * periodic_lfo(n, 3))
    return normalize(wind + air + traffic, 0.6)


def plant_loop():
    n = seconds(4)
    t = time_axis(n)
    hum = sum(np.sin(2 * np.pi * 50 * k * t) * (0.5 / k) for k in range(1, 7))
    rumble = band(noise(n), 60, 420) * 0.6 * (0.7 + 0.3 * periodic_lfo(n, 5))
    drum = band(noise(n), 800, 2400) * 0.05
    return normalize(hum * 0.5 + rumble + drum, 0.7)


def engine_loop():
    n = seconds(2)
    t = time_axis(n)
    f0 = 40.0
    tone = sum(np.sin(2 * np.pi * f0 * k * t + k) * (1.0 / (k ** 0.9)) for k in range(1, 14))
    # Firing pulses at four times the base frequency.
    pulse = 0.6 + 0.4 * np.sin(2 * np.pi * f0 * 2 * t)
    body = band(noise(n), 80, 700) * 0.5
    return normalize(tone * pulse * 0.3 + body, 0.7)


def paver_loop():
    n = seconds(2)
    t = time_axis(n)
    whirr = np.sin(2 * np.pi * 180 * t) * 0.4 + np.sin(2 * np.pi * 360 * t) * 0.2
    track = band(noise(n), 150, 1800) * 0.4
    ticks = np.zeros(n)
    for k in range(24):
        i = int(k * n / 24)
        width = seconds(0.012)
        ticks[i:i + width] += np.hanning(width * 2)[width:] * 1.2
    return normalize(whirr * 0.5 + track * (0.7 + 0.3 * periodic_lfo(n, 12)) + band(ticks, 300, 3000) * 0.5, 0.65)


def pour_loop():
    n = seconds(2)
    stream = band(noise(n), 250, 2200)
    burble = smooth_noise_lfo(n, 14)
    low = band(noise(n), 60, 260) * 0.7
    return normalize(stream * (0.4 + 0.6 * burble) + low * burble, 0.7)


def fill_loop():
    n = seconds(2)
    t = time_axis(n)
    stream = band(noise(n), 600, 3800)
    burble = smooth_noise_lfo(n, 20)
    # The tone of the rising level: a resonance that climbs and starts over once per loop.
    rise = np.sin(2 * np.pi * np.cumsum(300 + 500 * (np.arange(n) / n)) / SR) * 0.15
    return normalize(stream * (0.35 + 0.65 * burble) + rise * burble, 0.65)


def station_loop():
    n = seconds(2)
    t = time_axis(n)
    pump = np.sin(2 * np.pi * 110 * t) * 0.5 + np.sin(2 * np.pi * 220 * t) * 0.25
    flow = band(noise(n), 150, 1200) * 0.6
    return normalize(pump * (0.7 + 0.3 * periodic_lfo(n, 8)) + flow, 0.65)


# ------------------------------------------------------------------ one-shots


def scoop():
    n = seconds(0.55)
    t = time_axis(n)
    # The spoon bites into the asphalt (a low squelch), and a short slap when it lifts out.
    squelch = band(noise(n), 150, 900) * envelope(n, 0.01, 0.12)
    drip = np.zeros(n)
    i = seconds(0.32)
    drip[i:] = band(noise(n - i), 400, 2200) * envelope(n - i, 0.002, 0.05)
    return fade(normalize(squelch * 0.8 + drip * 0.7, 0.8))


def step(seed):
    local = np.random.default_rng(seed)
    n = seconds(0.16)
    x = band(local.standard_normal(n), 120 + 40 * (seed % 3), 1800) * envelope(n, 0.002, 0.03)
    crunch = band(local.standard_normal(n), 2500, 7000) * envelope(n, 0.001, 0.012) * 0.3
    return fade(normalize(x + crunch, 0.7))


def thud():
    n = seconds(0.35)
    t = time_axis(n)
    body = np.sin(2 * np.pi * (90 - 40 * t / 0.35) * t) * envelope(n, 0.002, 0.08)
    knock = band(noise(n), 200, 2000) * envelope(n, 0.001, 0.02) * 0.4
    return fade(normalize(body + knock, 0.85))


def click():
    n = seconds(0.07)
    t = time_axis(n)
    return fade(normalize(np.sin(2 * np.pi * 1800 * t) * envelope(n, 0.001, 0.012) + band(noise(n), 2000, 8000) * envelope(n, 0.0005, 0.004) * 0.3, 0.6))


def ping(freqs, spacing, tail, gain=0.8):
    n = seconds(spacing * (len(freqs) - 1) + tail)
    x = np.zeros(n)
    for k, f in enumerate(freqs):
        start = seconds(k * spacing)
        m = n - start
        t = time_axis(m)
        note = (np.sin(2 * np.pi * f * t) + 0.35 * np.sin(2 * np.pi * 2 * f * t) + 0.12 * np.sin(2 * np.pi * 3 * f * t)) * envelope(m, 0.004, tail / 4.5)
        x[start:] += note
    return fade(normalize(x, gain))


def buy():
    return ping([1318.5, 1760.0], 0.07, 0.5)


def unlock():
    return ping([523.25, 659.25, 783.99, 1046.5], 0.11, 0.9)


def win():
    return ping([392.0, 523.25, 659.25, 783.99, 1046.5, 1318.5], 0.16, 1.6, 0.85)


def toast():
    return ping([880.0], 0.0, 0.25, 0.55)


SOUNDS = {
    "Ambient_loop": ambient_loop,
    "Plant_loop": plant_loop,
    "Engine_loop": engine_loop,
    "Paver_loop": paver_loop,
    "Pour_loop": pour_loop,
    "Fill_loop": fill_loop,
    "Station_loop": station_loop,
    "Scoop": scoop,
    "StepA": lambda: step(1),
    "StepB": lambda: step(2),
    "StepC": lambda: step(3),
    "Thud": thud,
    "Click": click,
    "Buy": buy,
    "Unlock": unlock,
    "Win": win,
    "Toast": toast,
}


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "SourceAssets", "Audio")
    os.makedirs(out, exist_ok=True)
    for name, make in SOUNDS.items():
        write(out, name, make())


if __name__ == "__main__":
    main()
