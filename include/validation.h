#pragma once

#include <vector>

#include "types.h"

void check_nonzero_id(const char* field_name, Id id);
void check_unique_ids(const char* field_name, const std::vector<Id>& ids);
void check_discount(double discount);
