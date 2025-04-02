#include "WaveEdit.hpp"
#include "exprtk.hpp"
#include <sndfile.h>


static Wave clipboardWave = {};
bool clipboardActive = false;


const char *effectNames[EFFECTS_LEN] {
	"Pre-Gain",
	"Phase Shift",
	"Harmonic Shift",
	"Comb Filter",
	"Ring Modulation",
	"Chebyshev Wavefolding",
	"Sample & Hold",
	"Quantization",
	"Slew Limiter",
	"Lowpass Filter",
	"Highpass Filter",
	"Post-Gain",
};


Wave::Wave(int len) {
    effectsLen = EFFECTS_LEN;
    clear(len);
}

void Wave::clear(int len) {
    if (len == BANK_LEN && playingBank != NULL) {
        waveLen = playingBank->waveLen;
    } else {
        waveLen = len;
    }
    effects.clear();
    for (int i = 0; i < effectsLen; i++) {
        effects.push_back(0);
    }
    samples.clear();
    spectrum.clear();
    postSamples.clear();
    postSpectrum.clear();
    for (int i = 0; i < waveLen; i++) {
        samples.push_back(0);
        spectrum.push_back(0);
        postSamples.push_back(0);
        postSpectrum.push_back(0);
    }
    harmonics.clear();
    postHarmonics.clear();
    for (int i = 0; i < waveLen / 2; i++) {
        harmonics.push_back(0);
        postHarmonics.push_back(0);
    }
}

void Wave::randomize(int seed) {
    std::mt19937 generator;
    std::uniform_real_distribution<float> distributor(-1, 1);
    clear(waveLen);
    generator.seed(seed);
    for (int i = 0; i < waveLen; i++) {
        samples[i] = distributor(generator);
    }
    commitSamples();
}

bool Wave::overwrite(std::string formula) {
    float x;
    float wave;
    exprtk::symbol_table<float> symbols;
    exprtk::expression<float> expression;
    exprtk::parser<float> parser;
    if (!symbols.add_variable("x", x)) {
        return false;
    }
    if (!symbols.add_variable("wavesize", wave)) {
        return false;
    }
    if (!symbols.add_constants()) {
        return false;
    }
    if (!expression.register_symbol_table(symbols)) {
        return false;
    }
    if (!parser.compile(formula, expression)) {
        return false;
    }
    wave = waveLen;
    clear(waveLen);
    for (int j = 0; j < waveLen; j++) {
        x = ((float)j) / waveLen;
        samples[j] = clampf(expression.value(), -1.0, 1.0);
    }
    commitSamples();
    return true;
}

