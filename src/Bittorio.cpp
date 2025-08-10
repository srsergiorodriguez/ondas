#include "plugin.hpp"

const int N_EPOCHS = 16;
const int N = 16;
const int MAX_RULE = 255;
const int MAX_SEED = 65535;
const int DEFAULT_RULE = 30;

const int GATE_OUTS = 4;

struct Bittorio : Module {
	enum ParamId {
		RULE_PARAM,
		RANDOM_RULE_PARAM,
		SEED_PARAM,
		RANDOM_SEED_PARAM,
		SECTOR_PARAM,
		RESET_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		NEXT_STEP_INPUT,
		RULE_INPUT,
		RANDOM_RULE_INPUT,
		SEED_INPUT,
		RANDOM_SEED_INPUT,
		SECTOR_INPUT,
		RESET_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		ENUMS(GATE_OUTPUT, GATE_OUTS),
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	dsp::SchmittTrigger edgeDetectorTrigger;
	dsp::SchmittTrigger edgeDetectorStep;
	dsp::SchmittTrigger edgeDetectorReset;

	dsp::PulseGenerator pgenGateOut[GATE_OUTS];

	uint8_t rule = 30;
	uint16_t history[N_EPOCHS];
	uint16_t centerSeed = 1 << (N/2);
	uint16_t seed = centerSeed;
	int sector = 0;

	int step = 0;

	int cooldownSeed = 0;
	int cooldownRule = 0;
	int cooldownTime = 1;

	void setSeed() {
		for (int i = 0; i < N_EPOCHS; i++) {
			history[i] = 0;
		}
		history[0] = seed;
	}

	void setHistory() {
		uint16_t epoch = history[0];
		for (int y = 1; y < N_EPOCHS; y++) {
			uint16_t nextEpoch = 0;
			for (int x = 0; x < N; x++) {
				int left  = (epoch >> ((x + 15) & 15)) & 1;
				int self  = (epoch >> x) & 1;
				int right = (epoch >> ((x + 1) & 15)) & 1;
				int idx = (left << 2) | (self << 1) | right;
				if ((rule >> idx) & 1)
        	nextEpoch |= (1 << x);
			}
			history[y] = nextEpoch;
			epoch = nextEpoch;
		}
	}

	Bittorio() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		configParam(RULE_PARAM, 0, MAX_RULE, DEFAULT_RULE, "Rule select");
		{
			auto* pq = getParamQuantity(RULE_PARAM);
			pq->snapEnabled = true;
		}

		configButton(RANDOM_RULE_PARAM, "Random rule");
		configParam(SEED_PARAM, 0, MAX_SEED, centerSeed, "Seed select");
		{
			auto* pq = getParamQuantity(SEED_PARAM);
			pq->snapEnabled = true;
		}
		configButton(RANDOM_SEED_PARAM, "Random seed");
		configParam(SECTOR_PARAM, 0, 3, 0, "Window select");
		{
			auto* pq = getParamQuantity(SECTOR_PARAM);
			pq->snapEnabled = true;
		}

		configInput(RULE_INPUT, "Rule select");
		configInput(RANDOM_RULE_INPUT, "Random rule");
		configInput(SEED_INPUT, "Seed select");
		configInput(RANDOM_SEED_INPUT, "Random seed");
		configInput(SECTOR_INPUT, "Window select");

		configInput(NEXT_STEP_INPUT, "Next step");
		configButton(RESET_PARAM, "Reset step");
		configInput(RESET_INPUT, "Reset step");

		for (int i = 0; i < GATE_OUTS; i++) {
			configOutput(GATE_OUTPUT + i, "Row" + std::to_string(i));
		}

		setSeed();
		setHistory();
	}

