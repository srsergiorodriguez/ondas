#pragma once
#include <rack.hpp>

#include <string>
#include <iostream>
#include <sstream>

using namespace rack;

// Declare the Plugin, defined in plugin.cpp
extern Plugin* pluginInstance;

// Declare each Model, defined in each module source file
extern Model* modelKlok;
extern Model* modelSecu;
extern Model* modelBaBum;
extern Model* modelScener;
extern Model* modelDistroi;
extern Model* modelLogistic;
extern Model* modelBap;
extern Model* modelBittorio;
extern Model* modelKtick;
extern Model* modelHiller;
extern Model* modelScenerProMax;
extern Model* modelNorbert;

inline std::string Convert(float number){
	std::ostringstream buff;
	buff<<number;
	return buff.str();
}

struct BabumExpanderMessage {  
  float kickLength = 0.f;
	float snareLength = 0.f;
	float hihatLength = 0.f;
	float fxLength = 0.f;

	float kickDistortion = 0.f;
	float snareDistortion = 0.f;
	float fxDistortion = 0.f;
};

struct KlokExpanderMessage {  
  float values[4] = {0.f, 0.f, 0.f, 0.f};
	int factors[4] = {1, 1, 1, 1};
};

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
	int fontSize = 10;
	int align = 0;
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

		nvgFontSize(args.vg, fontSize);  // Font size
		if (customFont) {
			nvgFontFaceId(args.vg, customFont->handle);
		} else {
			nvgFontFaceId(args.vg, APP->window->uiFont->handle); // Fallback to default font
		}
		
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

struct LogisticMapWidget : TransparentWidget {
  Vec origin;
  Vec size;
	int nPoints = 0;      // how many Vec’s in the array
	Vec* points = nullptr;      // pointer to first element
	const float* rSumPtr = nullptr;

	LogisticMapWidget(const Vec& origin, const Vec& size, const float* rSumPtr, Vec* points, int nPoints) {
    this->origin = origin;
    this->size = size;
		this->nPoints = nPoints;
		this->points  = points;
		this->rSumPtr = rSumPtr;
    box.pos = origin;
    box.size = size;
  }

  void drawLayer(const DrawArgs& args, int layer) override {	
		if (!points || nPoints <= 0 || !rSumPtr)
			return;

    NVGcontext* vg = args.vg;

    float w  = size.x;
    float h  = size.y;

		nvgBeginPath(vg);
		nvgRect(vg, 0, 0, w, h);
		nvgFillColor(vg, nvgRGBA(0xe6, 0xe6, 0xe6, 0xe6)); // Bg color
    nvgFill(vg);

		float rNorm = clamp(( *rSumPtr - 1.1f ) / (3.9f - 1.1f), 0.f, 1.f);
		float yPos  = (1.0f - rNorm) * h;
		nvgBeginPath(vg);
		nvgMoveTo(vg, 0, yPos);
    nvgLineTo(vg, w, yPos);
    nvgStrokeWidth(vg, 1.5f);
    nvgStrokeColor(vg, nvgRGBA(0xe6, 0x3c, 0x4b, 0xff));
    nvgStroke(vg);

		nvgBeginPath(vg);
		for (int i = 0; i < nPoints; i++) {
      Vec pt = points[i];
      float nx = pt.x;
      float ny = pt.y;
      float x  = nx * w;
			float y = (1 - ny) * h;
      nvgCircle(vg, x, y, 0.7f);
    }

    nvgFillColor(vg, nvgRGBA(0x0, 0x0, 0x0, 0xff));
    nvgFill(vg);
  }
};

struct CellularAutomataWidget : TransparentWidget {
  Vec origin;
  Vec size;
	int n = 16;
	int epochs = 16;
	int* sectorPtr = nullptr;
	int* stepPtr = nullptr;
	uint16_t* history = nullptr;

	CellularAutomataWidget(const Vec& origin, const Vec& size, int n, int epochs, uint16_t* history, int* sectorPtr, int* stepPtr) {
    this->origin = origin;
    this->size = size;
		this->n = n;
		this->epochs = epochs;
		this->history = history;
		this->sectorPtr = sectorPtr;
		this->stepPtr = stepPtr;
    box.pos = origin;
    box.size = size;
  }

  void drawLayer(const DrawArgs& args, int layer) override {	
		if (!history || epochs <= 0 || !n)
			return;

    NVGcontext* vg = args.vg;

    float w  = size.x;
    float h  = size.y;

		nvgBeginPath(vg);
		nvgRect(vg, 0, 0, w, h + 10);
		nvgFillColor(vg, nvgRGBA(0xe6, 0xe6, 0xe6, 0xff)); // Bg color
    nvgFill(vg);

		float divX = w / float(n);
		float divY = h / float(epochs);

		for (int y = 0; y < epochs; y++) {
			uint16_t epochBits = history[y];

			for (int x = 0; x < n; x++) {
				float nx = x * divX;
				float ny = y * divY;

				bool cellIsOn = (epochBits >> x) & 0x1;

				nvgBeginPath(vg);
				nvgRect(vg, nx, ny, ceil(divX), ceil(divY));

				int step = *stepPtr;

				if (step == x) {
					if (cellIsOn) {
						nvgFillColor(vg, nvgRGBA(0xe6, 0x3c, 0x4b, 0xff)); // Red
						nvgFill(vg);
					} else {
						nvgFillColor(vg, nvgRGBA(0x00, 0x94, 0xd2, 0xff)); // Blue
						nvgFill(vg);
					}
				} else {
					if (cellIsOn) {
						nvgFillColor(vg, nvgRGBA(0x00, 0x00, 0x00, 0xff)); // Black
						nvgFill(vg);
					} else {
						nvgFillColor(vg, nvgRGBA(0x00, 0x00, 0x00, 0x00)); // Transparent
						nvgFill(vg);
					}
				}
			}
    }

		int sector = *sectorPtr;

		nvgBeginPath(vg);
		nvgRect(vg, 0, sector * (divY * 4), w, divY * 4);
		nvgStrokeColor(vg, nvgRGBA(0xe6, 0x3c, 0x4b, 0xff)); // Red
    nvgStroke(vg);
  }
};