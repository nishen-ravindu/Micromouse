#include "../mouse-firmware/floodfill.h"

int main()
{
    if (!floodfill_begin())
    {
        return 1;
    }

    while (!floodfill_finished())
    {
        if (!floodfill_step())
        {
            break;
        }
    }

    return 0;
}
