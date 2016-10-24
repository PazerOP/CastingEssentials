#include "Modules.h"

static ModuleManager s_ModuleManager;
ModuleManager& Modules() { return s_ModuleManager; }