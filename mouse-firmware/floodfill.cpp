#include "floodfill.h"
#include "API.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ARDUINO
#include <Arduino.h>
#endif

#define MAX_SIZE 16

#define UNKNOWN 0
#define OPEN    1
#define WALL    2

#define INF 1000000

// Reserve competition attempts for fast runs.
#define MAX_SEARCH_RUNS 3
#define MAX_FAST_RUNS   2

// Set to 1 in the simulator build if you want flood values drawn.
// Hardware API_setText() is a no-op, so this is harmless on the real mouse.
#define DRAW_DISTANCES 1

static int horizontal_walls[MAX_SIZE][MAX_SIZE + 1];
static int vertical_walls[MAX_SIZE + 1][MAX_SIZE];

// How sure the map is about each wall: every reading adds +1 (wall) or -1
// (open), kept within +/- WALL_EVIDENCE_MAX. The sign is the wall state; a tie
// keeps what the map had. So one reading that disagrees no longer flips a wall
// that was seen before. Without this, a wall that one sensor saw and another
// did not (side ToF "open", then front ToF "wall" after turning to it) flipped
// at every visit, and the mouse turned back and forth in the same cell.
#define WALL_EVIDENCE_MAX 3
static signed char horizontal_evidence[MAX_SIZE][MAX_SIZE + 1];
static signed char vertical_evidence[MAX_SIZE + 1][MAX_SIZE];

static int optimistic_distance[MAX_SIZE][MAX_SIZE];
static int confirmed_distance[MAX_SIZE][MAX_SIZE];
static int movement_distance[MAX_SIZE][MAX_SIZE];

static int visited[MAX_SIZE][MAX_SIZE];
static int visit_count[MAX_SIZE][MAX_SIZE];
static int fast_turn_cost[MAX_SIZE][MAX_SIZE][4];

static int width;
static int height;

enum Direction
{
    NORTH = 0,
    EAST = 1,
    SOUTH = 2,
    WEST = 3
};

enum Target
{
    TARGET_GOAL,
    TARGET_START
};

enum Mode
{
    SEARCH_TO_GOAL,
    SEARCH_TO_START,
    FAST_TO_GOAL,
    FAST_TO_START,
    FINISHED
};

static int mouse_x;
static int mouse_y;
static enum Direction mouse_heading;
static enum Mode mode;

static int completed_search_runs;
static int completed_fast_runs;
static bool fatal_error;

typedef struct
{
    int x;
    int y;
} Point;

typedef struct
{
    Point arr_queue[MAX_SIZE * MAX_SIZE];
    int queue_front;
    int queue_back;
} Queue;

static Queue q;

static void log_message(const char *text)
{
#ifdef ARDUINO
    Serial.println(text);
#else
    fprintf(stderr, "%s\n", text);
    fflush(stderr);
#endif
}

static void log_lengths(int optimistic, int confirmed)
{
#ifdef ARDUINO
    Serial.print("Optimistic=");
    if (optimistic >= INF) Serial.print("INF"); else Serial.print(optimistic);
    Serial.print(" Confirmed=");
    if (confirmed >= INF) Serial.println("INF"); else Serial.println(confirmed);
#else
    fprintf(stderr, "Optimistic=%d Confirmed=%d\n",
            optimistic >= INF ? -1 : optimistic,
            confirmed >= INF ? -1 : confirmed);
    fflush(stderr);
#endif
}

static int valid_cell(int x, int y)
{
    return x >= 0 && x < width && y >= 0 && y < height;
}

static char direction_char(enum Direction dir)
{
    switch (dir)
    {
        case NORTH: return 'n';
        case EAST:  return 'e';
        case SOUTH: return 's';
        case WEST:  return 'w';
    }

    return 'n';
}

static int get_neighbor(
    int x,
    int y,
    enum Direction dir,
    int *nx,
    int *ny)
{
    *nx = x;
    *ny = y;

    switch (dir)
    {
        case NORTH: (*ny)++; break;
        case SOUTH: (*ny)--; break;
        case EAST:  (*nx)++; break;
        case WEST:  (*nx)--; break;
    }

    return valid_cell(*nx, *ny);
}