	void process(const ProcessArgs& args) override {
		bool update = false;

		// Step
		if (edgeDetectorStep.process(inputs[NEXT_STEP_INPUT].getVoltage())) {
			step++;
			step = step >= N ? 0 : step;
		}

		if (params[RESET_PARAM].getValue() || edgeDetectorReset.process(inputs[RESET_INPUT].getVoltage())) {
			step = 0;
		}

		// Sector
		sector = inputs[SECTOR_INPUT].isConnected() 
						? floor(clamp(inputs[SECTOR_INPUT].getVoltage(), 0.f, 3.f))
						: params[SECTOR_PARAM].getValue();

		// Rule

		// Random value
		if (!params[RANDOM_RULE_PARAM].getValue() && !inputs[RANDOM_RULE_INPUT].getVoltage()) {
			cooldownRule = 0;
		}

		if (cooldownRule == 0 && (params[RANDOM_RULE_PARAM].getValue() || edgeDetectorTrigger.process(inputs[RANDOM_RULE_INPUT].getVoltage()))) {
			cooldownRule = cooldownTime * args.sampleRate;
			uint32_t r = random::u32();
			uint8_t newRule = (uint8_t)(r & 0xFFu);
			rule = newRule;
			update = true;
			params[RULE_PARAM].setValue(float(newRule));
		}

		// Set value

		else if (inputs[RULE_INPUT].isConnected()) {
			float v = clamp(inputs[RULE_INPUT].getVoltage(), 0.0f, 5.0f);
			float scaled = v / 5.0f * float(MAX_RULE);
			uint8_t newRule = (uint8_t)(round(scaled));
			if (newRule != rule) {
				rule = newRule;
				update = true;
			}
		}
		
		else if (params[RULE_PARAM].getValue() != rule ) {
			rule = params[RULE_PARAM].getValue();
			update = true;
		}

		// Seed

		// Random value
		if (!params[RANDOM_SEED_PARAM].getValue() && !inputs[RANDOM_SEED_INPUT].getVoltage()) {
			cooldownSeed = 0;
		}

		if (cooldownSeed == 0 && (params[RANDOM_SEED_PARAM].getValue() || edgeDetectorTrigger.process(inputs[RANDOM_SEED_INPUT].getVoltage()))) {
			cooldownSeed = cooldownTime * args.sampleRate;
			uint32_t r = random::u32();
			uint16_t newSeed = (uint16_t)(r & 0xFFFFu);
			seed = newSeed;
			update = true;
			params[SEED_PARAM].setValue(float(newSeed));
		}

		// Set value

		else if (inputs[SEED_INPUT].isConnected()) {
			float v = clamp(inputs[SEED_INPUT].getVoltage(), 0.0f, 5.0f);
			float scaled = v / 5.0f * float(MAX_SEED);
			uint16_t newSeed = (uint16_t)(round(scaled));
			if (newSeed != seed) {
				seed = newSeed;
				update = true;
			}
		}

		else if (params[SEED_PARAM].getValue() != seed ) {
			seed = params[SEED_PARAM].getValue();
			update = true;
		}

		if (update) {
			setSeed();
			setHistory();
		}

		for (int i = 0; i < GATE_OUTS; i++) {
			// outputs[GATE_OUTPUT + i].setVoltage(10.f);
			// outputs[GATE_OUTPUT + i].setVoltage(step);
			uint16_t epochBits = history[sector + i];
			bool cellIsOn = (epochBits >> step) & 0x1;
			float v = cellIsOn ? 5.f : 0.f;
			outputs[GATE_OUTPUT + i].setVoltage(v);
		}

		// Cooldowns

		cooldownRule--;
		cooldownRule = cooldownRule <= 0 ? 0 : cooldownRule;

		cooldownSeed--;
		cooldownSeed = cooldownSeed <= 0 ? 0 : cooldownSeed;
	}
};


struct BittorioWidget : ModuleWidget {
	BittorioWidget(Bittorio* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Bittorio.svg")));

		float hp = 5.08f;
		float u = 133.35f;
		float yDiv = u / 14.f;
		float tOffset = 7.f;

		addChild(new TextDisplayWidget("Bittorio", Vec(hp/2, hp*1.5), 14, -1));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		if (module) {
			Vec plotPos = mm2px(Vec(hp*0.5, yDiv*1.5));
			Vec plotSize = mm2px(Vec(floor(hp*8), floor(hp*8)));
			addChild(new CellularAutomataWidget(plotPos, plotSize, N, N_EPOCHS, module->history, &module->sector, &module->step));
		}

		float rulePosX = hp * 2.5;
		float seedPosX = hp * 6.5;
		float centerPosX = hp * 4.5;

		addChild(new TextDisplayWidget("Rule", Vec(rulePosX, (yDiv * 7) - tOffset), 10, 0));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(rulePosX - hp, yDiv * 7)), module, Bittorio::RULE_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(rulePosX + hp, yDiv * 7)), module, Bittorio::RULE_INPUT));

		addChild(new TextDisplayWidget("Seed", Vec(seedPosX, (yDiv * 7) - tOffset), 10, 0));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(seedPosX - hp, yDiv * 7)), module, Bittorio::SEED_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(seedPosX + hp, yDiv * 7)), module, Bittorio::SEED_INPUT));

		addChild(new TextDisplayWidget("Rnd", Vec(centerPosX, (yDiv * 8.5) - tOffset), 10, 0));

		addParam(createParamCentered<VCVButton>(mm2px(Vec(rulePosX - hp, yDiv * 8.5)), module, Bittorio::RANDOM_RULE_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(rulePosX + hp, yDiv * 8.5)), module, Bittorio::RANDOM_RULE_INPUT));

		addParam(createParamCentered<VCVButton>(mm2px(Vec(seedPosX - hp, yDiv * 8.5)), module, Bittorio::RANDOM_SEED_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(seedPosX + hp, yDiv * 8.5)), module, Bittorio::RANDOM_SEED_INPUT));

		addChild(new TextDisplayWidget("Window", Vec(centerPosX, (yDiv * 10) - tOffset), 10, 0));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(centerPosX - hp, yDiv * 10)), module, Bittorio::SECTOR_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(centerPosX + hp, yDiv * 10)), module, Bittorio::SECTOR_INPUT));

		addChild(new TextDisplayWidget("Stp", Vec(rulePosX - hp, (yDiv * 11)), 10, 1));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(rulePosX, yDiv * 11)), module, Bittorio::NEXT_STEP_INPUT));

		addChild(new TextDisplayWidget("Rst", Vec(seedPosX - (1.7 * hp), (yDiv * 11)), 10, 1));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(seedPosX - hp, yDiv * 11)), module, Bittorio::RESET_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(seedPosX + hp, yDiv * 11)), module, Bittorio::RESET_INPUT));

		float divx = (8 * hp) / GATE_OUTS;
		for (int i = 0; i < GATE_OUTS; i++) {
			addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec((hp*1.5) + (divx * i), yDiv*12.2)), module, Bittorio::GATE_OUTPUT + i));
		}
	}
};


Model* modelBittorio = createModel<Bittorio, BittorioWidget>("Bittorio");