#include "plugin.hpp"

const int PARAMS_NR = 4;


struct Ktick : Module {
	enum ParamId {
		ENUMS(DIV_PARAM, PARAMS_NR),
		PARAMS_LEN
	};
	enum InputId {
		INPUTS_LEN
	};
	enum OutputId {
		ENUMS(DIV_OUTPUT, PARAMS_NR),
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	KlokExpanderMessage leftMessages[2][1];

	Ktick() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		for (int i = 0; i < PARAMS_NR; i++) {
			configParam(DIV_PARAM + i, 1.f, 256.f, 1.f, "Division factor " + std::to_string(i));
			{
				auto* pq = getParamQuantity(DIV_PARAM + i);
				pq->snapEnabled = true;
			}
			configOutput(DIV_OUTPUT + i, "Division " + std::to_string(i));
		}

		leftExpander.producerMessage = leftMessages[0];
		leftExpander.consumerMessage = leftMessages[1];	
	}

	void process(const ProcessArgs& args) override {
		bool hasExpander = (leftExpander.module && leftExpander.module->model == modelKlok);
		if (hasExpander) {

			// Receive clock directly from Klok
			KlokExpanderMessage *messagesFromModule = (KlokExpanderMessage *)(leftExpander.consumerMessage);
			if (messagesFromModule) {
				for (int i = 0; i < PARAMS_NR; i++) {
					outputs[DIV_OUTPUT + i].setVoltage(messagesFromModule->values[i]);
				}
			}
			
			// Send factors to Klok
			KlokExpanderMessage *messageToModule = (KlokExpanderMessage*)(leftExpander.producerMessage);
			if (messageToModule) {
				for (int i = 0; i < PARAMS_NR; i++) {
					messageToModule->factors[i] = int(params[DIV_PARAM + i].getValue());
				}
			}
		} else {
			for (int i = 0; i < PARAMS_NR; i++) {
				outputs[DIV_OUTPUT + i].setVoltage(0.f);
			}
		}
	}
};


struct KtickWidget : ModuleWidget {
	KtickWidget(Ktick* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Ktick.svg")));

		float hp = 5.08f;
		float u = 133.35f;
		float yDiv = u / 14.f;
		float kPosx = hp * 1.5;
		float oPosx = hp * 2.5;

		addChild(new TextDisplayWidget("Ktick", Vec(hp/2, hp*1.5), 14, -1));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addChild(new TextDisplayWidget("Div", Vec(hp*2, (yDiv*3) - 10), 14));
		for (int i = 0; i < PARAMS_NR; i++) {
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(kPosx, (yDiv * 3) + (yDiv * i * 2.7))), module, Ktick::DIV_PARAM + i));
			addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(oPosx, (yDiv * 4) + (yDiv * i * 2.7) )), module, Ktick::DIV_OUTPUT + i));
		}
	}
};


Model* modelKtick = createModel<Ktick, KtickWidget>("Ktick");