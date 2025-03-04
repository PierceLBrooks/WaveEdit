#include "WaveEdit.hpp"
#include <string.h>
#include <sndfile.h>

#if defined(_WIN32)
#define strcasecmp _stricmp
#endif


Bank::Bank(int len) {
    waveLen = WAVE_LEN;
    clear(len);
}


void Bank::clear(int len) {
	// The lazy way
    waves.clear();
    bankLen = len;

	for (int i = 0; i < len; i++) {
        waves.push_back(Wave(waveLen));
		waves[i].commitSamples();
	}
}


void Bank::swap(int i, int j) {
	Wave tmp = waves[i];
	waves[i] = waves[j];
	waves[j] = tmp;
}


void Bank::shuffle() {
	for (int j = bankLen - 1; j >= 3; j--) {
		int i = rand() % j;
		swap(i, j);
	}
}


void Bank::setSamples(const float *in) {
	for (int j = 0; j < bankLen; j++) {
        for (int i = 0; i < waveLen; i++) {
            if (i >= waves[j].waveLen) {
                break;
            }
            waves[j].samples[i] = in[j * waveLen + i];
        }
		waves[j].commitSamples();
	}
}


void Bank::getPostSamples(float *out) {
	for (int j = 0; j < bankLen; j++) {
		memcpy(&out[j * waveLen], waves[j].postSamples.data(), sizeof(float) * waveLen);
	}
}


void Bank::duplicateToAll(int waveId) {
	for (int j = 0; j < bankLen; j++) {
		if (j != waveId)
			waves[j] = waves[waveId];
		// No need to commit the wave because we're copying everything
	}
}


bool Bank::save(const char *filename) {
	FILE *f = fopen(filename, "w");
    if (!f) {fclose(f);return false;}

    std::string s = std::to_string(bankLen)+"\n";
    fwrite(s.c_str(), s.length(), 1, f);
    s = std::to_string(waveLen)+"\n";
    fwrite(s.c_str(), s.length(), 1, f);
    for (int j = 0; j < bankLen; j++) {
        s = std::to_string(waves[j].effectsLen)+"\n";
        fwrite(s.c_str(), s.length(), 1, f);
        for (int i = 0; i < waves[j].effectsLen; i++) {
            s = std::to_string(waves[j].effects[i])+"\n";
            fwrite(s.c_str(), s.length(), 1, f);
        }
        s = std::to_string(waves[j].waveLen)+"\n";
        fwrite(s.c_str(), s.length(), 1, f);
        for (int i = 0; i < waves[j].waveLen; i++) {
            s = std::to_string(waves[j].samples[i])+"\n";
            fwrite(s.c_str(), s.length(), 1, f);
            s = std::to_string(waves[j].spectrum[i])+"\n";
            fwrite(s.c_str(), s.length(), 1, f);
            s = std::to_string(waves[j].postSamples[i])+"\n";
            fwrite(s.c_str(), s.length(), 1, f);
            s = std::to_string(waves[j].postSpectrum[i])+"\n";
            fwrite(s.c_str(), s.length(), 1, f);
        }
        for (int i = 0; i < waves[j].waveLen / 2; i++) {
            s = std::to_string(waves[j].harmonics[i])+"\n";
            fwrite(s.c_str(), s.length(), 1, f);
            s = std::to_string(waves[j].postHarmonics[i])+"\n";
            fwrite(s.c_str(), s.length(), 1, f);
        }
        s = std::to_string(static_cast<int>(waves[j].cycle))+"\n";
        fwrite(s.c_str(), s.length(), 1, f);
        s = std::to_string(static_cast<int>(waves[j].normalize))+"\n";
        fwrite(s.c_str(), s.length(), 1, f);
    }
	fclose(f);

    return true;
}


bool Bank::load(const char *filename) {
	FILE *f = fopen(filename, "r");
	if (!f) return false;
    std::string s = "";
    if (!freadLine(&s, f)) {fclose(f);return false;}
    bankLen = atoi(s.c_str());
    s = "";
    if (!freadLine(&s, f)) {fclose(f);return false;}
    waveLen = atoi(s.c_str());
    clear(bankLen);
    for (int j = 0; j < bankLen; j++) {
        s = "";
        if (!freadLine(&s, f)) {fclose(f);return false;}
        waves[j].effectsLen = atoi(s.c_str());
        waves[j].effects.clear();
        for (int i = 0; i < waves[j].effectsLen; i++) {
            s = "";
            if (!freadLine(&s, f)) {fclose(f);return false;}
            waves[j].effects.push_back(atof(s.c_str()));
        }
        s = "";
        if (!freadLine(&s, f)) {fclose(f);return false;}
        waves[j].waveLen = atoi(s.c_str());
        waves[j].samples.clear();
        waves[j].spectrum.clear();
        waves[j].postSamples.clear();
        waves[j].postSpectrum.clear();
        for (int i = 0; i < waves[j].waveLen; i++) {
            s = "";
            if (!freadLine(&s, f)) {fclose(f);return false;}
            waves[j].samples.push_back(atof(s.c_str()));
            s = "";
            if (!freadLine(&s, f)) {fclose(f);return false;}
            waves[j].spectrum.push_back(atof(s.c_str()));
            s = "";
            if (!freadLine(&s, f)) {fclose(f);return false;}
            waves[j].postSamples.push_back(atof(s.c_str()));
            s = "";
            if (!freadLine(&s, f)) {fclose(f);return false;}
            waves[j].postSpectrum.push_back(atof(s.c_str()));
        }
        waves[j].harmonics.clear();
        waves[j].postHarmonics.clear();
        for (int i = 0; i < waves[j].waveLen / 2; i++) {
            s = "";
            if (!freadLine(&s, f)) {fclose(f);return false;}
            waves[j].harmonics.push_back(atof(s.c_str()));
            s = "";
            if (!freadLine(&s, f)) {fclose(f);return false;}
            waves[j].postHarmonics.push_back(atof(s.c_str()));
        }
        s = "";
        if (!freadLine(&s, f)) {fclose(f);return false;}
        waves[j].cycle = static_cast<bool>(atoi(s.c_str()));
        s = "";
        if (!freadLine(&s, f)) {fclose(f);return false;}
        waves[j].normalize = static_cast<bool>(atoi(s.c_str()));
        waves[j].commitSamples();
    }
	fclose(f);

    return true;
}


