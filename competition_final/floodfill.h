#ifndef FLOODFILL_H
#define FLOODFILL_H

// Initialise all maze memory and the search state machine.
bool floodfill_begin();

// Executes one sense/plan/move cycle.
// Returns false when all planned runs are complete or on a fatal error.
bool floodfill_step();

bool floodfill_finished();

// ---- Race build: one run at a time, learned maze kept in flash ----
// A run is start -> goal -> back to start (SEARCH explores, FAST uses only
// known walls), then floodfill_step() returns false. The maze is kept.
void floodfill_startRun(bool fast);
bool floodfill_runComplete();          // the last run ended back at the start
bool floodfill_knownPathToGoal();      // FAST possible: a path of known-open walls

// The learned maze as bytes (walls, wall evidence, visited cells), for flash.
int  floodfill_mapBytes();
void floodfill_exportMap(unsigned char *buf);
bool floodfill_importMap(const unsigned char *buf, int len);  // false: other maze size / goal
void floodfill_clearMap();

#endif
