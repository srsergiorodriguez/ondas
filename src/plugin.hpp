#pragma once
#include <rack.hpp>
#include <string>

using namespace rack;

// Declare the Plugin, defined in plugin.cpp
extern Plugin* pluginInstance;

// Declare each Model, defined in each module source file
extern Model* modelKlok;
extern Model* modelSecu;
extern Model* modelBaBum;
extern Model* modelScener;
extern Model* modelDistroi;

struct StateButton : SVGSwitch {
	StateButton() {
		momentary = false;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/GUI/StateButton_0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/GUI/StateButton_1.svg")));
	}
};

struct RoundSmallBlackSnapKnob : RoundSmallBlackKnob {
	RoundSmallBlackSnapKnob() {
		snap = true;
	}
};

struct TextDisplayWidget : TransparentWidget {
	int fontSize;
	int align;
	std::string displayText;
	std::shared_ptr<Font> customFont;
	
	TextDisplayWidget(const std::string& text, Vec pos, int fs, int a = 0) {
		// Set the box size of the widget
		displayText = text;
		fontSize = fs;
		box.size = Vec(0, 0);
		box.pos = mm2px(pos);
		align = a;
		customFont = APP->window->loadFont(asset::plugin(pluginInstance, "res/Fonts/OverpassMono.ttf"));
		if (!customFont)
			WARN("Could not load custom font.");
	}

	void draw(const DrawArgs& args) override {
		// Call the parent draw function
		TransparentWidget::draw(args);
		drawText(args);
	}

	void drawText(const DrawArgs& args) {
		const char* text = displayText.c_str();

		nvgFontSize(args.vg, fontSize);                  // Font size
		if (customFont) {
			nvgFontFaceId(args.vg, customFont->handle);
		} else {
			// Fallback to default font
			nvgFontFaceId(args.vg, APP->window->uiFont->handle);
		}
		// nvgFontFaceId(args.vg, APP->window->uiFont->handle);  // Use VCV Rack's default font
		// Set the text color
		NVGcolor textColor = nvgRGB(0, 0, 0);
		nvgFillColor(args.vg, textColor);

		float bounds[4];
		nvgTextBounds(args.vg, 0, 0, text, nullptr, bounds);
		float textWidth = bounds[2] - bounds[0];
		float textHeight = bounds[3] - bounds[1];
		box.size = Vec(textWidth + 4, textHeight + 4); // Add some padding

		// Draw the text at a specific position
		int alignment;
		if (align == 1) {
			alignment = NVG_ALIGN_RIGHT;
		} else if (align == -1) {
			alignment = NVG_ALIGN_LEFT;
		} else {
			alignment = NVG_ALIGN_CENTER;
		}
		// NVG_ALIGN_CENTER = 1
		nvgTextAlign(args.vg, alignment | NVG_ALIGN_MIDDLE);
		nvgText(args.vg, 0, 0, text, nullptr);
	}
};