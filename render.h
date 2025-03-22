#pragma once

#include "error.h"

#include <stdio.h>

struct error render_init();
struct error render_halt();

struct error render(FILE *fp);