#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
	pluginInstance = p;

	p->addModel(modelKlok);
	p->addModel(modelSecu);
	p->addModel(modelBaBum);
	p->addModel(modelScener);
	p->addModel(modelDistroi);
	p->addModel(modelLogistic);
	p->addModel(modelBap);
	p->addModel(modelBittorio);
	p->addModel(modelKtick);
	p->addModel(modelHiller);
	p->addModel(modelScenerProMax);
	p->addModel(modelNorbert);
}
