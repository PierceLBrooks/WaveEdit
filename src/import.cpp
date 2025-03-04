#include "WaveEdit.hpp"

#include "imconfig.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <libgen.h>
#include "osdialog/osdialog.h"



enum ImportMode {
	CLEAR_IMPORT,
	OVERWRITE_IMPORT,
	ADD_IMPORT,
	MULTIPLY_IMPORT,
};

static float gain = 0.0;
static float offset = 0.0;
static float zoom = 1.0;
static float leftTrim = 0.0;
static float rightTrim = 0.0;
static ImportMode mode;
static float *audio = NULL;
static int audioLen = 0;
static float *audioPreview = NULL;
static char status[1024] = "";
static Bank importBank = NULL;

static int audioLenMin = 32;
static int audioLenMax = BANK_LEN * WAVE_LEN * 100;


static void zoomFit() {
    int bankLen = BANK_LEN;
    int waveLen = WAVE_LEN;
    if (playingBank != NULL) {
        bankLen = playingBank->bankLen;
        waveLen = playingBank->waveLen;
    }
	zoom = clampf((float)audioLen / (bankLen * waveLen), 0.01, 100.0);
}

static void clearImport() {
	gain = 0.0;
	offset = 0.0;
	zoom = 1.0;
	leftTrim = 0.0;
	rightTrim = BANK_LEN;
	mode = CLEAR_IMPORT;
	if (audio)
		delete[] audio;
	audio = NULL;
	audioLen = 0;
	if (audioPreview)
		delete[] audioPreview;
	audioPreview = NULL;

	status[0] = '\0';
	importBank.clear();
}

static void loadImport(const char *path, int bankLen = BANK_LEN, int waveLen = WAVE_LEN) {
    audioLenMax = bankLen * waveLen * 100;
	clearImport();
	audio = loadAudio(path, &audioLen);
	if (!audio) {
		snprintf(status, sizeof(status), "Cannot load audio file. Only WAV files are supported.");
		return;
	}

	if (audioLen > audioLenMax) {
		snprintf(status, sizeof(status), "Audio file contains %d samples, may have up to %d", audioLen, audioLenMax);
		delete[] audio;
		audio = NULL;
		return;
	}

	if (audioLen < audioLenMin) {
		snprintf(status, sizeof(status), "Audio file contains %d samples, must have at least %d", audioLen, audioLenMin);
		delete[] audio;
		audio = NULL;
		return;
	}

	zoomFit();

	// Generate status line
	char *pathCpy = strdup(path);
	char *filename = basename(pathCpy);
	ellipsize(filename, 80);
	snprintf(status, sizeof(status), "%s: %d samples", filename, audioLen);
	free(pathCpy);

	// Render audio preview by resampling to constant size
	audioPreview = new float[bankLen * waveLen]();
	double previewRatio = bankLen * waveLen / (double)audioLen;
	resample(audio, audioLen, audioPreview, bankLen * waveLen, previewRatio);
}

static float getAudioAmplitude() {
	float max = 0.0;
	for (int i = 0; i < audioLen; i++) {
		float amplitude = fabsf(audio[i]);
		if (amplitude > max)
			max = amplitude;
	}
	return max;
}

static void computeImport(float *samples, int bankLen = BANK_LEN, int waveLen = WAVE_LEN) {
	if (!audio) {
		currentBank.getPostSamples(samples);
		return;
	}

	std::vector<float> importSamples;
    for (int i = 0; i < bankLen * waveLen; i++) {
        importSamples.push_back(0);
    }

	// A bunch of weird constants to align the resampler correctly
	// Basically x's and w's are indices for the audio array, y's are for the bank array
	float wl = offset * audioLen;
	float wr = wl + bankLen * waveLen * zoom;
	float xl = clampf(wl, 0, audioLen);
	float xr = clampf(wr, 0, audioLen);
	float yl = rescalef(xl, wl, wr, 0, bankLen * waveLen);
	float yr = rescalef(xr, wl, wr, 0, bankLen * waveLen);
	yl = clampf(yl, 0, bankLen * waveLen);
	yr = clampf(yr, 0, bankLen * waveLen);
	yl = clampf(yl, leftTrim * waveLen, rightTrim * waveLen);
	yr = clampf(yr, leftTrim * waveLen, rightTrim * waveLen);
	xl = rescalef(yl, 0, bankLen * waveLen, wl, wr);
	xr = rescalef(yr, 0, bankLen * waveLen, wl, wr);
	int xli = roundf(xl);
	int xri = roundf(xr);
	int yli = roundf(yl);
	int yri = roundf(yr);
	float ratio = clampf(1.0 / zoom, 1/300.0, 300.0);

	resample(audio + xli, xri - xli, importSamples.data() + yli, yri - yli, ratio);

	// Apply mode mixing and gain
	switch (mode) {
		case CLEAR_IMPORT:
			break;
		case OVERWRITE_IMPORT:
		case ADD_IMPORT:
		case MULTIPLY_IMPORT:
			currentBank.getPostSamples(samples);
			break;
	}

	float amp = powf(10.0, gain / 20.0);
	for (int i = 0; i < bankLen * waveLen; i++) {
		importSamples[i] *= amp;

		switch (mode) {
			case CLEAR_IMPORT:
				samples[i] = importSamples[i];
				break;
			case OVERWRITE_IMPORT:
				if (yli <= i && i <= yri)
					samples[i] = importSamples[i];
				break;
			case ADD_IMPORT:
				samples[i] += importSamples[i];
				break;
			case MULTIPLY_IMPORT:
				samples[i] *= importSamples[i];
				break;
		}
	}
}