static void init_maze()
{
    int x;
    int y;
    int d;

    memset(horizontal_evidence, 0, sizeof(horizontal_evidence));
    memset(vertical_evidence, 0, sizeof(vertical_evidence));

    for (x = 0; x < width; x++)
    {
        for (y = 0; y <= height; y++)
        {
            horizontal_walls[x][y] = UNKNOWN;
        }

        horizontal_walls[x][0] = WALL;
        horizontal_walls[x][height] = WALL;
    }

    for (y = 0; y < height; y++)
    {
        for (x = 0; x <= width; x++)
        {
            vertical_walls[x][y] = UNKNOWN;
        }

        vertical_walls[0][y] = WALL;
        vertical_walls[width][y] = WALL;
    }

    for (x = 0; x < width; x++)
    {
        for (y = 0; y < height; y++)
        {
            visited[x][y] = 0;
            visit_count[x][y] = 0;
            optimistic_distance[x][y] = INF;
            confirmed_distance[x][y] = INF;
            movement_distance[x][y] = INF;

            for (d = 0; d < 4; d++)
            {
                fast_turn_cost[x][y][d] = INF;
            }
        }
    }
}

static enum Direction get_absolute(int offset)
{
    return (enum Direction)((mouse_heading + offset + 4) % 4);
}

static int get_wall_state(int x, int y, enum Direction dir)
{
    switch (dir)
    {
        case NORTH: return horizontal_walls[x][y + 1];
        case SOUTH: return horizontal_walls[x][y];
        case EAST:  return vertical_walls[x + 1][y];
        case WEST:  return vertical_walls[x][y];
    }

    return WALL;
}

static void record_wall(
    int x,
    int y,
    enum Direction dir,
    int state)
{
    int old_state = get_wall_state(x, y, dir);

    switch (dir)
    {
        case NORTH: horizontal_walls[x][y + 1] = state; break;
        case SOUTH: horizontal_walls[x][y] = state; break;
        case EAST:  vertical_walls[x + 1][y] = state; break;
        case WEST:  vertical_walls[x][y] = state; break;
    }

    if (state == WALL && old_state != WALL)
    {
        API_setWall(x, y, direction_char(dir));
    }
}

static signed char *wall_evidence(int x, int y, enum Direction dir)
{
    switch (dir)
    {
        case NORTH: return &horizontal_evidence[x][y + 1];
        case SOUTH: return &horizontal_evidence[x][y];
        case EAST:  return &vertical_evidence[x + 1][y];
        case WEST:  return &vertical_evidence[x][y];
    }

    return &horizontal_evidence[x][y];
}

// One sensor reading of a wall of cell (x, y). The outer wall of the maze stays
// a wall whatever the sensors say.
static void sense_wall(
    int x,
    int y,
    enum Direction dir,
    int state)
{
    int nx;
    int ny;

    if (!get_neighbor(x, y, dir, &nx, &ny))
    {
        return;
    }

    signed char *evidence = wall_evidence(x, y, dir);
    int e = *evidence + (state == WALL ? 1 : -1);

    if (e > WALL_EVIDENCE_MAX) e = WALL_EVIDENCE_MAX;
    if (e < -WALL_EVIDENCE_MAX) e = -WALL_EVIDENCE_MAX;
    *evidence = (signed char)e;

    if (e > 0) record_wall(x, y, dir, WALL);
    else if (e < 0) record_wall(x, y, dir, OPEN);
}

// The mouse drove through this wall position: it is certainly open.
static void confirm_open(int x, int y, enum Direction dir)
{
    *wall_evidence(x, y, dir) = -WALL_EVIDENCE_MAX;
    record_wall(x, y, dir, OPEN);
}

