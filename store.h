#pragma once

#include "error.h"
#include "pp.h"

struct error store_open(const char *db_file);
struct error store();
struct error store_close();

struct error query_tu(char *path, int n);
struct error query_semantics();