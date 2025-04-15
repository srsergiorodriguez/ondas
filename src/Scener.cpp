#include "plugin.hpp"

const int SIGNALS = 30;
const int COLUMNS = 5;
const int ROWS = 6;
const int MAX_STEPS = 16;
const int ALERTS = 2;

struct Scener : Module {
	enum ParamId {
		LOOP_PARAM,
		TRANSITION_PARAM,
		RESET_PARAM,
		SCENES_PARAM,
		ENUMS(ALERT_PARAM, ALERTS),
		ENUMS(STEPS_PARAM, ROWS),
		PARAMS_LEN
	};
	enum InputId {
		TRIGGER_INPUT,
		RESET_INPUT,
		ENUMS(SIGNAL_INPUT, SIGNALS),
		INPUTS_LEN
	};
	enum OutputId {
		ENUMS(ALERT_OUTPUT, ALERTS),
		ENUMS(SIGNAL_OUTPUT, COLUMNS),
		OUTPUTS_LEN
	};
	enum LightId {
		TRIGGER_LIGHT,
		ENUMS(ALERT_LIGHT, ALERTS),
		ENUMS(SCENE_LIGHT, ROWS),
		LIGHTS_LEN
	};

	dsp::SchmittTrigger edgeDetector;
	dsp::SchmittTrigger edgeDetectorReset;
	dsp::PulseGenerator pgenAlert[2];
	dsp::PulseGenerator pgenTrigger;

	int stepCount = 0;
	int sceneStepCount = 0;
	int currentScene = 0;
	int prevScene = 0;
	float ramp = 0.f;
	bool finished = false;
	bool starting = true;

	float TRIG_TIME = 1e-3f;

	Scener() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		for (int i = 0; i < SIGNALS; i++) {
			configInput(SIGNAL_INPUT + i, "Signal " + std::to_string(i % COLUMNS) + " scene " + std::to_string((int)floor(i / COLUMNS)));
		}

		for (int i = 0; i < COLUMNS; i++) {
			configOutput(SIGNAL_OUTPUT + i, "Signal " + std::to_string(i));
		}

		for (int i = 0; i < ROWS; i++) {
			configParam(STEPS_PARAM + i, 1.f, MAX_STEPS, 1.f, "Steps scene " + std::to_string(i));
		}

		for (int i = 0; i < ALERTS; i++) {
			configParam(ALERT_PARAM + i, 0.f, 1.f, 1.f, "Alert " + std::to_string(i));
			configOutput(ALERT_OUTPUT + i, "Alert " + std::to_string(i));
		}
		configInput(TRIGGER_INPUT, "Trigger");
		configParam(LOOP_PARAM, 1.f, 1.f, 0.f, "Loop toggle");
		configParam(TRANSITION_PARAM, 0.f, 3.f, 0.f, "Crossfade transition time");
		configInput(RESET_INPUT, "Reset");
		configButton(RESET_PARAM, "Reset");
		configParam(SCENES_PARAM, 1.f, (float)(ROWS), (float)(ROWS), "Scenes");

		lights[SCENE_LIGHT].setBrightness(1);
	}

	void process(const ProcessArgs& args) override {
		float inV = inputs[TRIGGER_INPUT].getVoltage();
		float trigger = edgeDetector.process(inV);
		float gateRatio = params[TRANSITION_PARAM].getValue();

		if (starting) {
			starting = false;
			for (int i = 0; i < ALERTS; i++) {
				float alert = (int)(params[STEPS_PARAM + currentScene].getValue() * params[ALERT_PARAM + i].getValue());
				lights[ALERT_LIGHT + i].setBrightness(alert == sceneStepCount ? 1.f : 0.f);
				if (alert == 0) {
					pgenAlert[i].trigger(TRIG_TIME);
				}
			}
		}

		lights[TRIGGER_LIGHT].setBrightness(0.f);

		if (trigger) {
			stepCount++;
			sceneStepCount++;
			prevScene = currentScene;
			currentScene = 0;

			pgenTrigger.trigger(0.1);
			lights[TRIGGER_LIGHT].setBrightness(1.f);

			int totalSteps = 0;
			for (int i = 0; i < params[SCENES_PARAM].getValue(); i++) {
				int steps = (int)(params[STEPS_PARAM + i].getValue());
				if (stepCount > totalSteps) {
					currentScene = i;
				}
				totalSteps += steps;
			}

			if (params[LOOP_PARAM].getValue()) {
				stepCount %= totalSteps;
			} else if (stepCount >= totalSteps && !finished) {
				finished = true;
				ramp = 0.f;
			}

			if (currentScene != prevScene) {
				sceneStepCount = 0;
				ramp = 0.f;
			}

			for (int i = 0; i < ALERTS; i++) {
				float alert = (int)(params[STEPS_PARAM + currentScene].getValue() * params[ALERT_PARAM + i].getValue());
				lights[ALERT_LIGHT + i].setBrightness(alert == sceneStepCount ? 1.f : 0.f);
				if (alert == sceneStepCount) {
					pgenAlert[i].trigger(TRIG_TIME);
				}
			}

			for (int i = 0; i < ROWS; i++) {
				lights[SCENE_LIGHT+i].setBrightness(0);
				if (currentScene == i) {
					lights[SCENE_LIGHT+i].setBrightness(1);
				}
			}	
		}

		ramp += args.sampleTime * (1.f / gateRatio); // Cycles per second
		if (ramp >= 1.f)
			ramp = 1.f;

		for (int i = 0; i < COLUMNS; i++) {
			float a = inputs[SIGNAL_INPUT + ((prevScene * COLUMNS) + i)].getVoltage();
			float b = inputs[SIGNAL_INPUT + ((currentScene * COLUMNS) + i)].getVoltage();
			if (finished) {
				outputs[SIGNAL_OUTPUT + i].setVoltage(a * (1 - ramp));
			} else {
				outputs[SIGNAL_OUTPUT + i].setVoltage((a * (1 - ramp)) + (b * ramp));
			}
		}

		for (int i = 0; i < ALERTS; i++) {
			outputs[ALERT_OUTPUT + i].setVoltage(10.f * pgenAlert[i].process(args.sampleTime));
		}

		lights[TRIGGER_LIGHT].setBrightness(pgenTrigger.process(args.sampleTime));	

		if (params[RESET_PARAM].getValue() || edgeDetectorReset.process(inputs[RESET_INPUT].getVoltage())) {
			stepCount = 0;
			currentScene = 0;
			finished = false;
		}

	}
};