static void sense_and_record()
{
    int state;

    visited[mouse_x][mouse_y] = 1;
    visit_count[mouse_x][mouse_y]++;
    API_setColor(mouse_x, mouse_y, 'G');

    state = API_wallFront() ? WALL : OPEN;
    sense_wall(mouse_x, mouse_y, get_absolute(0), state);

    state = API_wallRight() ? WALL : OPEN;
    sense_wall(mouse_x, mouse_y, get_absolute(1), state);

    state = API_wallLeft() ? WALL : OPEN;
    sense_wall(mouse_x, mouse_y, get_absolute(3), state);
}

static void enqueue(int x, int y)
{
    if (q.queue_back >= MAX_SIZE * MAX_SIZE)
    {
        fatal_error = true;
        return;
    }

    q.arr_queue[q.queue_back].x = x;
    q.arr_queue[q.queue_back].y = y;
    q.queue_back++;
}

static Point dequeue_point()
{
    Point p = q.arr_queue[q.queue_front];
    q.queue_front++;
    return p;
}

static int is_queue_empty()
{
    return q.queue_front == q.queue_back;
}

static int can_traverse(
    int x,
    int y,
    enum Direction dir,
    int allow_unknown)
{
    int nx;
    int ny;

    if (!get_neighbor(x, y, dir, &nx, &ny))
    {
        return 0;
    }

    int state = get_wall_state(x, y, dir);

    if (state == WALL) return 0;
    if (state == OPEN) return 1;

    return allow_unknown;
}

// Goal is a 2x2 block; this returns its cell nearest the start. On the mouse
// the tuner picks it (maze centre or manual); the simulator uses the centre.
static void goal_corner(int *goal_x1, int *goal_y1)
{
#ifdef ARDUINO
    *goal_x1 = API_goalX();
    *goal_y1 = API_goalY();
#else
    *goal_x1 = width / 2 - 1;
    *goal_y1 = height / 2 - 1;
#endif
}

static void add_goal_cells_to_queue(int distance[MAX_SIZE][MAX_SIZE])
{
    int goal_x1;
    int goal_y1;
    goal_corner(&goal_x1, &goal_y1);
    int goal_x2 = goal_x1 + 1;
    int goal_y2 = goal_y1 + 1;

    distance[goal_x1][goal_y1] = 0;
    enqueue(goal_x1, goal_y1);

    distance[goal_x2][goal_y1] = 0;
    enqueue(goal_x2, goal_y1);

    distance[goal_x1][goal_y2] = 0;
    enqueue(goal_x1, goal_y2);

    distance[goal_x2][goal_y2] = 0;
    enqueue(goal_x2, goal_y2);
}

static void flood_fill(
    enum Target target,
    int allow_unknown,
    int distance[MAX_SIZE][MAX_SIZE])
{
    int x;
    int y;
    int d;

    q.queue_front = 0;
    q.queue_back = 0;

    for (x = 0; x < width; x++)
    {
        for (y = 0; y < height; y++)
        {
            distance[x][y] = INF;
        }
    }

    if (target == TARGET_GOAL)
    {
        add_goal_cells_to_queue(distance);
    }
    else
    {
        distance[0][0] = 0;
        enqueue(0, 0);
    }

    while (!is_queue_empty() && !fatal_error)
    {
        Point current = dequeue_point();
        int cx = current.x;
        int cy = current.y;

        for (d = 0; d < 4; d++)
        {
            enum Direction dir = (enum Direction)d;
            int nx;
            int ny;

            if (!can_traverse(cx, cy, dir, allow_unknown))
            {
                continue;
            }

            if (!get_neighbor(cx, cy, dir, &nx, &ny))
            {
                continue;
            }

            if (distance[nx][ny] == INF)
            {
                distance[nx][ny] = distance[cx][cy] + 1;
                enqueue(nx, ny);
            }
        }
    }
}

static void calculate_goal_maps()
{
    flood_fill(TARGET_GOAL, 1, optimistic_distance);
    flood_fill(TARGET_GOAL, 0, confirmed_distance);
}

