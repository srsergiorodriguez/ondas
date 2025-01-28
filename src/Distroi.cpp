#include "plugin.hpp"

const int EFFECTSNR = 5;
const int PARAMSNR = 3;
const std::string NAMES[EFFECTSNR] = {"Bitcrush", "Decimate", "Distort", "Glitch", "Crop"};
const int MAXGLITCHSAMPLES = (int)(48000 / 2);

struct Distroi : Module {
	enum ParamId {
		ENUMS(BITCHRUSH_PARAM, PARAMSNR), // Quantity, CV Attenuation, dry/wet
		ENUMS(DECIMATE_PARAM, PARAMSNR),
		ENUMS(DISTORT_PARAM, PARAMSNR),
		ENUMS(GLITCH_PARAM, PARAMSNR),
		ENUMS(CROP_PARAM, PARAMSNR),
		PARAMS_LEN
	};
	enum InputId {
		ENUMS(INPUT, EFFECTSNR),
		ENUMS(CV_INPUT, EFFECTSNR),
		INPUTS_LEN
	};
	enum OutputId {
		ENUMS(OUTPUT, EFFECTSNR),
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(LIGHT, EFFECTSNR),
		LIGHTS_LEN
	};

	int decimateCounter = 0;
  float heldSample = 0.0f;

	float glitchBuffer[MAXGLITCHSAMPLES] = {};
	int samplesMade = 0;
	int glitchIndexWrite = 0;
	int glitchIndexRead = 0;
	int glitchTreshold = 0;

	int cropRamp = 0;
	int cropThreshold = 0;

	ParamId PARAMS[EFFECTSNR] = {BITCHRUSH_PARAM, DECIMATE_PARAM, DISTORT_PARAM, GLITCH_PARAM, CROP_PARAM};

	Distroi() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		for (int i = 0; i < EFFECTSNR; i++) {
			configParam(PARAMS[i], 0.f, 1.f, 0.f, NAMES[i] + " effect quantity");
			configParam(PARAMS[i] + 1, 0.f, 1.f, 0.f, NAMES[i] + " CV attenuator");
			configParam(PARAMS[i] + 2, 0.f, 1.f, 0.5f, NAMES[i] +  " dry/wet");
			configInput(INPUT + i, NAMES[i] + " input");
			configInput(CV_INPUT + i, NAMES[i] + "  CV input");
			configOutput(OUTPUT + i, NAMES[i] + " output");
		}
	}

	void process(const ProcessArgs& args) override {

		for (int i = 0; i < EFFECTSNR; i++) {
			if (!inputs[INPUT + i].isConnected() || !outputs[OUTPUT + i].isConnected()) continue;

			float inputSignal = inputs[INPUT + i].getVoltage();
			float cvAmmt = params[PARAMS[i] + 1].getValue();
			float cv = inputs[CV_INPUT + i].isConnected() ? (inputs[CV_INPUT + i].getVoltage() / 10.f) * cvAmmt : 0.0f;
			float quantity = clamp(params[PARAMS[i]].getValue() + cv, 0.f, 1.f);

			float dw = params[PARAMS[i] + 2].getValue();
			float result = inputSignal;

			if (i == 0) {
				// Bitcrusher
				float scale = pow(2.0f, 8 - ((0.2 + quantity) * 8));
        result = round(inputSignal * scale) / scale;
			}

			if (i == 1) {
				// Decimator
				decimateCounter++;
        if (decimateCounter >= quantity * 32) {
					heldSample = inputSignal; // Update the held sample
					decimateCounter = 0;
        }
				result = heldSample;
			}

			if (i == 2) {
				// Distort
				float drive = quantity * 10.f;
				result = tanh(inputSignal * (1 + drive));
			}

			if (i == 3) {
				// Glitch
				// Grab a random sample from the signal and occasionally rewrite it or play it
				// Constantly write little fragments of signal in the same buffer, grain like / regular buffering and jumped buffering
				// Wait until buffer is full before glitching
				// Once is glitched, randomly start glitch read
				if (samplesMade < MAXGLITCHSAMPLES) {
					glitchBuffer[samplesMade] = inputSignal;
					samplesMade++;
					result = inputSignal;
				} else {
					if (glitchIndexRead < glitchTreshold) {
						// Currently glitching
						result = glitchBuffer[glitchIndexRead];
						glitchBuffer[glitchIndexRead] = inputSignal;
						glitchIndexRead++;
					} else {
						if (random::uniform() < quantity) {
							glitchTreshold = (int)(random::uniform() * (MAXGLITCHSAMPLES - (quantity * 0.9 * MAXGLITCHSAMPLES)));
							glitchIndexRead = 0;
						}
						result = inputSignal;
					}
				}

			}

			if (i == 4) {
				// Crop
				// Occasionally silence signal abruptly
				if (cropRamp < cropThreshold) {
					// Currently cropping
					result = inputSignal * 0.01f;
					cropRamp++;
				} else {
					if (random::uniform() < quantity * 0.001) {
						cropThreshold = (int)(random::uniform() * args.sampleRate * 0.1);
						cropRamp = 0;
					}
					result = inputSignal;
				}
			}

			float output = (inputSignal * (1 - dw)) + (result * dw); 

			outputs[OUTPUT + i].setVoltage(output);
		}
	}
};


struct DistroiWidget : ModuleWidget {
	DistroiWidget(Distroi* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Distroi.svg")));

		float hp = 5.08f;

		addChild(new TextDisplayWidget("Distroi", Vec(hp/2, hp*1.5), 14, -1));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		float minX3 = hp * 1.5;
		float maxX3 = hp * 6.5;
		float divX3 = (maxX3 - minX3) / 2;

		float controlsY = hp * 4;
		float divYcontrols = hp * 4.4f;
		float tOffset = 6.f;

		Distroi::ParamId PARAMS[EFFECTSNR] = {Distroi::BITCHRUSH_PARAM, Distroi::DECIMATE_PARAM, Distroi::DISTORT_PARAM, Distroi::GLITCH_PARAM, Distroi::CROP_PARAM};

		for (int i = 0; i < EFFECTSNR; i++) {
			addChild(new TextDisplayWidget(NAMES[i], Vec(minX3 + (divX3 * 1.f), controlsY + (divYcontrols * i) - tOffset), 10));
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(minX3, controlsY + (divYcontrols * i))), module, PARAMS[i]));
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(minX3 + divX3, controlsY + (divYcontrols * i))), module, PARAMS[i] + 1));
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(minX3 + (divX3 * 2.f), controlsY + (divYcontrols * i))), module, PARAMS[i] + 2));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(minX3, controlsY + (divYcontrols * (i + 0.4f)))), module, Distroi::INPUT + i));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(minX3 + (divX3), controlsY + (divYcontrols * (i + 0.4f)))), module, Distroi::CV_INPUT + i));
			addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(minX3 + (divX3 * 2), controlsY + (divYcontrols * (i + 0.4f)))), module, Distroi::OUTPUT + i));
			// addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(minX3 + (divX3 * 1.5), controlsY + (divYcontrols * (i + 0.4f)))), module, Distroi::LIGHT + i));
		}
	}
};


Model* modelDistroi = createModel<Distroi, DistroiWidget>("Distroi");