struct ScenerWidget : ModuleWidget {
	ScenerWidget(Scener* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Scener.svg")));

		float hp = 5.08f;

		addChild(new TextDisplayWidget("Scener", Vec(hp/2, hp*1.5), 14, -1));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		float minX4 = hp * 2;
		float maxX4 = hp * 10;
		float divX4 = (maxX4 - minX4) / 3;

		float controlsY = hp * 4;
		float divYcontrols = hp * 2.8f;
		float tOffset = 6.f;

		addChild(new TextDisplayWidget("Trigger", Vec(minX4, controlsY - tOffset), 10));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(minX4, controlsY)), module, Scener::TRIGGER_INPUT));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(minX4 + divX4, controlsY)), module, Scener::TRIGGER_LIGHT));
		addChild(new TextDisplayWidget("Scenes", Vec(minX4 + (divX4 * 2), controlsY - tOffset), 10));
		addParam(createParamCentered<RoundSmallBlackSnapKnob>(mm2px(Vec(minX4 + (divX4 * 2), controlsY)), module, Scener::SCENES_PARAM));

		addChild(new TextDisplayWidget("Loop", Vec(minX4, (controlsY + (divYcontrols * 1)) - tOffset), 10));
		addParam(createParamCentered<CKSS>(mm2px(Vec(minX4, (controlsY + (divYcontrols * 1)))), module, Scener::LOOP_PARAM));
		addChild(new TextDisplayWidget("XFade", Vec(minX4 + (divX4), (controlsY + (divYcontrols * 1)) - tOffset), 10));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(minX4 + (divX4), (controlsY + (divYcontrols * 1)))), module, Scener::TRANSITION_PARAM));
		addChild(new TextDisplayWidget("Reset", Vec(minX4 + (divX4 * 2.5), (controlsY + (divYcontrols * 1)) - tOffset), 10));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(minX4 + (divX4 * 2), (controlsY + (divYcontrols * 1)))), module, Scener::RESET_INPUT));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(minX4 + (divX4 * 3), (controlsY + (divYcontrols * 1)))), module, Scener::RESET_PARAM));

		for (int i = 0; i < ALERTS; i++) {
			addChild(new TextDisplayWidget("Alert " + std::to_string(i), Vec(minX4 + (divX4 * (i * 2)), (controlsY + (divYcontrols * 2)) - tOffset), 10));
			addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(minX4 + (divX4 * (i * 2)), (controlsY + (divYcontrols * 2)))), module, Scener::ALERT_OUTPUT + i));
			addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(minX4 + (divX4/2) + (divX4 * (i * 2)), (controlsY + (divYcontrols * 2)))), module, Scener::ALERT_LIGHT + i));
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(minX4 + (divX4 * ((i * 2) + 1)), (controlsY + (divYcontrols * 2)))), module, Scener::ALERT_PARAM + i));
		}

		float minX = hp * 1.5;
		float maxX = 5.08f * 10.f;
		float divX = (maxX - minX) / COLUMNS;
		float minY = 64.f;
		float divY = divX;

		for (int i = 0; i < SIGNALS; i++) {
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(minX + (divX * (i % COLUMNS)), minY + (int(i / COLUMNS) * divY))), module, Scener::SIGNAL_INPUT + i));
		}

		for (int i = 0; i < COLUMNS; i++) {
			addChild(new TextDisplayWidget("Sig" + std::to_string(i), Vec(minX + (divX * i), minY - tOffset), 10));
			addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(minX + (divX * (i % COLUMNS)), minY + ((ROWS + 0.3f) * divY))), module, Scener::SIGNAL_OUTPUT + i));
		}

		for (int i = 0; i < ROWS; i++) {
			addParam(createParamCentered<RoundSmallBlackSnapKnob>(mm2px(Vec(minX + (divX * COLUMNS), minY + (i * divY))), module, Scener::STEPS_PARAM + i));
			addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(minX + (divX * (COLUMNS + 1)), minY + (i * divY))), module, Scener::SCENE_LIGHT + i));
		}
	}
};


Model* modelScener = createModel<Scener, ScenerWidget>("Scener");