static int at_goal()
{
    int goal_x1;
    int goal_y1;
    goal_corner(&goal_x1, &goal_y1);
    int goal_x2 = goal_x1 + 1;
    int goal_y2 = goal_y1 + 1;

    return ((mouse_x == goal_x1 || mouse_x == goal_x2) &&
            (mouse_y == goal_y1 || mouse_y == goal_y2));
}

static int at_start()
{
    return mouse_x == 0 && mouse_y == 0;
}

static int unknown_edge_count(int x, int y)
{
    int count = 0;

    for (int d = 0; d < 4; d++)
    {
        int nx;
        int ny;
        enum Direction dir = (enum Direction)d;

        if (!get_neighbor(x, y, dir, &nx, &ny))
        {
            continue;
        }

        if (get_wall_state(x, y, dir) == UNKNOWN)
        {
            count++;
        }
    }

    return count;
}

static int turn_quarters(enum Direction from, enum Direction to)
{
    int difference = ((int)to - (int)from + 4) % 4;

    if (difference == 0) return 0;
    if (difference == 2) return 2;
    return 1;
}

static int choose_search_move(
    int distance[MAX_SIZE][MAX_SIZE],
    enum Direction *best_direction)
{
    int current_distance = distance[mouse_x][mouse_y];

    int found = 0;
    int best_visited_penalty = INF;
    int best_unknown_bonus = -1;
    int best_visits = INF;
    int best_turns = INF;

    if (current_distance >= INF || current_distance == 0)
    {
        return 0;
    }

    for (int d = 0; d < 4; d++)
    {
        enum Direction dir = (enum Direction)d;
        int nx;
        int ny;

        if (!can_traverse(mouse_x, mouse_y, dir, 1))
        {
            continue;
        }

        if (!get_neighbor(mouse_x, mouse_y, dir, &nx, &ny))
        {
            continue;
        }

        if (distance[nx][ny] != current_distance - 1)
        {
            continue;
        }

        int visited_penalty = visited[nx][ny] ? 1 : 0;
        int unknown_bonus = unknown_edge_count(nx, ny);
        int visits = visit_count[nx][ny];
        int turns = turn_quarters(mouse_heading, dir);

        if (!found ||
            visited_penalty < best_visited_penalty ||
            (visited_penalty == best_visited_penalty && unknown_bonus > best_unknown_bonus) ||
            (visited_penalty == best_visited_penalty && unknown_bonus == best_unknown_bonus && visits < best_visits) ||
            (visited_penalty == best_visited_penalty && unknown_bonus == best_unknown_bonus && visits == best_visits && turns < best_turns))
        {
            *best_direction = dir;
            best_visited_penalty = visited_penalty;
            best_unknown_bonus = unknown_bonus;
            best_visits = visits;
            best_turns = turns;
            found = 1;
        }
    }

    return found;
}

static void build_fast_turn_cost(int distance[MAX_SIZE][MAX_SIZE])
{
    int max_distance = 0;

    for (int x = 0; x < width; x++)
    {
        for (int y = 0; y < height; y++)
        {
            if (distance[x][y] < INF && distance[x][y] > max_distance)
            {
                max_distance = distance[x][y];
            }

            for (int h = 0; h < 4; h++)
            {
                fast_turn_cost[x][y][h] = INF;
            }
        }
    }

    for (int x = 0; x < width; x++)
    {
        for (int y = 0; y < height; y++)
        {
            if (distance[x][y] == 0)
            {
                for (int h = 0; h < 4; h++)
                {
                    fast_turn_cost[x][y][h] = 0;
                }
            }
        }
    }

    for (int distance_value = 1; distance_value <= max_distance; distance_value++)
    {
        for (int x = 0; x < width; x++)
        {
            for (int y = 0; y < height; y++)
            {
                if (distance[x][y] != distance_value)
                {
                    continue;
                }

                for (int h = 0; h < 4; h++)
                {
                    int best = INF;

                    for (int d = 0; d < 4; d++)
                    {
                        enum Direction dir = (enum Direction)d;
                        int nx;
                        int ny;

                        if (!can_traverse(x, y, dir, 0))
                        {
                            continue;
                        }

                        if (!get_neighbor(x, y, dir, &nx, &ny))
                        {
                            continue;
                        }

                        if (distance[nx][ny] != distance_value - 1)
                        {
                            continue;
                        }

                        if (fast_turn_cost[nx][ny][d] >= INF)
                        {
                            continue;
                        }

                        int candidate = turn_quarters((enum Direction)h, dir) +
                                        fast_turn_cost[nx][ny][d];

                        if (candidate < best)
                        {
                            best = candidate;
                        }
                    }

                    fast_turn_cost[x][y][h] = best;
                }
            }
        }
    }
}

