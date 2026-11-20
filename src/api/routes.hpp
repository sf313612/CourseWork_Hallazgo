#pragma once

#include "crow.h"
#include "api_state.hpp"

void registerRoutes(crow::SimpleApp& app, ApiState& state);