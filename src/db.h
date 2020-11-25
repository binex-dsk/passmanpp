#ifndef DB_H
#define DB_H
#include <experimental/filesystem>
#include <sodium.h>

#include "sql.h"

uint32_t randomChar();
bool exists(std::string cmd);

std::string genPass(int length, bool capitals, bool numbers, bool symbols);
#endif