// Fast-route choice from cell (x, y) with the given heading.
static int choose_fast_move_from(
    int x,
    int y,
    enum Direction heading,
    int distance[MAX_SIZE][MAX_SIZE],
    enum Direction *best_direction)
{
    int current_distance = distance[x][y];
    int best_cost = INF;
    int best_immediate_turns = INF;
    int found = 0;

    if (current_distance >= INF || current_distance == 0)
    {
        return 0;
    }

    for (int d = 0; d < 4; d++)
    {
        enum Direction dir = (enum Direction)d;
        int nx;
        int ny;

        if (!can_traverse(x, y, dir, 0))
        {
            continue;
        }

        if (!get_neighbor(x, y, dir, &nx, &ny))
        {
            continue;
        }

        if (distance[nx][ny] != current_distance - 1)
        {
            continue;
        }

        if (fast_turn_cost[nx][ny][d] >= INF)
        {
            continue;
        }

        int immediate_turns = turn_quarters(heading, dir);
        int total_cost = immediate_turns + fast_turn_cost[nx][ny][d];

        if (!found ||
            total_cost < best_cost ||
            (total_cost == best_cost && immediate_turns < best_immediate_turns))
        {
            *best_direction = dir;
            best_cost = total_cost;
            best_immediate_turns = immediate_turns;
            found = 1;
        }
    }

    return found;
}

static int choose_fast_move(
    int distance[MAX_SIZE][MAX_SIZE],
    enum Direction *best_direction)
{
    return choose_fast_move_from(mouse_x, mouse_y, mouse_heading, distance, best_direction);
}

#ifdef ARDUINO
// How many cells the fast route goes straight on in `dir` from the mouse's
// cell: stops where the route turns or at the target (goal or start).
static int fast_straight_cells(
    int distance[MAX_SIZE][MAX_SIZE],
    enum Direction dir)
{
    int cells = 1;
    int x;
    int y;
    enum Direction next;

    if (!get_neighbor(mouse_x, mouse_y, dir, &x, &y))
    {
        return 1;
    }

    while (cells < MAX_SIZE &&
           choose_fast_move_from(x, y, dir, distance, &next) &&
           next == dir)
    {
        if (!get_neighbor(x, y, dir, &x, &y))
        {
            break;
        }
        cells++;
    }

    return cells;
}
#endif

