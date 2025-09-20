#include <obs-module.h>
#include <util/threading.h>
#include "plugin-macros.generated.h"
#include "ptz-backend.hpp"

ptz_backend::ptz_backend()
{
	ref = 1;
}

ptz_backend::~ptz_backend() {}

void ptz_backend::release()
{
	if (os_atomic_dec_long(&ref) == 0)
		delete this;
}