void Bank::saveWAV(const char *filename) {
	SF_INFO info;
	info.samplerate = 44100;
	info.channels = 1;
	info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16 | SF_ENDIAN_LITTLE;
	SNDFILE *sf = sf_open(filename, SFM_WRITE, &info);
	if (!sf)
		return;

	for (int j = 0; j < bankLen; j++) {
		sf_write_float(sf, waves[j].postSamples.data(), waveLen);
	}

	sf_close(sf);
}


void Bank::loadWAV(const char *filename) {
	clear(bankLen);

	SF_INFO info;
	SNDFILE *sf = sf_open(filename, SFM_READ, &info);
	if (!sf)
		return;

	for (int i = 0; i < bankLen; i++) {
		sf_read_float(sf, waves[i].samples.data(), waveLen);
		waves[i].commitSamples();
	}

	sf_close(sf);
}


void Bank::saveWaves(const char *dirname) {
	for (int b = 0; b < bankLen; b++) {
		char filename[1024];
		snprintf(filename, sizeof(filename), "%s/%02d.wav", dirname, b);

		waves[b].saveWAV(filename);
	}
}


void Bank::saveWT(const char *filename)
{
	FILE *f = fopen(filename, "wb");
	if (!f)
		return;

	fputs("vawt", f);
	fwriteLE32(waveLen, f);
	fwriteLE16(bankLen, f);
	fwriteLE16(0, f);

	for (int i = 0; i < bankLen; i++) {
		for (int j = 0; j < waveLen; j++) {
			union { uint32_t i; float f; } u;
			u.f = waves[i].postSamples[j];
			fwriteLE32(u.i, f);
		}
	}

	fclose(f);
}


void Bank::loadWT(const char *filename)
{
	FILE *f = fopen(filename, "rb");
	if (!f)
		return;

	char magic[4];
	memset(magic, 0, 4);
	fread(magic, 4, 1, f);

	if (memcmp(magic, "vawt", 4) != 0) {
		fclose(f);
		return;
	}

	uint32_t waveLen = 0;
	uint16_t fileBankLen = 0;
	uint16_t flags = 0;

	freadLE32(&waveLen, f);
	freadLE16(&fileBankLen, f);
	freadLE16(&flags, f);

	/*if (waveLen > 1024) {
		fclose(f);
		return;
	}*/

    bankLen = fileBankLen;
    this->waveLen = waveLen;

	clear(bankLen);

	std::vector<float> rawSamples;

	if (flags & 4) {
		for (uint32_t i = 0; i < waveLen * bankLen; ++i) {
			int16_t sample = 0;
			freadLE16((uint16_t *)&sample, f);
			rawSamples.push_back(sample / 32768.0f);
		}
	}
	else {
		for (uint32_t i = 0; i < waveLen * bankLen; ++i) {
			union { uint32_t i; float f; } u;
			u.i = 0;
			freadLE32(&u.i, f);
			rawSamples.push_back(u.f);
		}
	}

	for (uint32_t i = 0; i < bankLen; i++) {
        while (waves.size() <= i) {
            waves.push_back(Wave(waveLen));
        }
        waves[i].samples.clear();
        for (uint32_t j = 0; j < waveLen; j++) {
            waves[i].samples.push_back(rawSamples[waveLen * i + j]);
        }
		waves[i].commitSamples();
	}

	fclose(f);
}


enum BankFileFormat {
	BANK_FORMAT_WAV,
	BANK_FORMAT_WT,
};


static int getFormatByFilename(const char *filename)
{
	size_t len = strlen(filename);
	if (len > 4 && strcasecmp(filename + len - 4, ".wav") == 0)
		return BANK_FORMAT_WAV;
	else if (len > 3 && strcasecmp(filename + len - 3, ".wt") == 0)
		return BANK_FORMAT_WT;
	else
		return -1;
}


void Bank::saveAuto(const char *filename)
{
	switch (getFormatByFilename(filename)) {
		default:
		case BANK_FORMAT_WAV:
			saveWAV(filename);
			break;
		case BANK_FORMAT_WT:
			saveWT(filename);
			break;
	}
}


void Bank::loadAuto(const char *filename)
{
	switch (getFormatByFilename(filename)) {
		default:
		case BANK_FORMAT_WAV:
			loadWAV(filename);
			break;
		case BANK_FORMAT_WT:
			loadWT(filename);
			break;
	}
}
