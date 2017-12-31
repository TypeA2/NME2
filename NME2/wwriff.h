#pragma once

#include "defs.h"
#include "bitmanip.h"

// Creates an ogg
errno_t create_ogg(membuf* data, FILE* out);