#ifndef API_H
#define API_H

// This header deliberately mirrors the mackorone/mms-c API so that the same
// floodfill.cpp can be compiled for the simulator or for the real ESP32 mouse.
#ifdef __cplusplus
extern "C" {
#endif

int API_mazeWidth();
int API_mazeHeight();

int API_wallFront();
int API_wallRight();
int API_wallLeft();

int API_moveForward();
void API_turnRight();
void API_turnLeft();

void API_setWall(int x, int y, char direction);
void API_clearWall(int x, int y, char direction);

void API_setColor(int x, int y, char color);
void API_clearColor(int x, int y);
void API_clearAllColor();

void API_setText(int x, int y, char *text);
void API_clearText(int x, int y);
void API_clearAllText();

int API_wasReset();
void API_ackReset();

#ifdef ARDUINO
// Hardware-only fault latch. floodfill.cpp uses this only when ARDUINO is defined.
int API_motionFault();
void API_clearMotionFault();

// Hardware-only goal: the 2x2 goal's cell nearest the start (x = sideways into
// the maze, y = forward). The simulator build keeps the maze centre.
int API_goalX();
int API_goalY();

// Hardware-only fast runs: drive `cells` cells without stopping in between.
// API_fastContinuous() is the tuner switch that allows it (0 = cell by cell).
int API_moveForwardCells(int cells);
int API_fastContinuous();
#endif

#ifdef __cplusplus
}
#endif

#endif
