#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
	pluginInstance = p;

	p->addModel(modelKlok);
	p->addModel(modelSecu);
	p->addModel(modelBaBum);
	p->addModel(modelScener);
	p->addModel(modelDistroi);
}
