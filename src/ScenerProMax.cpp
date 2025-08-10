#include "plugin.hpp"

const int COLUMNS = 6; // Inputs
const int ROWS = 16; // Scenes
const int SIGNALS = COLUMNS * ROWS;
const int MAX_STEPS = 32;
const int ALERTS = 4;

struct ScenerProMax : Module {
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
	dsp::PulseGenerator pgenAlert[ALERTS];
	dsp::PulseGenerator pgenTrigger;

	int stepCount = 0;
	int sceneStepCount = 0;
	int currentScene = 0;
	int prevScene = 0;
	float ramp = 0.f;
	bool finished = false;
	bool starting = true;

	float TRIG_TIME = 1e-2f;

	ScenerProMax() {
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
		configParam(LOOP_PARAM, 0.f, 1.f, 1.f, "Loop toggle");
		configParam(TRANSITION_PARAM, 0.f, 20.f, 0.f, "Crossfade transition time (Seconds)");
		configInput(RESET_INPUT, "Reset");
		configButton(RESET_PARAM, "Reset");
		configParam(SCENES_PARAM, 1.f, (float)(ROWS), (float)(ROWS), "Scenes");

		lights[SCENE_LIGHT].setBrightness(1);
	}

	void process(const ProcessArgs& args) override {
		float inV = inputs[TRIGGER_INPUT].getVoltage();
		float trigger = edgeDetector.process(inV);
		float transitionTime = params[TRANSITION_PARAM].getValue();

		lights[TRIGGER_LIGHT].setBrightness(0.f);

		if (params[RESET_PARAM].getValue() || edgeDetectorReset.process(inputs[RESET_INPUT].getVoltage())) {
			// Reset all values
			stepCount = 0;
			sceneStepCount = 0;
			currentScene = 0;
			prevScene = 0;
			ramp = 0.f;
			finished = false;
			starting = true;
			for (int i = 0; i < ROWS; i++) {
				lights[SCENE_LIGHT+i].setBrightness(0);
				if (currentScene == i) {
					lights[SCENE_LIGHT+i].setBrightness(1);
				}
			}
		}

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

		if (trigger) {
			stepCount++; // All steps elapsed
			sceneStepCount++; // Steps elapsed in current scene

			pgenTrigger.trigger(0.1f);
			lights[TRIGGER_LIGHT].setBrightness(1.f);

			int totalSteps = 0; // Get all steps in total for all the scenes
			for (int i = 0; i < params[SCENES_PARAM].getValue(); i++) {
				int steps = (int)(params[STEPS_PARAM + i].getValue());
				totalSteps += steps;
			}

			if (stepCount >= totalSteps) {
				if (params[LOOP_PARAM].getValue()) {
					sceneStepCount = 0;
					stepCount = 0;
				} else {
					finished = true;
				}
			}

			int stepsIncr = 0;
			int tempCurrentScene;
			for (int i = 0; i < params[SCENES_PARAM].getValue(); i++) {
				int steps = (int)(params[STEPS_PARAM + i].getValue());
				if (stepCount >= stepsIncr) {
					tempCurrentScene = i; // Set current scene based on steps elapsed
				}
				stepsIncr += steps;
			}

			for (int i = 0; i < ALERTS; i++) {
				// Turn on alert if current scenStepCount corresponds to the percentage of the total scene specified in the alert
				float alert = (int)(params[STEPS_PARAM + currentScene].getValue() * params[ALERT_PARAM + i].getValue());
				lights[ALERT_LIGHT + i].setBrightness(alert == sceneStepCount ? 1.f : 0.f);
				if (alert == sceneStepCount) {
					pgenAlert[i].trigger(TRIG_TIME);
				}
			}

			if (tempCurrentScene != currentScene) {
				// Scene changed
				prevScene = currentScene;
				sceneStepCount = 0;
				ramp = 0.f; // Set ramp to 0 so it starts incrementing again
			}
			currentScene = tempCurrentScene;

			for (int i = 0; i < ROWS; i++) {
				lights[SCENE_LIGHT+i].setBrightness(0);
				if (currentScene == i) {
					lights[SCENE_LIGHT+i].setBrightness(1);
				}
			}
		}

		// Set the ramp for crossfading
		float step = (transitionTime > 0.f) ? args.sampleTime / transitionTime : 1.f;
		ramp += step;
		if (ramp >= 1.f)
			ramp = 1.f;

		for (int i = 0; i < COLUMNS; i++) {
			float a = inputs[SIGNAL_INPUT + ((prevScene * COLUMNS) + i)].getVoltage();
			float b = inputs[SIGNAL_INPUT + ((currentScene * COLUMNS) + i)].getVoltage();
			// Apply crossfade
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
	}
};


struct ScenerProMaxWidget : ModuleWidget {
	ScenerProMaxWidget(ScenerProMax* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/ScenerProMax.svg")));

		float hp = 5.08f;

		addChild(new TextDisplayWidget("Scener Pro Max", Vec(hp/2, hp*1.5), 14, -1));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		float minX4 = hp * 2;
		float maxX4 = hp * 10;
		float divX4 = (maxX4 - minX4) / 3;

		float controlsY = hp * 4;
		float divYcontrols = hp * 3.2f;
		float tOffset = 6.f;

		addChild(new TextDisplayWidget("Trigger", Vec(minX4, controlsY - tOffset), 10));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(minX4, controlsY)), module, ScenerProMax::TRIGGER_INPUT));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(minX4 + divX4, controlsY)), module, ScenerProMax::TRIGGER_LIGHT));
		addChild(new TextDisplayWidget("Scenes", Vec(minX4 + (divX4 * 2), controlsY - tOffset), 10));
		addParam(createParamCentered<RoundSmallBlackSnapKnob>(mm2px(Vec(minX4 + (divX4 * 2), controlsY)), module, ScenerProMax::SCENES_PARAM));

		addChild(new TextDisplayWidget("Loop", Vec(minX4, (controlsY + (divYcontrols * 1)) - tOffset), 10));
		addParam(createParamCentered<CKSS>(mm2px(Vec(minX4, (controlsY + (divYcontrols * 1)))), module, ScenerProMax::LOOP_PARAM));
		addChild(new TextDisplayWidget("XFade", Vec(minX4 + (divX4), (controlsY + (divYcontrols * 1)) - tOffset), 10));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(minX4 + (divX4), (controlsY + (divYcontrols * 1)))), module, ScenerProMax::TRANSITION_PARAM));
		addChild(new TextDisplayWidget("Reset", Vec(minX4 + (divX4 * 2.5), (controlsY + (divYcontrols * 1)) - tOffset), 10));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(minX4 + (divX4 * 2), (controlsY + (divYcontrols * 1)))), module, ScenerProMax::RESET_INPUT));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(minX4 + (divX4 * 3), (controlsY + (divYcontrols * 1)))), module, ScenerProMax::RESET_PARAM));

		float sepXAlerts = hp*16;
		for (int i = 0; i < ALERTS; i++) {
			addChild(new TextDisplayWidget("Alert " + std::to_string(i), Vec(sepXAlerts + minX4 + (divX4 * (i * 2)), (controlsY + (divYcontrols * 1)) - tOffset), 10));
			addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(sepXAlerts + minX4 + (divX4 * (i * 2)), (controlsY + (divYcontrols * 1)))), module, ScenerProMax::ALERT_OUTPUT + i));
			addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(sepXAlerts + minX4 + (divX4/2) + (divX4 * (i * 2)), (controlsY + (divYcontrols * 1)))), module, ScenerProMax::ALERT_LIGHT + i));
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(sepXAlerts + minX4 + (divX4 * ((i * 2) + 1)), (controlsY + (divYcontrols * 1)))), module, ScenerProMax::ALERT_PARAM + i));
		}

		float minX = hp * 1.5;
		float maxX = 5.08f * 13.f;
		float divY = (maxX - minX) / COLUMNS;
		float minY = 68.f;
		float divX = hp * 2.1;

		for (int i = 0; i < SIGNALS; i++) {
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(minX + divX + (int(i / COLUMNS) * divX), minY + (divY * (i % COLUMNS)))), module, ScenerProMax::SIGNAL_INPUT + i));
		}

		for (int i = 0; i < COLUMNS; i++) {
			addChild(new TextDisplayWidget("Sig" + std::to_string(i), Vec(minX, minY + (divY * i)), 10));
			addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(minX + (divX * (ROWS + 1)), minY + (i * divY))), module, ScenerProMax::SIGNAL_OUTPUT + i));
		}

		for (int i = 0; i < ROWS; i++) {
			addParam(createParamCentered<RoundSmallBlackSnapKnob>(mm2px(Vec(minX + (divX * (i+1)), minY - divY * 1)), module, ScenerProMax::STEPS_PARAM + i));
			addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(minX + (divX * (i+1)), minY - divY * 2.0)), module, ScenerProMax::SCENE_LIGHT + i));
		}
	}
};


Model* modelScenerProMax = createModel<ScenerProMax, ScenerProMaxWidget>("ScenerProMax");