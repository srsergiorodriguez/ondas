#include "plugin.hpp"
#include <string>

const int PARTS = 5;

struct BaBum : Module {
	enum ParamId {
		TUNEBD_PARAM,
		TUNESNR_PARAM,
		TUNEHH_PARAM,
		TUNEFX_PARAM,
		LENGTHBD_PARAM,
		LENGTHSNR_PARAM,
		LENGTHHH_PARAM,
		LENGTHFX_PARAM,
		PARAMBD_PARAM,
		PARAMSNR_PARAM,
		PARAMHH_PARAM,
		PARAMFX_PARAM,
		TRIGBD_PARAM,
		TRIGSNR_PARAM,
		TRIGHH_PARAM,
		TRIGHHO_PARAM,
		TRIGFX_PARAM,
		MIXBD_PARAM,
		MIXSNR_PARAM,
		MIXHH_PARAM,
		MIXHHO_PARAM,
		MIXFX_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		TUNEBD_INPUT,
		TUNESNR_INPUT,
		TUNEHH_INPUT,
		TUNEFX_INPUT,
		BD_INPUT,
		SNR_INPUT,
		HH_INPUT,
		HHO_INPUT,
		FX_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		BD_OUTPUT,
		SNR_OUTPUT,
		HH_OUTPUT,
		HHO_OUTPUT,
		FX_OUTPUT,
		MIX_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTBD_LIGHT,
		LIGHTSNR_LIGHT,
		LIGHTHH_LIGHT,
		LIGHTHHO_LIGHT,
		LIGHTFX_LIGHT,
		LIGHTS_LEN
	};

	float CLIP_RATIO = 0.02; // To set a little attack time to avoid clipping in some amps
	float BASE_FREQ = 10; // Base freq in Hz for oscs
	float REST_STATE = 4.0f; // Fraction of a second to define rest threshold for triggers

	float oscRamps[PARTS] = {0.f, 0.f, 0.f, 0.f, 0.f}; // Register ramp state for oscs
	float ampRamps[PARTS] = {0.f, 0.f, 0.f, 0.f, 0.f}; // Register ramp state for amps
	float ampRatio[PARTS] = {5.f, 5.f, 5.f, 5.f, 5.f}; // Fraction of a second to define length of each amp
	float triggerPrevStates[PARTS] = {0.f, 0.f, 0.f, 0.f, 0.f}; // Previous state of trigger to avoid fast retriggering
	float triggerRestStates[PARTS] = {0.f, 0.f, 0.f, 0.f, 0.f}; // Rest state counter before rettriger can happen

	dsp::PulseGenerator pgens[PARTS];
	dsp::SchmittTrigger edgeDetectors[PARTS];
	dsp::RCFilter noiseFilter;

	ParamId TRIGGERS_PARAM[PARTS] = {TRIGBD_PARAM, TRIGSNR_PARAM, TRIGHH_PARAM, TRIGHHO_PARAM, TRIGFX_PARAM};
	ParamId LENGTHS_PARAM[PARTS] = {LENGTHBD_PARAM, LENGTHSNR_PARAM, LENGTHHH_PARAM, LENGTHHH_PARAM, LENGTHFX_PARAM};

	BaBum() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(TUNEBD_PARAM, 0.f, 1.f, 0.f, "Tune Kick");
		configParam(TUNESNR_PARAM, 0.f, 1.f, 0.f, "Tune Snare");
		configParam(TUNEHH_PARAM, 1.f, 20000.f, 1.f, "HiHat HP Filter");
		configParam(TUNEFX_PARAM, 0.f, 1.f, 0.f, "Tune FX");

		configParam(LENGTHBD_PARAM, .001f, 5.f, 2.5f, "Kick Length");
		configParam(LENGTHSNR_PARAM, .001f, 5.f, 2.5f, "Snare Length");
		configParam(LENGTHHH_PARAM, .001f, 5.f, 2.5f, "HiHat Length");
		configParam(LENGTHFX_PARAM, .001f, 5.f, 2.5f, "FX Length");

		configParam(PARAMBD_PARAM, 1.f, 10.f, 1.f, "Kick distortion");
		configParam(PARAMSNR_PARAM, 1.f, 10.f, 1.f, "Snare distortion");
		configParam(PARAMFX_PARAM, 1.f, 49.f, 1.f, "FX distortion");

		configParam(TRIGBD_PARAM, 0.f, 1.f, 0.f, "Trigger Kick");
		configParam(TRIGSNR_PARAM, 0.f, 1.f, 0.f, "Trigger Snare");
		configParam(TRIGHH_PARAM, 0.f, 1.f, 0.f, "Trigger HiHat Closed");
		configParam(TRIGHHO_PARAM, 0.f, 1.f, 0.f, "Trigger HiHat Open");
		configParam(TRIGFX_PARAM, 0.f, 1.f, 0.f, "Trigger FX sound");