void importPage(int bankLen, int waveLen) {
	ImGui::BeginChild("Import", ImVec2(0, 0), true);
	{
		ImGui::PushItemWidth(-1.0);

		if (ImGui::Button("Browse...")) {
			char *path = osdialog_file(OSDIALOG_OPEN, NULL, NULL, NULL);
			if (path) {
				loadImport(path);
				free(path);
			}
		}
		ImGui::SameLine();
		ImGui::Text("%s", status);

		playingBank = &importBank;
        playingBank->waveLen = waveLen;
        playingBank->clear(bankLen);
		float amp = powf(10.0, gain / 20.0);

		// Audio preview
		ImGui::Text("Imported Audio Preview");
		if (audioPreview) {
            std::vector<float> audioPreviewGain;
			for (int i = 0; i < bankLen * waveLen; i++) {
				audioPreviewGain.push_back(amp * audioPreview[i]);
			}
			float previewStart = offset * bankLen * waveLen;
			float previewRatio = bankLen * waveLen / (float)audioLen;
			float previewEnd = previewStart + bankLen * waveLen * previewRatio * zoom;
			float deltaAudio = renderBankWave("audio preview", 200.0, audioPreviewGain.data(),
                bankLen * waveLen,
				previewStart,
				previewEnd,
                bankLen);
			offset += deltaAudio;
		}
		else {
			renderBankWave("audio preview", 200.0, NULL,
                bankLen * waveLen,
				0,
                bankLen * waveLen,
                bankLen);
		}

		// Bank preview
		ImGui::Text("Bank Preview");
		// Initialize from previous bank
        std::vector<float> bankSamples;
        for (int i = 0; i < bankLen * waveLen; i++) {
            bankSamples.push_back(0);
        }
		computeImport(bankSamples.data(), bankLen, waveLen);
		importBank.setSamples(bankSamples.data());
		float deltaBank = renderBankWave("bank preview", 200.0, bankSamples.data(),
            bankLen * waveLen,
			0,
            bankLen * waveLen,
            bankLen);
		offset -= deltaBank * zoom / audioLen * (bankLen * waveLen);

		if (audio) {
			ImGui::Text("Import Settings");
			// Gain
			if (ImGui::Button("Reset Gain")) gain = 0.0;
			ImGui::SameLine();
			if (ImGui::Button("Normalize")) {
				gain = clampf(-20.0 * log10f(getAudioAmplitude()), -40.0, 40.0);
			}
			ImGui::SameLine();
			ImGui::SliderFloat("##gain", &gain, -40.0, 40.0, "Gain: %.2fdB");

			// Offset
			ImGui::SliderFloat("##offset", &offset, 0.0, 1.0, "Offset: %.4f");

			// Zoom
			if (ImGui::Button("Zoom 1:1")) zoom = 1.0;
			ImGui::SameLine();
			if (ImGui::Button("Zoom Fit")) {
				zoomFit();
			}
			static bool snapZoom = false;
			ImGui::SameLine();
			ImGui::Checkbox("Snap to Power of 2", &snapZoom);
			ImGui::SameLine();
			ImGui::SliderFloat("##zoom", &zoom, 0.01, 100.0, "Zoom: %.4f", 0.0);
			if (snapZoom) {
				zoom = powf(2.0, roundf(log2f(zoom)));
			}

			// Trim
			if (ImGui::Button("Reset Trim")) {
				leftTrim = 0;
				rightTrim = bankLen;
			}
			ImGui::SameLine();
			static bool snapTrim = true;
			ImGui::Checkbox("Snap Trim", &snapTrim);
			if (snapTrim) {
				leftTrim = roundf(leftTrim);
				rightTrim = roundf(rightTrim);
			}
			ImGui::SameLine();
			ImGui::PushItemWidth(-1.0);
			float width = ImGui::CalcItemWidth() / 2.0 - ImGui::GetStyle().FramePadding.y;
			ImGui::PushItemWidth(width);
			ImGui::SliderFloat("##leftTrim", &leftTrim, 0.0, bankLen, snapTrim ? "Left Trim: %.0f" : "Left Trim: %.2f");
			ImGui::SameLine();
			ImGui::SliderFloat("##rightTrim", &rightTrim, 0.0, bankLen, snapTrim ? "Right Trim: %.0f" : "Right Trim: %.2f");
			ImGui::PopItemWidth();
			ImGui::PopItemWidth();

			// Modes
			if (ImGui::RadioButton("Replace All", mode == CLEAR_IMPORT)) mode = CLEAR_IMPORT;
			ImGui::SameLine();
			if (ImGui::RadioButton("Replace Partial", mode == OVERWRITE_IMPORT)) mode = OVERWRITE_IMPORT;
			ImGui::SameLine();
			if (ImGui::RadioButton("Mix", mode == ADD_IMPORT)) mode = ADD_IMPORT;
			ImGui::SameLine();
			if (ImGui::RadioButton("Ring Modulate", mode == MULTIPLY_IMPORT)) mode = MULTIPLY_IMPORT;

			// Apply
			if (ImGui::Button("Cancel")) {
				clearImport();
			}
			ImGui::SameLine();
			if (ImGui::Button("Import")) {
				currentBank = importBank;
				clearImport();
			}
		}
	}
	ImGui::EndChild();
}