// Turns to target_direction, then drives `cells` cells forward (1 = one cell,
// then stop; more only in fast runs with API_fastContinuous()).
static int execute_move(enum Direction target_direction, int cells)
{
    int old_x = mouse_x;
    int old_y = mouse_y;

    int turns_needed = ((int)target_direction - (int)mouse_heading + 4) % 4;

#ifdef ARDUINO
    API_clearMotionFault();
#endif

    if (turns_needed == 1)
    {
        API_turnRight();
    }
    else if (turns_needed == 2)
    {
        API_turnRight();
#ifdef ARDUINO
        if (API_motionFault())
        {
            fatal_error = true;
            return 0;
        }
#endif
        API_turnRight();
    }
    else if (turns_needed == 3)
    {
        API_turnLeft();
    }

#ifdef ARDUINO
    if (API_motionFault())
    {
        // If a real turn fails, physical heading may no longer match the
        // logical maze heading. Stop rather than corrupting the map.
        fatal_error = true;
        return 0;
    }
#endif

    // Logical heading changes only after the physical turn has completed.
    mouse_heading = target_direction;

    // Re-sense after the turn. If a wall is now seen directly ahead, count the
    // reading and replan without attempting a forward movement.
    if (API_wallFront())
    {
        sense_wall(mouse_x, mouse_y, mouse_heading, WALL);
        return 0;
    }

    int moved;
#ifdef ARDUINO
    if (cells > 1)
    {
        moved = API_moveForwardCells(cells);
    }
    else
#endif
    {
        cells = 1;
        moved = API_moveForward();
    }

    if (!moved)
    {
#ifdef ARDUINO
        // A real movement failure can leave the chassis between cell centres.
        // Continuing would desynchronise physical and logical coordinates.
        fatal_error = true;
#endif
        return 0;
    }

    for (int i = 0; i < cells; i++)
    {
        confirm_open(old_x, old_y, mouse_heading);

        switch (mouse_heading)
        {
            case NORTH: mouse_y++; break;
            case SOUTH: mouse_y--; break;
            case EAST:  mouse_x++; break;
            case WEST:  mouse_x--; break;
        }

        old_x = mouse_x;
        old_y = mouse_y;
    }

    return 1;
}

static int shortest_path_proven()
{
    int confirmed = confirmed_distance[0][0];
    int optimistic = optimistic_distance[0][0];

    if (confirmed >= INF || optimistic >= INF)
    {
        return 0;
    }

    return confirmed == optimistic;
}

static void draw_distances(int distance[MAX_SIZE][MAX_SIZE])
{
#if DRAW_DISTANCES
    char text[16];

    for (int x = 0; x < width; x++)
    {
        for (int y = 0; y < height; y++)
        {
            if (distance[x][y] >= INF)
            {
                snprintf(text, sizeof(text), "X");
            }
            else
            {
                snprintf(text, sizeof(text), "%d", distance[x][y]);
            }

            API_setText(x, y, text);
        }
    }
#else
    (void)distance;
#endif
}

static void color_goal()
{
    int goal_x1;
    int goal_y1;
    goal_corner(&goal_x1, &goal_y1);
    int goal_x2 = goal_x1 + 1;
    int goal_y2 = goal_y1 + 1;

    API_setColor(goal_x1, goal_y1, 'R');
    API_setColor(goal_x2, goal_y1, 'R');
    API_setColor(goal_x1, goal_y2, 'R');
    API_setColor(goal_x2, goal_y2, 'R');
}

static void handle_simulator_reset()
{
    // On real hardware API_wasReset() always returns false. In MMS this keeps
    // this same floodfill.cpp usable when the simulator Reset button is used.
    if (!API_wasReset())
    {
        return;
    }

    API_ackReset();

    mouse_x = 0;
    mouse_y = 0;
    mouse_heading = NORTH;
    mode = SEARCH_TO_GOAL;
    completed_search_runs = 0;
    completed_fast_runs = 0;

    // Preserve learned maze memory, as with the earlier simulator version.
    log_message("Simulator reset acknowledged; learned maze preserved.");
}

bool floodfill_begin()
{
    width = API_mazeWidth();
    height = API_mazeHeight();

    if (width <= 1 || height <= 1 || width > MAX_SIZE || height > MAX_SIZE)
    {
        log_message("Unsupported maze size.");
        fatal_error = true;
        mode = FINISHED;
        return false;
    }

    fatal_error = false;
    completed_search_runs = 0;
    completed_fast_runs = 0;

    init_maze();

    // Logical map is always rotated so that the only opening from the start
    // is NORTH. This works whichever physical maze corner is used.
    mouse_x = 0;
    mouse_y = 0;
    mouse_heading = NORTH;
    mode = SEARCH_TO_GOAL;

    color_goal();
    log_message("Starting micromouse search.");
    return true;
}

bool floodfill_finished()
{
    return mode == FINISHED || fatal_error;
}

