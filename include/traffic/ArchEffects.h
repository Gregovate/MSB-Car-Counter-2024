#pragma once
#include "RuntimeState.h"

// Service interface; definitions are compiled once in src/services.
void playPattern();

// Runs before detection so the detector always has final control of its outputs.
void serviceIdleArchPattern();

void stopIdleArchPattern();
