#include "plugin.hpp"

const int SETS = 4;

struct Hiller : Module {
	enum ParamId {
		ENUMS(COMPARATOR_PARAM, SETS),
		PARAMS_LEN
	};
	enum InputId {
		ENUMS(REFERENCE_INPUT, SETS),
		ENUMS(A_INPUT, SETS),
		ENUMS(B_INPUT, SETS),
		INPUTS_LEN
	};
	enum OutputId {
		ENUMS(A_OUTPUT, SETS),
		ENUMS(B_OUTPUT, SETS),
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	Hiller() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		for (int i = 0; i < SETS; i++) {
			configInput(A_INPUT, "A " + std::to_string(i));
			configInput(B_INPUT, "B " + std::to_string(i));

			configParam(COMPARATOR_PARAM + i, -10.f, 10.f, 0.f, "Comparator " + std::to_string(i));
			configInput(REFERENCE_INPUT, "Reference " + std::to_string(i));

			configOutput(A_OUTPUT, "A " + std::to_string(i));
			configOutput(B_OUTPUT, "B " + std::to_string(i));
		}
	}

	void process(const ProcessArgs& args) override {

		for (int i = 0; i < SETS; i++) {
			float referenceValue = inputs[REFERENCE_INPUT + i].getVoltage();
			float threshold = params[COMPARATOR_PARAM + i].getValue();
			bool doChange = referenceValue > threshold;

			float aInput = inputs[A_INPUT + i].getVoltage();
			float bInput = inputs[B_INPUT + i].getVoltage();
		
			if (doChange) {
				outputs[A_OUTPUT + i].setVoltage(bInput);
				outputs[B_OUTPUT + i].setVoltage(aInput);
			} else {
				outputs[A_OUTPUT + i].setVoltage(aInput);
				outputs[B_OUTPUT + i].setVoltage(bInput);
			}
		}

	}
};


struct HillerWidget : ModuleWidget {
	HillerWidget(Hiller* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Hiller.svg")));

		float hp = 5.08f;
		float u = 133.35f;
		float yDiv = u / 14.f;

		float xBase = hp * 1.5;
		float yBase = yDiv * 2.5;
		float xSep = hp * 3;
		float ySep = 2.6;

		addChild(new TextDisplayWidget("Hiller", Vec(hp/2, hp*1.5), 14, -1));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		for (int i = 0; i < SETS; i++) {
			float yBlock = yBase + (yDiv * i * ySep);
			float xBlock = i % 2 == 1 ? xBase + (xSep * 2) : xBase;
			addChild(new TextDisplayWidget("A", Vec(xBlock, yBlock - 6), 16));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(xBlock, yBlock)), module, Hiller::A_INPUT + i));
			addChild(new TextDisplayWidget("B", Vec(xBlock + xSep, yBlock - 6), 16));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(xBlock + xSep, yBlock)), module, Hiller::B_INPUT + i));

			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(xBlock, yBlock + yDiv)), module, Hiller::REFERENCE_INPUT + i));
			addChild(new TextDisplayWidget(">", Vec(xBlock + (xSep/2), yBlock + yDiv), 20));
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(xBlock + xSep, yBlock + yDiv)), module, Hiller::COMPARATOR_PARAM + i));

			addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(xBlock, yBlock + (yDiv * 2))), module, Hiller::A_OUTPUT + i));
			addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(xBlock + xSep, yBlock + (yDiv * 2))), module, Hiller::B_OUTPUT + i));
		}
	}
};


Model* modelHiller = createModel<Hiller, HillerWidget>("Hiller");