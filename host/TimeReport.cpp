#include <windows.h>
#include "common/debug.h"

unsigned int TimeReport::getTime() {
	return GetTickCount();
}
