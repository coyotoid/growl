#ifndef PARSER_H
#define PARSER_H

#include "common.h"
#include "vendor/mpc.h"

V parser_init(V);
V parser_deinit(V);

extern mpc_parser_t *Program;

#endif