void Wave::updatePost() {
    static std::vector<float> out;
	while (out.size() < waveLen) {
		out.push_back(0);
	}
    for (int i = 0; i < waveLen; i++) {
        out[i] = samples[i];
    }

	// Pre-gain
	if (effects[PRE_GAIN]) {
		float gain = powf(20.0, effects[PRE_GAIN]);
		for (int i = 0; i < waveLen; i++) {
			out[i] *= gain;
		}
	}

	// Temporal and Harmonic Shift
	if (effects[PHASE_SHIFT] > 0.0 || effects[HARMONIC_SHIFT] > 0.0) {
		// Shift Fourier phase proportionally
        std::vector<float> tmp;
        for (int i = 0; i < waveLen; i++) {
            tmp.push_back(out[i]);
        }
		RFFT(out.data(), tmp.data(), waveLen);
		for (int k = 0; k < waveLen / 2; k++) {
			float phase = clampf(effects[HARMONIC_SHIFT], 0.0, 1.0) + clampf(effects[PHASE_SHIFT], 0.0, 1.0) * k;
			float br = cosf(2 * M_PI * phase);
			float bi = -sinf(2 * M_PI * phase);
			cmultf(&tmp[2 * k], &tmp[2 * k + 1], tmp[2 * k], tmp[2 * k + 1], br, bi);
		}
		IRFFT(tmp.data(), out.data(), waveLen);
	}

	// Comb filter
	if (effects[COMB] > 0.0) {
		const float base = 0.75;
		const int taps = 40;

		// Build the kernel in Fourier space
		// Place taps at positions `comb * j`, with exponentially decreasing amplitude
        std::vector<float> kernel;
        for (int i = 0; i < waveLen; i++) {
            kernel.push_back(0);
        }
		for (int k = 0; k < waveLen / 2; k++) {
			for (int j = 0; j < taps; j++) {
				float amplitude = powf(base, j);
				// Normalize by sum of geometric series
				amplitude *= (1.0 - base);
				float phase = -2.0 * M_PI * k * effects[COMB] * j;
				kernel[2 * k] += amplitude * cosf(phase);
				kernel[2 * k + 1] += amplitude * sinf(phase);
			}
		}

		// Convolve FFT of input with kernel
        std::vector<float> fft;
        for (int i = 0; i < waveLen; i++) {
            fft.push_back(0);
        }
		RFFT(out.data(), fft.data(), waveLen);
		for (int k = 0; k < waveLen / 2; k++) {
			cmultf(&fft[2 * k], &fft[2 * k + 1], fft[2 * k], fft[2 * k + 1], kernel[2 * k], kernel[2 * k + 1]);
		}
		IRFFT(fft.data(), out.data(), waveLen);
	}

	// Ring modulation
	if (effects[RING] > 0.0) {
		float ring = ceilf(powf(effects[RING], 2) * (waveLen / 2 - 2));
		for (int i = 0; i < waveLen; i++) {
			float phase = (float)i / waveLen * ring;
			out[i] *= sinf(2 * waveLen * phase);
		}
	}

	// Chebyshev waveshaping
	if (effects[CHEBYSHEV] > 0.0) {
		float n = powf(50.0, effects[CHEBYSHEV]);
		for (int i = 0; i < waveLen; i++) {
			// Apply a distant variant of the Chebyshev polynomial of the first kind
			if (-1.0 <= out[i] && out[i] <= 1.0)
				out[i] = sinf(n * asinf(out[i]));
			else
				out[i] = sinf(n * asinf(1.0 / out[i]));
		}
	}

	// Sample & Hold
	if (effects[SAMPLE_AND_HOLD] > 0.0) {
		float frameskip = powf(waveLen / 2.0, clampf(effects[SAMPLE_AND_HOLD], 0.0, 1.0));
        std::vector<float> tmp;
        for (int i = 0; i < waveLen; i++) {
            tmp.push_back(out[i]);
        }
        tmp.push_back(tmp[0]);

		// Dumb linear interpolation S&H
		for (int i = 0; i < waveLen; i++) {
			float index = roundf(i / frameskip) * frameskip;
			out[i] = linterpf(tmp.data(), clampf(index, 0.0, waveLen - 1));
		}
	}

	// Quantization
	if (effects[QUANTIZATION] > 1e-3) {
		float levels = powf(clampf(effects[QUANTIZATION], 0.0, 1.0), -1.5);
		for (int i = 0; i < waveLen; i++) {
			out[i] = roundf(out[i] * levels) / levels;
		}
	}

	// Slew Limiter
	if (effects[SLEW] > 0.0) {
		float slew = powf(0.001, effects[SLEW]);

		float y = out[0];
		for (int i = 1; i < waveLen; i++) {
			float dxdt = out[i] - y;
			float dydt = clampf(dxdt, -slew, slew);
			y += dydt;
			out[i] = y;
		}
	}

	// Brick-wall lowpass / highpass filter
	// TODO Maybe change this into a more musical filter
	if (effects[LOWPASS] > 0.0 || effects[HIGHPASS]) {
        std::vector<float> fft;
        for (int i = 0; i < waveLen; i++) {
            fft.push_back(0);
        }
		RFFT(out.data(), fft.data(), waveLen);
		float lowpass = 1.0 - effects[LOWPASS];
		float highpass = effects[HIGHPASS];
		for (int i = 1; i < waveLen / 2; i++) {
			float v = clampf(waveLen / 2 * lowpass - i, 0.0, 1.0) * clampf(-waveLen / 2 * highpass + i, 0.0, 1.0);
			fft[2 * i] *= v;
			fft[2 * i + 1] *= v;
		}
		IRFFT(fft.data(), out.data(), waveLen);
	}

	// TODO Consider removing because Normalize does this for you
	// Post gain
	if (effects[POST_GAIN]) {
		float gain = powf(20.0, effects[POST_GAIN]);
		for (int i = 0; i < waveLen; i++) {
			out[i] *= gain;
		}
	}

	// Cycle
	if (cycle) {
		float start = out[0];
		float end = out[waveLen - 1] / (waveLen - 1) * waveLen;

		for (int i = 0; i < waveLen; i++) {
			out[i] -= (end - start) * (i - waveLen / 2) / waveLen;
		}
	}

	// Normalize
	if (normalize) {
		float max = -INFINITY;
		float min = INFINITY;
		for (int i = 0; i < waveLen; i++) {
			if (out[i] > max) max = out[i];
			if (out[i] < min) min = out[i];
		}

		if (max - min >= 1e-6) {
			for (int i = 0; i < waveLen; i++) {
				out[i] = rescalef(out[i], min, max, -1.0, 1.0);
			}
		}
		else {
            out.clear();
            for (int i = 0; i < waveLen; i++) {
                out.push_back(0);
            }
		}
	}

	// Hard clip :(
    // TODO Fix possible race condition with audio thread here
    // Or not, because the race condition would only just replace samples as they are being read, which just gives a click sound.
	for (int i = 0; i < waveLen; i++) {
		out[i] = clampf(out[i], -1.0, 1.0);
        postSamples[i] = out[i];
	}

	// Convert wave to spectrum
	RFFT(postSamples.data(), postSpectrum.data(), waveLen);
	// Convert spectrum to harmonics
	for (int i = 0; i < waveLen / 2; i++) {
		postHarmonics[i] = hypotf(postSpectrum[2 * i], postSpectrum[2 * i + 1]) * 2.0;
	}
}

