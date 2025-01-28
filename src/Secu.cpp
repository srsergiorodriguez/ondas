#include "plugin.hpp"
#include <cmath>

const int MAX_STEPS = 8;
const int OUTPUTS = 5;

struct Secu : Module {

	enum ParamId {
		RANDOM_PARAM,
		SPARSERND_PARAM,
		PROB_PARAM,
		STEPS_PARAM,
		ENUMS(COLUMN0_PARAM, MAX_STEPS),
		ENUMS(COLUMN1_PARAM, MAX_STEPS),
		ENUMS(COLUMN2_PARAM, MAX_STEPS),
		ENUMS(COLUMN3_PARAM, MAX_STEPS),
		ENUMS(COLUMN4_PARAM, MAX_STEPS),
		PARAMS_LEN
	};
	enum InputId {
		RESET_INPUT,
		TRIGGER_INPUT,
		PROB_INPUT,
		RANDOM_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		ENUMS(OUTPUT, OUTPUTS),
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(STEPLIGHT, MAX_STEPS),
		LIGHTS_LEN
	};

	int stepNr = 0; // Consecutive step increments
	int stepOut = stepNr; // Step after chance is applied
	dsp::SchmittTrigger edgeDetector;
	dsp::SchmittTrigger edgeDetectorReset;
	dsp::SchmittTrigger edgeDetectorRandom;
	float restRandomize = 0.0f;
	float prevRandomizeState = 0.0f;

	ParamId COLUMNS[5] = {COLUMN0_PARAM,  COLUMN1_PARAM,  COLUMN2_PARAM,  COLUMN3_PARAM,  COLUMN4_PARAM};

	void randomizeSteps() {
		for (int i = 0; i < MAX_STEPS; i++) {
			for (int j = 0; j < 5; j++) {
				bool on = params[SPARSERND_PARAM].getValue() > random::uniform();
				params[COLUMNS[j] + i].setValue(on ? 1.f : 0.f);
			}
		}
	}

	Secu() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		configButton(RANDOM_PARAM,"Randomize steps");
		configParam(SPARSERND_PARAM, 0.f, 1.f, 0.3f, "Randomization sparseness");
		configParam(PROB_PARAM, 0.f, 1.f, 0.2f, "Glitch probability");
		configParam(STEPS_PARAM, 1, 8, 8, "Sequence steps");

		for (int i = 0; i < MAX_STEPS; i++) {
			for (int j = 0; j < 5; j++) {
				configSwitch(COLUMNS[j] + i, 0, 1, 0, "Set gate " + std::to_string(j) + "-" + std::to_string(i), {"Off", "On"});
			}
		}

		configInput(RESET_INPUT, "Reset");
		configInput(TRIGGER_INPUT, "Trigger");
		configInput(PROB_INPUT, "Probability");
		configInput(RANDOM_INPUT, "Randomize");

		for (int i = 0; i < OUTPUTS; i++) {
			configOutput(OUTPUT + i, "Trigger " + std::to_string(i) + "Out");
		}
	}

	void process(const ProcessArgs& args) override {
		float inV = inputs[TRIGGER_INPUT].getVoltage();
		float trigger = edgeDetector.process(inV);
		int stepsLength = params[STEPS_PARAM].getValue();

		if (edgeDetectorReset.process(inputs[RESET_INPUT].getVoltage())) {
			stepNr = 0;
		}

		if (params[RANDOM_PARAM].getValue() > 0.1) {
			// Randomize
			if (prevRandomizeState == 0.0f || restRandomize <= 0.0f) {
				randomizeSteps();
				restRandomize = args.sampleRate / 4;
			} else {
				restRandomize--;
				restRandomize = clamp(restRandomize, 0.0f, args.sampleRate);
			}
		}

		if (edgeDetectorRandom.process(inputs[RANDOM_INPUT].getVoltage())) {
			randomizeSteps();
		}

		if (trigger) {
			float chance = clamp(params[PROB_PARAM].getValue() + inputs[PROB_INPUT].getVoltage(), 0.0f, 1.0f) > random::uniform();
			stepOut = chance ? int(floor(random::uniform() * stepsLength)) : stepNr;
			
			for (int i = 0; i < MAX_STEPS; i++) {
				if (stepOut == i) {
					lights[STEPLIGHT+stepOut].setSmoothBrightness(trigger, 5e-6f);
				} else {
					lights[STEPLIGHT+i].setBrightness(0);
				}
			}

			stepNr++;
			stepNr = stepNr % stepsLength;
		}
		
		for (int j = 0; j < 5; j++) {
			outputs[OUTPUT+j].setVoltage(0.0f);
			if (params[COLUMNS[j] + stepOut].getValue() >= 0.1 && outputs[OUTPUT+j].isConnected()) {
				outputs[OUTPUT+j].setVoltage(inV);
			}
		}
		prevRandomizeState = params[RANDOM_PARAM].getValue();
	}
	
};


