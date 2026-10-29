#ifndef SERAPHIM_IPC_H
#define SERAPHIM_IPC_H

#include <string>

struct World;
struct Script;

std::string ipc_execute(World *w, Script *s, const std::string &line);

#endif