void Wave::commitSamples() {
	// Convert wave to spectrum
	RFFT(samples.data(), spectrum.data(), waveLen);
	// Convert spectrum to harmonics
	for (int i = 0; i < waveLen / 2; i++) {
		harmonics[i] = hypotf(spectrum[2 * i], spectrum[2 * i + 1]) * 2.0;
	}
	updatePost();
}

void Wave::commitHarmonics() {
	// Rescale spectrum by the new norm
	for (int i = 0; i < waveLen / 2; i++) {
		float oldHarmonic = hypotf(spectrum[2 * i], spectrum[2 * i + 1]);
		float newHarmonic = harmonics[i] / 2.0;
		if (oldHarmonic > 1.0e-6) {
			// Preserve old phase but apply new magnitude
			float ratio = newHarmonic / oldHarmonic;
			if (i == 0) {
				spectrum[2 * i] *= ratio;
				spectrum[2 * i + 1] = 0.0;
			}
			else {
				spectrum[2 * i] *= ratio;
				spectrum[2 * i + 1] *= ratio;
			}
		}
		else {
			// If there is no old phase (magnitude is 0), set to 90 degrees
			if (i == 0) {
				spectrum[2 * i] = newHarmonic;
				spectrum[2 * i + 1] = 0.0;
			}
			else {
				spectrum[2 * i] = 0.0;
				spectrum[2 * i + 1] = -newHarmonic;
			}
		}
	}
	// Convert spectrum to wave
	IRFFT(spectrum.data(), samples.data(), waveLen);
	updatePost();
}

void Wave::clearEffects() {
    effects.clear();
    for (int i = 0; i < effectsLen; i++) {
        effects.push_back(0);
    }
	cycle = false;
	normalize = false;
	updatePost();
}

void Wave::bakeEffects() {
    for (int i = 0; i < waveLen; i++) {
        samples[i] = postSamples[i];
    }
	clearEffects();
}

void Wave::randomizeEffects() {
	for (int i = 0; i < effectsLen; i++) {
		effects[i] = randf() > 0.5 ? powf(randf(), 2) : 0.0;
	}
	updatePost();
}

void Wave::saveWAV(const char *filename) {
	SF_INFO info;
	info.samplerate = 44100;
	info.channels = 1;
	info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16 | SF_ENDIAN_LITTLE;
	SNDFILE *sf = sf_open(filename, SFM_WRITE, &info);
	if (!sf)
		return;

	sf_write_float(sf, postSamples.data(), waveLen);

	sf_close(sf);
}

void Wave::loadWAV(const char *filename) {
	clear();

	SF_INFO info;
	SNDFILE *sf = sf_open(filename, SFM_READ, &info);
	if (!sf)
		return;

	sf_read_float(sf, samples.data(), waveLen);
	commitSamples();

	sf_close(sf);
}

void Wave::clipboardCopy() {
	memcpy(&clipboardWave, this, sizeof(*this));
	clipboardActive = true;
}

void Wave::clipboardPaste() {
	if (clipboardActive) {
		memcpy(this, &clipboardWave, sizeof(*this));
	}
}
