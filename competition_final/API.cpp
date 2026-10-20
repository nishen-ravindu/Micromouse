#include "API.h"
#include "motors.h"
#include "sensors.h"

static bool motionFault = false;

int API_mazeWidth()
{
    return tb_mazeSize();
}

int API_mazeHeight()
{
    return tb_mazeSize();
}

int API_wallFront()
{
    return sensors_wallFront() ? 1 : 0;
}

int API_wallRight()
{
    return sensors_wallRight() ? 1 : 0;
}

int API_wallLeft()
{
    return sensors_wallLeft() ? 1 : 0;
}

int API_moveForward()
{
    bool ok = motors_moveOneCell();
    if (!ok) motionFault = true;
    return ok ? 1 : 0;
}

void API_turnRight()
{
    if (!motors_turnRight90()) motionFault = true;
}

void API_turnLeft()
{
    if (!motors_turnLeft90()) motionFault = true;
}

// The simulator uses these for visualisation. They intentionally do nothing on
// the physical mouse, while keeping floodfill.cpp source-compatible with MMS.
void API_setWall(int x, int y, char direction)
{
    (void)x;
    (void)y;
    (void)direction;
}

void API_clearWall(int x, int y, char direction)
{
    (void)x;
    (void)y;
    (void)direction;
}

void API_setColor(int x, int y, char color)
{
    (void)x;
    (void)y;
    (void)color;
}

void API_clearColor(int x, int y)
{
    (void)x;
    (void)y;
}

void API_clearAllColor()
{
}

void API_setText(int x, int y, char *text)
{
    (void)x;
    (void)y;
    (void)text;
}

void API_clearText(int x, int y)
{
    (void)x;
    (void)y;
}

void API_clearAllText()
{
}

int API_wasReset()
{
    // A real robot is not reset by MMS, so this always returns false.
    return 0;
}

void API_ackReset()
{
}

#ifdef ARDUINO
int API_motionFault()
{
    return motionFault ? 1 : 0;
}

void API_clearMotionFault()
{
    motionFault = false;
}

int API_goalX()
{
    return tb_goalX();
}

int API_goalY()
{
    return tb_goalY();
}

int API_moveForwardCells(int cells)
{
    bool ok = motors_moveCells(cells);
    if (!ok) motionFault = true;
    return ok ? 1 : 0;
}

int API_fastContinuous()
{
    return tb_fastContinuous();
}
#endif
