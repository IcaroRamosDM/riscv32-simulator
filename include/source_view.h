#pragma once
#include "debug_info.h"

void source_view_location(const DebugInfo *info, uint32_t address, bool show_text);
void source_view_files(const DebugInfo *info);
void source_view_lines(const DebugInfo *info, uint32_t address, unsigned center, size_t file);
void source_view_symbols(const DebugInfo *info, const char *name);
