#ifndef FLOODFILL_H
#define FLOODFILL_H

// Initialise all maze memory and the search state machine.
bool floodfill_begin();

// Executes one sense/plan/move cycle.
// Returns false when all planned runs are complete or on a fatal error.
bool floodfill_step();

bool floodfill_finished();

#endif