bool floodfill_step()
{
    if (floodfill_finished())
    {
        return false;
    }

    handle_simulator_reset();

    enum Direction next_direction = NORTH;
    int found_move = 0;

    sense_and_record();
    calculate_goal_maps();

    if (fatal_error)
    {
        log_message("Flood-fill queue error.");
        mode = FINISHED;
        return false;
    }

    int optimistic_length = optimistic_distance[0][0];
    int confirmed_length = confirmed_distance[0][0];
    log_lengths(optimistic_length, confirmed_length);

    // ---------------- State transitions ----------------

    if (mode == SEARCH_TO_GOAL && at_goal())
    {
        log_message("Search reached goal; mapping on return to start.");
        mode = SEARCH_TO_START;
    }
    else if (mode == SEARCH_TO_START && at_start())
    {
        completed_search_runs++;

#ifdef ARDUINO
        Serial.print("Completed search run ");
        Serial.println(completed_search_runs);
#endif

        if (shortest_path_proven())
        {
            log_message("Optimistic == confirmed. Switching to fast-route mode.");
            mode = FAST_TO_GOAL;
        }
        else if (completed_search_runs >= MAX_SEARCH_RUNS)
        {
            log_message("Three search runs used. Reserving remaining attempts for fast runs.");
            mode = FAST_TO_GOAL;
        }
        else
        {
            log_message("A shorter route may still exist. Starting another search run.");
            mode = SEARCH_TO_GOAL;
        }
    }
    else if (mode == FAST_TO_GOAL && at_goal())
    {
        log_message("Fast route reached goal; returning to start.");
        mode = FAST_TO_START;
    }
    else if (mode == FAST_TO_START && at_start())
    {
        completed_fast_runs++;

        if (completed_fast_runs >= MAX_FAST_RUNS)
        {
            log_message("Two fast runs completed.");
            mode = FINISHED;
            return false;
        }

        log_message("Starting second fast run.");
        mode = FAST_TO_GOAL;
    }

    // ---------------- Route planning ----------------

    if (mode == SEARCH_TO_GOAL)
    {
        draw_distances(optimistic_distance);
        found_move = choose_search_move(optimistic_distance, &next_direction);
    }
    else if (mode == SEARCH_TO_START)
    {
        flood_fill(TARGET_START, 1, movement_distance);
        draw_distances(movement_distance);
        found_move = choose_search_move(movement_distance, &next_direction);
    }
    else if (mode == FAST_TO_GOAL)
    {
        // Fast-route mode never trusts unknown edges.
        if (confirmed_distance[mouse_x][mouse_y] >= INF)
        {
            log_message("No confirmed path to goal.");
            mode = FINISHED;
            return false;
        }

        draw_distances(confirmed_distance);
        build_fast_turn_cost(confirmed_distance);
        found_move = choose_fast_move(confirmed_distance, &next_direction);
    }
    else if (mode == FAST_TO_START)
    {
        flood_fill(TARGET_START, 0, movement_distance);
        draw_distances(movement_distance);
        build_fast_turn_cost(movement_distance);
        found_move = choose_fast_move(movement_distance, &next_direction);
    }

    if (!found_move)
    {
        log_message("No valid planned move. Stopping safely.");
        mode = FINISHED;
        return false;
    }

    // Fast runs follow a known route: with the tuner switch on, a straight
    // stretch is driven in one go instead of stopping at every cell.
    int cells = 1;
#ifdef ARDUINO
    if (API_fastContinuous())
    {
        if (mode == FAST_TO_GOAL)
        {
            cells = fast_straight_cells(confirmed_distance, next_direction);
        }
        else if (mode == FAST_TO_START)
        {
            cells = fast_straight_cells(movement_distance, next_direction);
        }
    }
#endif

    if (!execute_move(next_direction, cells))
    {
        if (fatal_error)
        {
            log_message("Physical motion fault. Stopping to protect map alignment.");
            mode = FINISHED;
            return false;
        }

        // Normal search case: a newly discovered wall blocked an optimistic
        // move before the robot moved, so the next loop simply replans.
        log_message("New wall blocked planned move; replanning.");
    }

    return true;
}
