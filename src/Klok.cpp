#include "plugin.hpp"

#include <iostream>
#include <sstream>
using namespace std;

const int MOD_OUTPUTS = 8;

std::string Convert (float number){
	std::ostringstream buff;
	buff<<number;
	return buff.str();
}

struct Klok : Module {
	enum ParamId {
		RUN_PARAM,
		TEMPO_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		INPUTS_LEN
	};
	enum OutputId {
		RESET_OUTPUT,
		ENUMS(MOD_OUTPUT, MOD_OUTPUTS),
		OUTPUTS_LEN
	};
	enum LightId {
		BLINK_LIGHT,
		LIGHTS_LEN
	};

	dsp::PulseGenerator pgen;
	dsp::PulseGenerator preset;

	float counter, period;
	float TRIG_TIME = 1e-3f;
	int steps = 0;
	bool reset = true;
	int output_offset = MOD_OUTPUT;

	Klok() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(RUN_PARAM, 0.f, 1.f, 0.f, "Run clock");
		configParam(TEMPO_PARAM, 30.0, 360.0, 120.0, "Set tempo", "BPM");
		configOutput(RESET_OUTPUT, "Reset");
		for (int i = 0; i < MOD_OUTPUTS; i++) {
			configOutput(MOD_OUTPUT + i, "Modulo " + std::to_string(i));
		}
	}

	void process(const ProcessArgs& args) override {
		float BPM = params[TEMPO_PARAM].getValue();
		float running = params[RUN_PARAM].getValue();

		if (running) {
			
			// RESET
			if (reset) {
				// Send reset pulse
				preset.trigger(TRIG_TIME);
				reset = false;
			}
			float resetout = preset.process(args.sampleTime);
			outputs[RESET_OUTPUT].setVoltage(10.f * resetout);

			// CLOCK PULSE
			period = 60.f * args.sampleRate/(BPM * 2); // Samples that need to pass before a new trigger, get octave notes

			if (counter > period) {
				pgen.trigger(TRIG_TIME);
				counter -= period; // Compensate for small errors
				steps++;
				steps %= MOD_OUTPUTS;
			}
			counter++;

			float out = pgen.process(args.sampleTime); // Gets the state of the trigger
			for (int i = output_offset; i < OUTPUTS_LEN; i++) {
				// Loop through all modulo outputs
				if (steps % i - output_offset + 1 == 0 && outputs[i].isConnected()) {
					outputs[i].setVoltage(10.f * out); // Set the value using the general pulse generator
				}
			}
			lights[BLINK_LIGHT].setSmoothBrightness(out, 5e-6f); // Set light to value between 0 and 1, second argument sets vinishing time

		} else {
			counter = period = steps = 0;
			reset = true;
		}
	}
};


struct KlokWidget : ModuleWidget {
	KlokWidget(Klok* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Klok.svg")));

		float hp = 5.08f;

		addChild(new TextDisplayWidget("Klok", Vec(hp/2, hp*1.5), 14, -1));
		addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(hp/2 + 5.75f, hp*1.5)), module, Klok::BLINK_LIGHT));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		float runY = 22.f;

		addChild(new TextDisplayWidget("Run", Vec(hp, runY - 6.f), 10));
		addParam(createParamCentered<CKSS>(mm2px(Vec(hp, runY)), module, Klok::RUN_PARAM));
		addChild(new TextDisplayWidget("Rst", Vec(hp * 2.7f, runY - 6.f), 10));
		addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(hp * 2.7f, runY)), module, Klok::RESET_OUTPUT));

		float tempoY = 38.f;

		addChild(new TextDisplayWidget("Tempo", Vec(hp*2, tempoY - 7.f), 10));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(hp*2, tempoY)), module, Klok::TEMPO_PARAM));

		float minX = hp*2.6f;
		float outY = 56.f;
		float divY = 9.f;

		addChild(new TextDisplayWidget("%", Vec(hp*2, outY - 7.f), 16));

		for (int i = 0; i < MOD_OUTPUTS; i++) {
			addChild(new TextDisplayWidget(Convert(i), Vec(hp, outY + (divY * i)), 16));
			addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(minX, outY + (divY * i))), module, Klok::MOD_OUTPUT + i));
		}
	}
};


Model* modelKlok = createModel<Klok, KlokWidget>("Klok");