#pragma once
#include "RuntimeState.h"

// Service interface; definitions are compiled once in src/services.
void countTheCar();
void detectCar();

void publishPassageDebugState();
void resetPassageDebugConnection();
bool handlePassageDebugCommand(const char* topic,const char* message);
