#pragma once

#include <map>
#include <optional>

#include "types.h"

struct BatchForecast {
    TimePoint start;
    TimePoint end;
    TimePoint release;
    BatchStatus status;
    bool lower_bound;
};

struct MachineForecast {
    TimePoint available_at;
    bool reserved = false;
    bool awaiting_confirmation = false;
};

struct OrderForecast {
    std::optional<TimePoint> ready_at;
    bool lower_bound = false;
    bool overdue = false;
    bool at_risk = false;
};

struct ShopForecast {
    TimePoint now;
    std::map<Id, BatchForecast> batches;
    std::map<Id, MachineForecast> machines;
    std::map<Id, OrderForecast> orders;
};
