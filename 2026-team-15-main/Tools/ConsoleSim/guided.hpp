/* ======================================================================================

This code is written by...

Name:           Lee Mingxuan
Course:         RTIS
Digipen ID:     mingxuan.lee

Guided mode for the console demo: YOU type every amount, in real time.
    - The shift clock and every customer's patience run on the real clock while you type.
    - A wrong pour is wasted and you must redo the step.
    - After each customer, a low shelf asks you to type "Refill <ingredient>".

====================================================================================== */

#pragma once

#include "Simulation/simulation.hpp"

// Runs one whole shift. Returns when the shift is over (time up, typed "quit", or input closed).
// The Simulation must already be Init()-ed, with a time limit set on its scenario.
void RunGuided(core::Simulation& sim);
