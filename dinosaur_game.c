#include <curses.h>
#include <math.h>
#include <time.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define FPS 60

const int KEY_CTRLZ = 26;

typedef struct rect
{
    int height;
    int width;
    int y;
    int x;
} rect;

typedef struct level
{
    int number;
    int max_obstacles;
    int latest_obstacle;
    int min_obstacle_distance;
    int max_obstacle_distance;
    int variability; 
    float gravity;
    float speed;
    bool game_state;
} level;

typedef struct foreground
{
    rect wbox;
    WINDOW *window;
} foreground;

typedef struct dinosaur  
{
    rect hitbox;
    
    // Used to control movement
    float max_dy;
    float dy;
    int ground;

    // ASCII
    char **current_frame;
    char *frame_1[4];
    char *frame_2[4];
} dinosaur;

typedef struct obstacle
{
    rect hitbox;
    char *ascii[];
} obstacle;

// Create dinosaur
dinosaur dino;
foreground fg;

// Function prototypes
void jump(dinosaur *dino);
void update(level *current_level, dinosaur *dino, obstacle **obstacles);
obstacle *generate_obstacle(obstacle **obstacles, level *current_level);
obstacle *obstacle_constructor(int height, int width, int y, int n, ...);
bool detect_object_collisions(rect left, rect right);

int main() 
{
    initscr();
    noecho();

    // Seed
    srand(time(NULL));

    // Resizing the screen
    LINES = 30;
    if (resize_term(LINES, COLS) == ERR)
    {
        endwin();
    }

    // Initialize the level
    level current_level;
    current_level.number = 1;
    current_level.max_obstacles = 2 * current_level.number;
    current_level.latest_obstacle = 0;
    current_level.min_obstacle_distance = round(COLS / current_level.max_obstacles); 
    current_level.max_obstacle_distance = COLS;
    current_level.variability = 10;
    current_level.gravity = -0.40f;
    current_level.speed = 2 * current_level.number;
    current_level.game_state = true;

    // Keeps track of obstacles
    obstacle **obstacles = calloc(current_level.max_obstacles, sizeof(obstacle *));

    // Initialize the foreground's wbox
    rect wbox = {
        .height = 20,
        .width = COLS,
        .y = 5,
        .x = 0
    };

    // Initialize the foreground
    fg.wbox = wbox;
    fg.window = newwin(fg.wbox.height, fg.wbox.width, fg.wbox.y, fg.wbox.x); 
    keypad(fg.window, TRUE);
    nodelay(fg.window, TRUE);

    // Initialize the dinosaur's hitbox
    rect hitbox = {
        .height = 4,
        .width = 9,
        .y = fg.wbox.height - 5,
        .x = 40
    };

    // Initialize the dinosaur
    dino.hitbox = hitbox;
    dino.max_dy = 3.0f;
    dino.dy = 0;
    dino.ground = dino.hitbox.y;
    dino.current_frame = dino.frame_1;

    dino.frame_1[0] = "    :+++-";
    dino.frame_1[1] = "    -*=. ";
    dino.frame_1[2] = " --=**:  ";
    dino.frame_1[3] = "  -+-.   ";

    dino.frame_2[0] = "    :+++-";
    dino.frame_2[1] = "    -*=. ";
    dino.frame_2[2] = " --=**:  ";
    dino.frame_2[3] = "  .+--   ";
    
    chtype ch;

    // Game loop
    while (current_level.game_state)
    {  
        ch = wgetch(fg.window);

        // Exit 
        if (ch == KEY_CTRLZ)
        {
            current_level.game_state = false;
        }
        // Player movement
        else if (ch == 'w')
        {
            jump(&dino);
        }

        // Game status updates
        update(&current_level, &dino, obstacles);

        // Erase the previous iteration of the window before drawing the next iteration of the window
        werase(fg.window);

        // Move the cursor down to the last line of the foreground and draw the ground
        wmove(fg.window, fg.wbox.height - 1, 0);
        for (int i = 0; i < COLS; i++)
        {
            waddch(fg.window, '~');
        }

        // Draws each line of the dinosaur
        for (int i = 0; i < dino.hitbox.height; i++)
        {
            wmove(fg.window, dino.hitbox.y + i, dino.hitbox.x);
            waddstr(fg.window, dino.current_frame[i]);
        }

        // For each obstacle
        for (int i = 0; i < current_level.latest_obstacle; i++)
        {
            // Draws each line of the  obstacle
            for (int j = 0; j < obstacles[i]->hitbox.height; j++)
            {
                wmove(fg.window, obstacles[i]->hitbox.y + j, obstacles[i]->hitbox.x);
                waddstr(fg.window, obstacles[i]->ascii[j]);
            }
        }

        // Terminal screen to update at the set FPS
        wrefresh(fg.window);
        napms(round(1000 / FPS));
    }    

    endwin();

    return 0;
}