struct SecuWidget : ModuleWidget {
	SecuWidget(Secu* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Secu.svg")));

		float hp = 5.08f;
		addChild(new TextDisplayWidget("Secu", Vec(hp/2, hp*1.5), 14, -1));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		float inputsY = 19.f;
		float divY = hp * 2.6f;
		float minX3 = hp * 1.5f;
		float maxX3 = 44.f;
		float div3 = (maxX3 - minX3) / 3.f;
		float tOffset = 6.f;

		addChild(new TextDisplayWidget("Rst", Vec(minX3, inputsY - tOffset), 10));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(minX3, inputsY)), module, Secu::RESET_INPUT));
		addChild(new TextDisplayWidget("Trig", Vec(minX3 + div3, inputsY - tOffset), 10));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(minX3 + div3, inputsY)), module, Secu::TRIGGER_INPUT));

		addChild(new TextDisplayWidget("Rnd", Vec(minX3 + (div3 * 2), inputsY - tOffset), 10));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(minX3 + (div3 * 2), inputsY)), module, Secu::RANDOM_INPUT));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(minX3 + (div3 * 2), inputsY + divY)), module, Secu::RANDOM_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(minX3 + (div3 * 2), inputsY + (divY * 2))), module, Secu::SPARSERND_PARAM));

		addChild(new TextDisplayWidget("Prob", Vec(minX3 + (div3 * 0.5f), inputsY + divY - tOffset), 10));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(minX3, inputsY + divY)), module, Secu::PROB_INPUT));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(minX3 + div3, inputsY + divY)), module, Secu::PROB_PARAM));
		
		addChild(new TextDisplayWidget("Steps", Vec(minX3 + div3, inputsY + (divY * 2) - tOffset), 10));
		addParam(createParamCentered<RoundSmallBlackSnapKnob>(mm2px(Vec(minX3 + div3, inputsY + (divY * 2))), module, Secu::STEPS_PARAM));

		float btnY = 54.f;
		float btnX = hp*1.3;
		float divXBtn = 7.f;
		float divYBtn = 7.f;

		Secu::ParamId COLUMNS[5] = {Secu::COLUMN0_PARAM,  Secu::COLUMN1_PARAM,  Secu::COLUMN2_PARAM,  Secu::COLUMN3_PARAM,  Secu::COLUMN4_PARAM};

		for (int i = 0; i < MAX_STEPS; i++) {
			for (int j = 0; j < 5; j++) {
				addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(btnX + (divXBtn * j), btnY + (divYBtn * i))), module, Secu::STEPLIGHT + i));
				addParam(createParamCentered<StateButton>(mm2px(Vec(btnX + (divXBtn * j), btnY + (divYBtn * i))), module, COLUMNS[j] + i));
			}
		}

		float outY = 112.f;
		float divYOut = 8.f;

		addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(6.415, outY)), module, Secu::OUTPUT));
		addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(20.435, outY)), module, Secu::OUTPUT+2));
		addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(34.456, outY)), module, Secu::OUTPUT+4));
		addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(13.425, outY + divYOut)), module, Secu::OUTPUT+1));
		addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(27.445, outY + divYOut)), module, Secu::OUTPUT+3));
	}
};


Model* modelSecu = createModel<Secu, SecuWidget>("Secu");