		configParam(MIXBD_PARAM, 0.f, 1.f, 0.5f, "BassDrum Mix");
		configParam(MIXSNR_PARAM, 0.f, 1.f, 0.5f, "Snare Mix");
		configParam(MIXHH_PARAM, 0.f, 1.f, 0.5f, "HiHat Closed Mix");
		configParam(MIXHHO_PARAM, 0.f, 1.f, 0.5f, "HiHat Open Mix");
		configParam(MIXFX_PARAM, 0.f, 1.f, 0.5f, "FX Mix");

		configInput(TUNEBD_INPUT, "Tune Kick");
		configInput(TUNESNR_INPUT, "Tune Snare");
		configInput(TUNEHH_INPUT, "Filter HiHat");
		configInput(TUNEFX_INPUT, "Tune FX Sound");

		configInput(BD_INPUT, "Trigger Kick");
		configInput(SNR_INPUT, "Trigger Snare");
		configInput(HH_INPUT, "Trigger HiHat Closed");
		configInput(HHO_INPUT, "Trigger HiHat Open");
		configInput(FX_INPUT, "Trigger FX Sound");

		configOutput(BD_OUTPUT, "Kick");
		configOutput(SNR_OUTPUT, "Snare");
		configOutput(HH_OUTPUT, "HiHat Closed");
		configOutput(HHO_OUTPUT, "HiHat Open");
		configOutput(FX_OUTPUT, "FX Sound");
		configOutput(MIX_OUTPUT, "Mix");
	}

	void process(const ProcessArgs& args) override {

		float noise = (random::uniform() - 0.5) * 2;
		float noiseTune = params[TUNEHH_PARAM].getValue() + (inputs[TUNEBD_INPUT].getVoltage() * 1000);
		noiseFilter.setCutoff(noiseTune / args.sampleRate);
		noiseFilter.process(noise);
		float filteredNoise = noiseFilter.highpass();

		float connectedInputs = 0.f;
		float generalMix = 0.f;

		for (int i = 0; i < PARTS; i++) {
			lights[LIGHTBD_LIGHT + i].setBrightness(0.f);
			if (!inputs[BD_INPUT + i].isConnected()) continue;
			connectedInputs += 1.f;

			float triggerValue = params[TRIGGERS_PARAM[i]].getValue();
			float gateRatio = 7.f - params[LENGTHS_PARAM[i]].getValue();

			if (triggerValue >= 0.01 || edgeDetectors[i].process(inputs[BD_INPUT+i].getVoltage())) {
				// Trigger if more than 0
				if (triggerPrevStates[i] == 0.0f || triggerRestStates[i] <= 0.0f) {
					oscRamps[i] = 0.0f;
					ampRamps[i] = 0.0f;
					pgens[i].trigger(1.0f / gateRatio);
					triggerRestStates[i] = args.sampleRate / REST_STATE;
				} else {
					triggerRestStates[i]--;
					triggerRestStates[i] = clamp(triggerRestStates[i], 0.0f, args.sampleRate);
				}
			}
			triggerPrevStates[i] = triggerValue; // Reset prev state of triggers

			// osc and ramps update
			ampRamps[i] += args.sampleTime * gateRatio; // Cycles per second
			if (ampRamps[i] >= 1.f)
				ampRamps[i] = 1.f; // -= 1.f to reset

			oscRamps[i] += args.sampleTime * BASE_FREQ; // Cycles per second
			if (oscRamps[i] >= 1.f)
				oscRamps[i] = 1.f; // amp Set to one so it doesnt make an abrupt noise

			float pgenState = pgens[i].process(args.sampleTime);
			float amp;
			float mix;

			// Specific code for each instrument
			if (i == 0) {
				float tune = clamp(params[TUNEBD_PARAM].getValue() + clamp(inputs[TUNEBD_INPUT].getVoltage() / 10.f, 0.f, 1.f), 0.f, 1.f);
				float drive = params[PARAMBD_PARAM].getValue(); // This could be another param
				amp = clamp(ampRamps[i] < CLIP_RATIO ? ampRamps[i] / CLIP_RATIO : pow(1 - ampRamps[i] + CLIP_RATIO, 2), 0.0f, 1.0f);
				float osc = clamp(sin(sqrt(oscRamps[i]) * ((tune * 200) + 50)) * drive, -1.0f, 1.0f);
				mix = osc * amp * params[MIXBD_PARAM].getValue(); ; // First part is the osc, second part is the amp then the mixer volume
			}

			if (i == 1) {
				float tune = clamp(params[TUNESNR_PARAM].getValue() + clamp(inputs[TUNESNR_INPUT].getVoltage() / 10.f, 0.f, 1.f), 0.f, 1.f);
				float drive = params[PARAMSNR_PARAM].getValue(); // This could be another param
				amp = clamp(ampRamps[i] < CLIP_RATIO ? ampRamps[i] / CLIP_RATIO : pow(1 - ampRamps[i] + CLIP_RATIO, 2), 0.0f, 1.0f);
				float amp2 = pow(1 - ampRamps[i] + CLIP_RATIO, 6);
				float osc = clamp(sin(sqrt(oscRamps[i]) * ((tune * 100) + 100)) * drive, -1.0f, 1.0f);
				mix = ((osc * amp) + (noise * 0.5 * amp2)) * params[MIXSNR_PARAM].getValue(); ; // First part is the osc, second part is the amp then the mixer volume
			}

			if (i == 2) {
				amp = clamp(ampRamps[i] < CLIP_RATIO ? ampRamps[i] / CLIP_RATIO : pow(1 - ampRamps[i] + CLIP_RATIO, 10), 0.0f, 1.0f);
				mix = filteredNoise * amp * params[MIXHH_PARAM].getValue(); ; // First part is the osc, second part is the amp then the mixer volume
			}

			if (i == 3) {
				amp = clamp(ampRamps[i] < CLIP_RATIO ? ampRamps[i] / CLIP_RATIO : pow(1 - ampRamps[i] + CLIP_RATIO, 2), 0.0f, 1.0f);
				mix = filteredNoise * amp * params[MIXHHO_PARAM].getValue(); ; // First part is the osc, second part is the amp then the mixer volume
			}

			if (i == 4) {
				float tune = clamp(params[TUNEFX_PARAM].getValue() + clamp(inputs[TUNEFX_INPUT].getVoltage() / 10.f, 0.f, 1.f), 0.f, 1.f);
				float drive = params[PARAMFX_PARAM].getValue(); // This could be another param
				float osc = clamp(sin(sqrt(oscRamps[i] * oscRamps[i] * oscRamps[i]) * ((tune * 1000) + 80)) * drive, -1.0f, 1.0f);
				amp = clamp(ampRamps[i] < CLIP_RATIO ? ampRamps[i] / CLIP_RATIO : pow(1 - ampRamps[i] + CLIP_RATIO, 2), 0.0f, 1.0f);
				mix = osc * amp * params[MIXFX_PARAM].getValue(); ; // First part is the osc, second part is the amp then the mixer volume
			}

			float mixV = mix * 10.f * pgenState;

			generalMix += mixV;

			outputs[BD_OUTPUT + i].setVoltage(mixV);
			lights[LIGHTBD_LIGHT + i].setBrightness(amp);
		}
		outputs[MIX_OUTPUT].setVoltage(generalMix / (connectedInputs + 0.0000000001));
	}
};