bool detect_object_collisions(rect left, rect right)
{
    if (left.x + left.width >= right.x && left.x < right.x + right.width &&
        left.y + left.height > right.y)
    {
        // Has collided
        return true;
    }
    else
    {
        // Did not collide
        return false;
    }
}

void jump(dinosaur *dino)
{
    dino->dy = dino->max_dy;
}

void update(level *current_level, dinosaur *dino, obstacle **obstacles)
{
    // Animate the dinosaur
    if (dino->current_frame >= dino->frame_1 && dino->current_frame < dino->frame_1 + dino->hitbox.height)
    {
        dino->current_frame = dino->frame_2;
    }
    else
    {
        dino->current_frame = dino->frame_1;
    }

    // Move the dinosaur up or down
    (dino->dy > 0) ? (dino->hitbox.y -= floor(dino->dy)) : (dino->hitbox.y -= ceil(dino->dy));

    // Stop the dinosaur from falling through the ground when coming down
    (dino->hitbox.y > dino->ground) ? (dino->hitbox.y = dino->ground) : (void) 0;

    // Apply the effect of gravity
    (dino->hitbox.y != dino->ground) ? (dino->dy += current_level->gravity) : (dino->dy = 0);

    for (int i = 0; i < current_level->latest_obstacle; i++)
    {
        // Move the obstacle left so the dinosaur appears to be moving to the right
        obstacles[i]->hitbox.x -= current_level->speed;

        // Detect if a collision occurs between the dinosaur and the obstacle
        if (detect_object_collisions(dino->hitbox, obstacles[i]->hitbox))
        {
            current_level->game_state = false;
            
            // Pause until the user presses any key
            nodelay(fg.window, FALSE);
            wgetch(fg.window);
            
            return;
        }
    }

    // Then detect if a collision has occurred between the left-most obstacle and the left edge of the screeen
    if (obstacles[0] != NULL && obstacles[0]->hitbox.x <= 0)
    {
        // Remove the obstacle from the array of obstacles by copying the contents of the sub-array of obstacles to the original array of obstacles
        obstacle **sub_array = &obstacles[1];
        obstacles = memmove(obstacles, sub_array, current_level->max_obstacles * sizeof(obstacle *));

        current_level->latest_obstacle--;
    }

    // Generate new obstacles
    obstacle *cactus = generate_obstacle(obstacles, current_level);
    if (cactus != NULL)
    {
        obstacles[current_level->latest_obstacle] = cactus;
        current_level->latest_obstacle++;
    }
}

obstacle *generate_obstacle(obstacle **obstacles, level *current_level)
{
    /* If the number of obstacles currently on the screen is less than the maximum
       If there is at least one obstacle on the screen
       If the previous obstacle is too close to the right edge of the screen 
       If the previous obstacle is not too far from the right edge of the screen that another obstacle must be generated without a good roll
       Then don't generate an obstacle */
    if ((current_level->latest_obstacle > current_level->max_obstacles) ||
        ((obstacles[0] != NULL) &&
         (obstacles[current_level->latest_obstacle - 1]->hitbox.x > COLS - current_level->min_obstacle_distance ||
         (obstacles[current_level->latest_obstacle - 1]->hitbox.x > COLS - current_level->max_obstacle_distance && rand() % current_level->variability > 0))))
    {
        return NULL;
    }

    switch (rand() % 3)
    {
        case 0:
            {
                return obstacle_constructor(4, 9, 15, 4, "   -@% +-", 
                                                         "-% =@@*# ", 
                                                         "=@#%@@   ", 
                                                         "   +@@   ");
            }
        case 1:
            {
                return obstacle_constructor(4, 6, 15, 4, "  %% .", 
                                                         "=:@@+#", 
                                                         " #@@= ", 
                                                         " .%%. ");
            }
        case 2:
            {
                return obstacle_constructor(4, 15, 15, 4, "   -@% +-  %% .", 
                                                          "-% =@@*# =:@@+#", 
                                                          "=@#%@@    #@@= ", 
                                                          "   +@@    .%%. ");
            }
    }
}
 
obstacle *obstacle_constructor(int height, int width, int y, int n, ...)
{
    va_list args;
    va_start(args, n);

    // Initialize the object's hitbox
    rect hitbox = {
        .height = height,
        .width = width,
        .y = y,
        .x = COLS - (width + 1) // All objects will appear from the right edge of the screen, so an x parameter isn't required
    };

    // Initialize the object
    obstacle *cactus = malloc(sizeof(obstacle) + hitbox.height * sizeof(char *));
    cactus->hitbox = hitbox;
    for (int i = 0; i < n; i++)
    {
        cactus->ascii[i] = va_arg(args, char *);
    }

    va_end(args);
    return cactus;
}
