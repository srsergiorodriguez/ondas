#include "plugin.hpp"


struct Norbert : Module {
	enum ParamId {
		SPRING_PARAM,    // spring constant k
		SPRING_ATTENUATOR_PARAM,
		DAMPING_PARAM,   // damping constant d
		DAMPING_ATTENUATOR_PARAM,
		IMPULSE_PARAM,   // how strong each trigger kick is
		PARAMS_LEN
	};
	enum InputId {
		TRIG_INPUT,
		SPRING_INPUT,
		DAMPING_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		ANGLE_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	float angle = 0.0f;        // x
	float velocity = 0.0f;     // x'
	dsp::SchmittTrigger trig;

	float springFactor = 200.f;
	const float kMin =  5.f;
	const float kMax = 1000.f;

	Norbert() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		configParam(SPRING_PARAM,  0.0f,  1.0f, 0.2f, "Spring");
		configInput(SPRING_INPUT, "Spring");
		configParam(SPRING_ATTENUATOR_PARAM,  0.0f,  1.0f, 0.0f, "Spring CV attenuator");

		configParam(DAMPING_PARAM, 0.0f,  1.0f,  0.1f, "Damping");
		configInput(DAMPING_INPUT, "Damping");
		configParam(DAMPING_ATTENUATOR_PARAM,  0.0f,  1.0f, 0.0f, "Damping CV attenuator");

		configParam(IMPULSE_PARAM, 0.0f,  10.0f,  10.0f, "Impulse strength");

		configInput(TRIG_INPUT, "Trigger");

		configOutput(ANGLE_OUTPUT, "Angle");
	}

	void process(const ProcessArgs& args) override {

		// spring-damper dynamics:  a = -k * x - d * v
		float t = params[SPRING_PARAM].getValue();
		float kCV = (inputs[SPRING_INPUT].getVoltage() * params[SPRING_ATTENUATOR_PARAM].getValue()) / 10.f;
		float kSum = clamp(t + kCV, 0.0f, 1.0f);
		float k = kMin * powf(kMax/kMin, kSum);

		// Change velocity on trigger
		if (trig.process(inputs[TRIG_INPUT].getVoltage())) {
			float baseImp = params[IMPULSE_PARAM].getValue();
			float comp = powf(kMin / k, 0.1f);
			velocity += baseImp * comp;
		}

		float dParam = params[DAMPING_PARAM].getValue();
		float dCV = (inputs[DAMPING_INPUT].getVoltage() * params[DAMPING_ATTENUATOR_PARAM].getValue()) / 10.f;
		float d = clamp(dParam + dCV, 0.0f, 1.0f) * 10.f;

		float accel = -k * angle - d * velocity;

		velocity += accel * args.sampleTime;
		angle    += velocity * args.sampleTime;

		// Output cv
		float angleAmp = angle * 1.f;
		float out = clamp(angleAmp, -10.0f, 10.0f);
		outputs[ANGLE_OUTPUT].setVoltage(out);
	}
};


struct NorbertWidget : ModuleWidget {
	NorbertWidget(Norbert* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Norbert.svg")));

		float hp = 5.08f;
		float u = 133.35f;
		float yDiv = u / 14.f;

		addChild(new TextDisplayWidget("Nor", Vec(hp/2, hp*1.5), 14, -1));
		addChild(new TextDisplayWidget("bert", Vec(hp/2, hp*2), 14, -1));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		float xMin = hp * 2.5;
		float yMin = yDiv * 1.5;

		addChild(new TextDisplayWidget("k", Vec(xMin, yMin + (yDiv) - 7.f), 10, 0));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(xMin, yMin + (yDiv))), module, Norbert::SPRING_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(xMin - hp, yMin + (yDiv * 2))), module, Norbert::SPRING_INPUT));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(xMin + hp, yMin + (yDiv * 2))), module, Norbert::SPRING_ATTENUATOR_PARAM));

		addChild(new TextDisplayWidget("d", Vec(xMin, yMin + (yDiv * 4) - 7.f), 10, 0));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(xMin, yMin + (yDiv * 4))), module, Norbert::DAMPING_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(xMin - hp, yMin + (yDiv * 5))), module, Norbert::DAMPING_INPUT));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(xMin + hp, yMin + (yDiv * 5))), module, Norbert::DAMPING_ATTENUATOR_PARAM));

		addChild(new TextDisplayWidget("Strength", Vec(xMin, yMin + (yDiv * 7) - 7.f), 10, 0));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(xMin, yMin + (yDiv * 7))), module, Norbert::IMPULSE_PARAM));

		addChild(new TextDisplayWidget("Trigger", Vec(xMin, yMin + (yDiv * 9) - 7.f), 10, 0));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(xMin, yMin + (yDiv * 9))), module, Norbert::TRIG_INPUT));

		addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(xMin, yMin + (yDiv * 10.5))), module, Norbert::ANGLE_OUTPUT));
	}
};


Model* modelNorbert = createModel<Norbert, NorbertWidget>("Norbert");