struct BaBumWidget : ModuleWidget {
	BaBumWidget(BaBum* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/BaBum.svg")));

		float hp = 5.08f;
		addChild(new TextDisplayWidget("BaBum", Vec(hp/2, hp*1.5), 14, -1));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		float minX4 = hp * 3.f;
		float maxX4 = 44.f;
		float div4 = (maxX4 - minX4) / 3.f;
		
		float ControlsY = 30.f;
		float SepY = 10.f;

		for (int i = 0; i < PARTS - 1; i++) {
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(minX4 + (div4 * float(i)), ControlsY + (SepY * 0))), module, BaBum::TUNEBD_PARAM + i));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(Vec(minX4 + (div4 * float(i)), ControlsY + (SepY * 1)))), module, BaBum::TUNEBD_INPUT + i));
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(minX4 + (div4 * float(i)), ControlsY + (SepY * 2))), module, BaBum::LENGTHBD_PARAM + i));
			if (i != 2) {
				addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(minX4 + (div4 * float(i)), ControlsY + (SepY * 3))), module, BaBum::PARAMBD_PARAM + i));
			}
		}

		std::string CONTROLS[3] = {"Tune", "Len", "Char"};
		for (int i = 1; i < PARTS - 1; i++) {
			addChild(new TextDisplayWidget(CONTROLS[i - 1], Vec(5.08f * 2, ControlsY + (SepY * i)), 9, 1));
		}

		float minX5 = 7.5f;
		float maxX5 = 44.f;
		float div5 = (maxX5 - minX5) / 4.f;

		float triggerY = 75.f;
		float mixY = 100.f;
		float outY = 110.f;

		std::string NAMES[PARTS] = {"Kck", "Snr", "HhC", "HhO", "Fx"};

		for (int i = 0; i < 5; i++) {
			addChild(new TextDisplayWidget(NAMES[i], Vec(minX5 + (div5 * float(i)), triggerY - 5.f), 13));
			addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(minX5 + (div5 * float(i)), triggerY)), module, BaBum::LIGHTBD_LIGHT + i));
			addParam(createParamCentered<VCVButton>(mm2px(Vec(minX5 + (div5 * float(i)), triggerY)), module, BaBum::TRIGBD_PARAM + i));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(minX5 + (div5 * float(i)), triggerY + 8.f)), module, BaBum::BD_INPUT + i));
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(minX5 + (div5 * float(i)), mixY)), module, BaBum::MIXBD_PARAM + i));
			addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(minX5 + (div5 * float(i)), outY)), module, BaBum::BD_OUTPUT + i));
		}
		addChild(new TextDisplayWidget("Mixer", Vec(minX5 + (div5 * 2), mixY - 7.f), 14));

		// TextDisplayWidget* mixText; // Para tener la variable
		addChild(new TextDisplayWidget("All", Vec((minX5 + div5 * 2) - 5.f, outY + 10.f), 10, 1));
		addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(minX5 + (div5 * 2), outY + 10.f)), module, BaBum::MIX_OUTPUT));		
	}
};


Model* modelBaBum = createModel<BaBum, BaBumWidget>("BaBum");