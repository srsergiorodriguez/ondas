#include "plugin.hpp"

const int PARTSNR = 4;
const std::string NAMES[PARTSNR] = {"Kck", "Snr", "Hh", "Fx"};

struct Bap : Module {
	enum ParamId {
		PARAMS_LEN
	};
	enum InputId {
		LENGTHBD_INPUT,
		LENGTHSNR_INPUT,
		LENGTHHH_INPUT,
		LENGTHFX_INPUT,
		PARAMBD_INPUT,
		PARAMSNR_INPUT,
		NONE,
		PARAMFX_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	BabumExpanderMessage leftMessages[2][1];

	Bap() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		configInput(LENGTHBD_INPUT, "Kick Length");
		configInput(LENGTHSNR_INPUT, "Snare Length");
		configInput(LENGTHHH_INPUT, "HiHat Length");
		configInput(LENGTHFX_INPUT, "FX Sound Length");

		configInput(PARAMBD_INPUT, "Kick distortion");
		configInput(PARAMSNR_INPUT, "Snare distortion");
		configInput(PARAMFX_INPUT, "FX distortion");

		leftExpander.producerMessage = leftMessages[0];
		leftExpander.consumerMessage = leftMessages[1];	
	}

	void process(const ProcessArgs& args) override {
		float kickLength = inputs[LENGTHBD_INPUT].getVoltage();
		float snareLength = inputs[LENGTHSNR_INPUT].getVoltage();
		float hihatLength = inputs[LENGTHHH_INPUT].getVoltage();
		float fxLength = inputs[LENGTHFX_INPUT].getVoltage();

		float kickDistortion = inputs[PARAMBD_INPUT].getVoltage();
		float snareDistortion = inputs[PARAMSNR_INPUT].getVoltage();
		float fxDistortion = inputs[PARAMFX_INPUT].getVoltage();

		bool hasExpander = (leftExpander.module && leftExpander.module->model == modelBaBum);
		if (hasExpander) {
			// BabumExpanderMessage *messagesFromModule = (BabumExpanderMessage *)(leftExpander.consumerMessage);
			// someValueFromTheModule = messagesFromModule->someThingToSendToTheExpander;
			
			BabumExpanderMessage *messagesToModule = (BabumExpanderMessage *)(leftExpander.producerMessage);

			messagesToModule->kickLength = kickLength;
			messagesToModule->snareLength = snareLength;
			messagesToModule->hihatLength = hihatLength;
			messagesToModule->fxLength = fxLength;

			messagesToModule->kickDistortion = kickDistortion;
			messagesToModule->snareDistortion = snareDistortion;
			messagesToModule->fxDistortion = fxDistortion;
		}
	}
};


struct BapWidget : ModuleWidget {
	BapWidget(Bap* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Bap.svg")));

		float hp = 5.08f;
		float u = 133.35f;
		float yDiv = u / 14.f;

		addChild(new TextDisplayWidget("Bap", Vec(hp/2, hp*1.5), 14, -1));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addChild(new TextDisplayWidget("Length", Vec(hp/2, yDiv*2), 12, -1));
		addChild(new TextDisplayWidget("Distort", Vec(hp/2, yDiv*8), 12, -1));

		for (int i = 0; i < PARTSNR; i++) {
			float yPos = (yDiv*3) + (yDiv*i);
			addChild(new TextDisplayWidget(NAMES[i], Vec(hp*1.5, yPos), 10, 1));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(hp*2.5, yPos)), module, Bap::LENGTHBD_INPUT + i));

			float yPos2 = (yDiv*9) + (yDiv*i);
			if (i != 2) {
				addChild(new TextDisplayWidget(NAMES[i], Vec(hp*1.5, yPos2), 10, 1));
				addInput(createInputCentered<PJ301MPort>(mm2px(Vec(hp*2.5, yPos2)), module, Bap::PARAMBD_INPUT + i));
			}
		}
		
	}
};


Model* modelBap = createModel<Bap, BapWidget>("Bap");