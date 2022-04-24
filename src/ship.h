#include <stdlib.h>
#include <stdbool.h>

typedef struct Ship
{
  unsigned int len;
  unsigned int row;
  unsigned int col;
  unsigned int hit;
  unsigned char rotate;
  bool alive;
  bool placed;
} Ship;

typedef struct Player
{
  unsigned int ship_count;
} Player;