#include "plugin.hpp"

const int DATAPOINTS = 1024;
const int DATAX = 16;
const int DATAY = DATAPOINTS / DATAX;

struct Logistic : Module {
	enum ParamId {
		R_PARAM,
		R_INPUT_ATTENUATOR_PARAM,
		RESET_PARAM,
		RESET_VALUE_PARAM,
		SLEW_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		TICK_INPUT,
		R_INPUT,
		RESET_INPUT,
		SLEW_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		FUNCTION_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	dsp::SchmittTrigger tickEdgeDetector;
	dsp::SchmittTrigger resetEdgeDetector;

	float n = 0.5f;
	float nextN = 0.5f;
	float rSum = 0.f;

	const float minRate = 10.f;    // per second, at k=0
	const float maxRate = 20000.f; // per second, at k=1
	const float exponent = 6.f;

	// Plot variables

	Vec plotPoints[DATAPOINTS] = {};

	Logistic() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(R_PARAM, 1.1f, 3.9f, 3.f, "r");
		configParam(R_INPUT_ATTENUATOR_PARAM, 0.f, 1.f, 0.f, "r CV attenuator");
		configParam(RESET_PARAM, 0.f, 1.f, 0.f, "Trigger n reset");
		configParam(RESET_VALUE_PARAM, 0.1f, 0.9f, 0.5f, "Reset n value");
		configParam(SLEW_PARAM, 0.f, 1.f, 0.f, "Slew");

		configInput(TICK_INPUT, "Tick for next iteration");
		configInput(R_INPUT, "r");

		configOutput(FUNCTION_OUTPUT, "Function");

		float nPlot = 0.5;
		for (int y = 0; y < DATAY; y++) {
			float nn = nPlot;
			float ny = float(y) / float(DATAY - 1);  // 0…1
			float r = 1.1f + (ny * (3.9f - 1.1f));
			for (int i = 0; i < 20; i++) {
				nn = r * nn * (1 - nn);
			}
			for (int x = 0; x < DATAX; x++) {
				nn = r * nn * (1 - nn);
				float nx = nn;
				plotPoints[(y * DATAX) + x] = Vec(nx, ny);
			};
		}
	}

	void process(const ProcessArgs& args) override {
		if (params[RESET_PARAM].getValue() || resetEdgeDetector.process(inputs[RESET_INPUT].getVoltage())) {
			n = nextN = params[RESET_VALUE_PARAM].getValue();
		}

		float r = params[R_PARAM].getValue();
		float rCV = inputs[R_INPUT].getVoltage() * params[R_INPUT_ATTENUATOR_PARAM].getValue();
		rSum = clamp(r + rCV, 1.1f, 3.9f);

		if (tickEdgeDetector.process(inputs[TICK_INPUT].getVoltage())) {
			nextN = rSum * n * (1.0f - n);
		}

		float slew = params[SLEW_PARAM].getValue();
		float dt = args.sampleTime; // Seconds per sample
		float curve = powf(1.f - slew, exponent);
		float rate = minRate + (maxRate - minRate) * curve;

		n += (nextN - n) * (rate * dt);

		outputs[FUNCTION_OUTPUT].setVoltage(n * 5.f);
	}
};

struct LogisticWidget : ModuleWidget {
	LogisticWidget(Logistic* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Logistic.svg")));

		float hp = 5.08f;
		float u = 133.35f;
		float yDiv = u / 14.f;

		addChild(new TextDisplayWidget("Logistic", Vec(hp/2, hp*1.5), 14, -1));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		if (module) {
			Vec plotPos = mm2px(Vec(hp, yDiv*1.5));
			Vec plotSize = mm2px(Vec(hp*4, yDiv*3.5));
			addChild(new LogisticMapWidget(plotPos, plotSize, &module->rSum, module->plotPoints, DATAPOINTS));
		}
		
		addChild(new TextDisplayWidget("tick", Vec(hp*3, (yDiv*6) - 6.f), 10, 0));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(hp*3, (yDiv*6))), module, Logistic::TICK_INPUT));

		addChild(new TextDisplayWidget("r", Vec(hp*3, (yDiv*7.5) - 7.f), 10, 0));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(hp*3, yDiv*7.5)), module, Logistic::R_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(hp*2, (yDiv*8.5))), module, Logistic::R_INPUT));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(hp*4, yDiv*8.5)), module, Logistic::R_INPUT_ATTENUATOR_PARAM));

		addChild(new TextDisplayWidget("slew", Vec(hp*3, (yDiv*10) - 6.f), 10, 0));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(hp*4, yDiv*10)), module, Logistic::SLEW_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(hp*2, (yDiv*10))), module, Logistic::SLEW_INPUT));

		addChild(new TextDisplayWidget("rst", Vec(hp*3, (yDiv*11.5) - 6.f), 10, 0));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(hp*1.5, (yDiv*11.5))), module, Logistic::RESET_INPUT));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(hp*3, yDiv*11.5)), module, Logistic::RESET_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(hp*4.5, yDiv*11.5)), module, Logistic::RESET_VALUE_PARAM));

		addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(hp*3, u - (yDiv*1.5))), module, Logistic::FUNCTION_OUTPUT));
	}
};

Model* modelLogistic = createModel<Logistic, LogisticWidget>("Logistic");