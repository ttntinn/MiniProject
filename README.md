 #include "stm32f0xx.h"
#include "ship.h"
#include "midi.h"
#include "midiplay.h"
#include <string.h>

//===========================================================================
// Function Declarations
//===========================================================================
void update_history(int col, int rows); // record the buttons of the driven column
void update_history2(int col, int rows); // record the buttons of the driven column
char get_keypress(void);  // wait for only a button press event.
void instructions(void);
void spi_cmd(unsigned int);
void users_inputs(void);
void enter_number(void);
void display_string(char []);
void nano_wait(unsigned int);
void drive_column(int);   
void drive_column2(int);
int read_rows();         
int read_rows2();

extern const signed char MISS[58498];
extern const signed char sunk[37368];
extern const signed char HIT[19489];
#define VOICES 15

typedef struct {
    uint8_t in_use;
    uint8_t note;
    uint8_t chan;
    uint8_t volume;
    int     step;
    int     offset;
} Voice;

Voice voice[VOICES];



uint8_t col; // the column being scanned
uint8_t col2;
uint16_t numbers[10] = { 0x200+'0',0x200+'1',0x200+'2',0x200+'3',0x200+'4',0x200+'5',0x200+'6',0x200+'7',0x200+'8',0x200+'9'};
char keypad1_numbers[10] = {'0' | 0x80, '1' | 0x80, '2' | 0x80, '3' | 0x80, '4' | 0x80, '5' | 0x80, '6' | 0x80, '7' | 0x80, '8' | 0x80, '9' | 0x80};
char keypad2_numbers[10] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};
char keypad1_char[6] = {'*' | 0x80, '#' | 0x80, 'D' | 0x80, 'C' | 0x80, 'B' | 0x80, 'A' | 0x80};
char keypad2_char[6] = {'*', '#', 'D', 'C', 'B', 'A'};
uint16_t display[34] = {
        0x002, // Command to set the cursor at the first position line 1
        0x200+' ', 0x200+' ', 0x200+' ', 0x200+' ', 0x200+' ', + 0x200+' ', 0x200+' ', 0x200+' ',
        0x200+' ', 0x200+' ', 0x200+' ', 0x200+' ', + 0x200+' ', 0x200+' ', 0x200+' ', 0x200+' ',
        0x0c0, // Command to set the cursor at the first position line 2
        0x200+' ', 0x200+' ', 0x200+' ', 0x200+' ', 0x200+' ', + 0x200+' ', 0x200+' ', 0x200+' ',
        0x200+' ', 0x200+' ', 0x200+' ', 0x200+' ', + 0x200+' ', 0x200+' ', 0x200+' ', 0x200+' ',
};
int display_size = sizeof(display)/sizeof(display[0]) - 1;

void TIM7_IRQHandler()
{
    TIM7->SR &= ~TIM_SR_UIF;
    // Keypad 1
    int rows = read_rows();
    update_history(col, rows);
    col = (col + 1) & 3;
    drive_column(col);

    // Keypad 2
    int rows2 = read_rows2();
    update_history2(col2, rows2);
    col2 = (col2 + 1) & 3;
    drive_column2(col2);
}

void init_tim7(void)
{
    RCC->APB1ENR |= 1<<5;
    TIM7->PSC = 48-1;
    TIM7->ARR = 100-1;
    TIM7->DIER |= 1<<0;
    NVIC->ISER[0] = 1<<TIM7_IRQn;
    TIM7->CR1 |= 1<<0;
}

#define KPRATE 100
#define NUMSHIP 5

// Init game
char game_mode = 's';
/*
game mode are: Will need more mode inside here tho :( like a FSM sort of
"s" start: Starting screen
"p" place: Placing ship
"g" game : Game is playing
"e" end  : Ending
*/

// Init Player
Player p1 = {.ship_count=5};
Player p2 = {.ship_count=5};

// Init Ship
Ship p1_ship0 = {.len=1, .row=30, .col=1, .rotate='v', .alive=true};
Ship p1_ship1 = {.len=1, .row=30, .col=1, .rotate='v', .alive=true};
Ship p1_ship2 = {.len=1, .row=30, .col=1, .rotate='v', .alive=true};
Ship p1_ship3 = {.len=1, .row=30, .col=1, .rotate='v', .alive=true};
Ship p1_ship4 = {.len=1, .row=30, .col=1, .rotate='v', .alive=true};
Ship* p1_ships[5] = {&p1_ship0, &p1_ship1, &p1_ship2, &p1_ship3, &p1_ship4}; //Array of p1_ships

Ship p2_ship0 = {.len=1, .row=30, .col=1, .rotate='v', .alive=true};
Ship p2_ship1 = {.len=1, .row=30, .col=1, .rotate='v', .alive=true};
Ship p2_ship2 = {.len=1, .row=30, .col=1, .rotate='v', .alive=true};
Ship p2_ship3 = {.len=1, .row=30, .col=1, .rotate='v', .alive=true};
Ship p2_ship4 = {.len=1, .row=30, .col=1, .rotate='v', .alive=true};
Ship* p2_ships[5] = {&p2_ship0, &p2_ship1, &p2_ship2, &p2_ship3, &p2_ship4}; //Array of p2_ships

unsigned int p1_shipnum = 0;
unsigned int p2_shipnum = 0;

// Init placing
bool p1_place_done = false;
bool p2_place_done = false;

bool p1_turn = true;
bool p2_turn = false;

bool p1_win = false;
bool p2_win = false;

bool sink_sound = false;
bool hit_sound  = false;
bool miss_sound = false;

//Game Screen Player 1
/*
Each point is 8 bit
point[2:0] RGB
point[5:3] ship num
point[8]   miss
point[6]   hit
point[7]   place 

*/
uint16_t p1_screen [32][16] = {
  {0,0,0,0,0,0,0,0,0,0,0,0,0,4,4,4},
  {0,0,0,0,0,0,0,0,0,0,0,0,4,4,4,4},
  {0,0,0,0,0,7,0,0,0,0,0,4,4,4,4,4},
  {0,0,0,0,0,7,0,0,0,0,0,4,4,4,4,4},
  {0,0,0,0,0,7,0,7,0,0,0,4,4,4,4,4},
  {0,0,0,0,7,7,7,7,7,0,0,0,4,4,4,4},
  {0,0,0,0,7,7,7,7,7,7,0,0,4,4,4,4},
  {0,0,0,0,0,0,0,7,7,7,7,0,0,4,4,4},
  {0,0,0,0,0,0,0,7,7,7,7,7,7,4,4,4},
  {0,0,0,0,0,0,0,0,7,7,7,7,4,4,4,4},
  {0,0,0,0,0,0,0,7,7,7,7,4,4,4,4,4},
  {0,0,0,0,0,0,7,7,7,7,4,4,4,4,4,4},
  {0,0,0,0,1,0,7,7,7,7,4,4,4,4,4,4},
  {0,0,0,0,1,0,7,7,7,7,4,4,4,4,4,4},
  {1,1,1,1,1,0,0,7,7,7,7,4,4,4,4,4},
  {0,1,0,0,1,0,0,0,7,7,7,7,4,4,4,4},
  {0,0,0,0,1,0,0,0,7,7,7,7,7,4,4,4},
  {0,0,0,0,0,0,0,7,7,7,7,7,4,4,4,4},
  {1,1,1,0,0,0,0,7,7,7,7,4,4,4,4,4},
  {1,0,1,0,0,0,0,7,7,7,7,4,4,4,4,4},
  {1,1,1,1,1,0,0,0,7,7,7,4,4,4,4,4},
  {0,0,0,0,0,0,0,0,7,7,7,7,4,4,4,4},
  {0,0,0,0,7,7,7,7,7,7,7,7,7,4,4,4},
  {0,0,0,7,7,7,7,7,7,7,7,7,4,4,4,4},
  {0,0,0,7,7,7,7,7,7,7,7,7,4,4,4,4},
  {0,0,0,0,7,7,7,7,7,7,7,4,4,4,4,4},
  {0,0,0,0,0,0,0,0,7,7,7,4,4,4,4,4},
  {3,3,0,0,0,0,0,7,7,7,7,4,4,4,4,4},
  {3,3,3,0,0,0,7,7,7,7,7,7,4,4,4,4},
  {3,3,3,3,0,0,7,7,7,7,7,0,4,4,4,4},
  {3,3,3,3,3,0,0,0,0,0,0,0,0,4,4,4},
  {3,3,3,3,3,0,0,0,0,0,0,0,0,0,0,4},
};

//Game Screen Player 2
uint16_t p2_screen [32][16] = {
  {0,0,0,0,0,0,0,0,0,0,0,0,0,4,4,4},
  {0,0,0,0,0,0,0,0,0,0,0,0,4,4,4,4},
  {0,0,0,0,0,7,0,0,0,0,0,4,4,4,4,4},
  {0,0,0,0,0,7,0,0,0,0,0,4,4,4,4,4},
  {0,0,0,0,0,7,0,7,0,0,0,4,4,4,4,4},
  {0,0,0,0,7,7,7,7,7,0,0,0,4,4,4,4},
  {0,0,0,0,7,7,7,7,7,7,0,0,4,4,4,4},
  {0,0,0,0,0,0,0,7,7,7,7,0,0,4,4,4},
  {0,0,0,0,0,0,0,7,7,7,7,7,7,4,4,4},
  {0,0,0,0,0,0,0,0,7,7,7,7,4,4,4,4},
  {0,0,0,0,0,0,0,7,7,7,7,4,4,4,4,4},
  {0,0,0,0,0,0,7,7,7,7,4,4,4,4,4,4},
  {0,0,0,0,0,0,7,7,7,7,4,4,4,4,4,4},
  {0,1,0,0,1,0,7,7,7,7,4,4,4,4,4,4},
  {1,0,1,0,1,0,0,7,7,7,7,4,4,4,4,4},
  {1,0,0,1,1,0,0,0,7,7,7,7,4,4,4,4},
  {0,0,0,0,0,0,0,0,7,7,7,7,7,4,4,4},
  {0,0,0,0,0,0,0,7,7,7,7,7,4,4,4,4},
  {1,1,1,0,0,0,0,7,7,7,7,4,4,4,4,4},
  {1,0,1,0,0,0,0,7,7,7,7,4,4,4,4,4},
  {1,1,1,1,1,0,0,0,7,7,7,4,4,4,4,4},
  {0,0,0,0,0,0,0,0,7,7,7,7,4,4,4,4},
  {0,0,0,0,7,7,7,7,7,7,7,7,7,4,4,4},
  {0,0,0,7,7,7,7,7,7,7,7,7,4,4,4,4},
  {0,0,0,7,7,7,7,7,7,7,7,7,4,4,4,4},
  {0,0,0,0,7,7,7,7,7,7,7,4,4,4,4,4},
  {0,0,0,0,0,0,0,0,7,7,7,4,4,4,4,4},
  {3,3,0,0,0,0,0,7,7,7,7,4,4,4,4,4},
  {3,3,3,0,0,0,7,7,7,7,7,7,4,4,4,4},
  {3,3,3,3,0,0,7,7,7,7,7,0,4,4,4,4},
  {3,3,3,3,3,0,0,0,0,0,0,0,0,4,4,4},
  {3,3,3,3,3,0,0,0,0,0,0,0,0,0,0,4},
};

uint16_t win_screen [32][16] = {
  {0,7,7,7,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,7,7,7,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,7,7,7,7,7,7,7,7,7,7,7,7,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,7,7,7,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,2,2,0,0,0,0,2,2,0,0,0,0},
  {0,0,0,0,2,2,0,0,0,0,2,2,7,7,0,0},
  {0,7,7,0,2,2,0,2,2,0,2,2,0,0,0,0},
  {0,7,7,0,2,2,2,0,0,2,2,2,0,0,0,0},
  {0,0,7,0,0,2,0,7,7,7,2,7,7,7,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,2,2,2,2,2,2,2,2,7,7,7,0},
  {0,0,0,0,0,0,0,2,2,0,0,0,7,7,7,0},
  {0,7,7,7,7,7,7,2,2,7,7,7,7,7,0,0},
  {0,7,7,7,7,7,7,2,2,7,7,7,7,7,0,0},
  {0,0,0,0,2,2,2,2,2,2,2,2,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,2,2,0,0,0,0,2,2,0,0,0,0},
  {0,0,0,0,2,2,2,0,0,0,2,2,0,0,0,0},
  {0,0,0,0,2,2,0,2,0,0,2,2,0,0,0,0},
  {0,0,0,0,2,2,0,0,2,0,2,2,0,0,0,0},
  {0,0,0,0,2,2,0,0,0,2,2,2,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,1,1,1,0,0,0,7,7,7,0},
  {0,0,0,0,0,1,1,1,1,1,0,0,7,7,7,0},
  {0,0,0,0,1,3,3,3,3,1,1,0,7,7,7,0},
  {0,0,0,1,1,3,0,0,0,3,1,1,7,7,7,0},
  {0,0,0,1,1,1,3,0,0,3,1,0,7,7,7,0},
  {0,0,0,7,7,1,1,3,3,1,7,7,7,7,0,0},
  {0,0,7,7,7,7,1,1,1,7,7,7,7,7,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,7,7,7,0},
  {0,7,7,7,7,7,7,7,7,7,7,7,7,7,0,0},
};

uint16_t lose_screen [32][16] = {
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,,0,0},
  {0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0},
  {0,0,1,1,0,0,0,0,0,0,0,0,1,1,0,0},
  {0,0,1,1,0,0,0,0,0,0,0,0,1,1,0,0},
  {0,0,1,1,0,0,0,0,0,0,0,0,1,1,0,0},
  {0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0},
  {0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0},
  {0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0},
  {0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
 
};
// uint8_t *** p_screen = malloc(sizeof(uint8_t**) * 2);
// p_screen[0] = p1_screen;
// p_screen[1] = p2_screen;


int p1_tarRow = 14;
int p1_tarCol = 1;
int p2_tarRow = 14;
int p2_tarCol = 1;
void init_points() {
  p1_screen[p1_tarRow][p1_tarCol] &= ~0x7;
  p1_screen[p1_tarRow][p1_tarCol] |= 0x1;
  p2_screen[p2_tarRow][p2_tarCol] &= ~0x7;
  p2_screen[p2_tarRow][p2_tarCol] |= 0x1;
}

void deletePoint(int player)
{
  if (player == 1) {
    if (p1_screen[p1_tarRow][p1_tarCol] & (1<<6)) { //hit
      p1_screen[p1_tarRow][p1_tarCol] |= 0x7;
    }
    else if (p1_screen[p1_tarRow][p1_tarCol] & (1<<8)) { //missed already
      p1_screen[p1_tarRow][p1_tarCol] &= ~0x7;
      p1_screen[p1_tarRow][p1_tarCol] |= 0x4;
    }
    else
    {
      //make it black
      p1_screen[p1_tarRow][p1_tarCol] &= ~0x7;
    }
  }
  else {
    if (p2_screen[p2_tarRow][p2_tarCol] & (1<<6)) { //hit
      p2_screen[p2_tarRow][p2_tarCol] |= 0x7;
    }
    else if (p2_screen[p2_tarRow][p2_tarCol] & (1<<8)) { //missed already
      p2_screen[p2_tarRow][p2_tarCol] &= ~0x7;
      p2_screen[p2_tarRow][p2_tarCol] |= 0x4;
    }
    else
    {
      //make it black
      p2_screen[p2_tarRow][p2_tarCol] &= ~0x7;
    }
  }

}

void displayPoint(int player) {
  if (player == 1)
  {
    p1_screen[p1_tarRow][p1_tarCol] &= ~0x7;
    p1_screen[p1_tarRow][p1_tarCol] |= 0x1;
  }
  else
  {
    p2_screen[p2_tarRow][p2_tarCol] &= ~0x7;
    p2_screen[p2_tarRow][p2_tarCol] |= 0x1;
  }
}

void movePoint(int moveVal, int player){
  bool restore = false;
  if (player == 1) {
    deletePoint(player);
    if (moveVal == 2){ //move up
      if ((p1_tarRow) > 1) {
        p1_tarRow--;
      }
    }
    else if (moveVal == 8){ //move down
      if (p1_tarRow < 14){
        p1_tarRow++;
      }
    }
    else if (moveVal == 4){ //move left
      if (p1_tarCol > 1){
        p1_tarCol--;
      }
      
    }
    else { //move right
      if ((p1_tarCol) < 14){
        p1_tarCol++;
      }
        
    }
    // if (moveVal == 2) p1_tarRow--;
    // if (moveVal == 8) p1_tarRow++;
    // if (moveVal == 4) p1_tarCol--;
    // if (moveVal == 6) p1_tarCol++;
    displayPoint(player);
  }
  else {
    deletePoint(player);
     if (moveVal == 2){ //move up
      if ((p2_tarRow) > 1) {
        p2_tarRow--;
      }
    }
    else if (moveVal == 8){ //move down
      if (p2_tarRow < 14){
        p2_tarRow++;
      }
    }
    else if (moveVal == 4){ //move left
      if (p2_tarCol > 1){
        p2_tarCol--;
      }
      
    }
    else { //move right
      if ((p2_tarCol) < 14){
        p2_tarCol++;
      }
        
    }
    // if (moveVal == 2) p2_tarRow--;
    // if (moveVal == 8) p2_tarRow++;
    // if (moveVal == 4) p2_tarCol--;
    // if (moveVal == 6) p2_tarCol++;
    displayPoint(player);
  }


}

void clearScreen()
{
  for(int i=0; i<32; i++)
  {
    for(int j=0; j<16; j++)
    {
      p1_screen[i][j] = 0;
      p2_screen[i][j] = 0;
    }
  }
}



void init_shipScreen(int player)
{
  if (player == 1){
    Ship * p1_ship = p1_ships[p1_shipnum];
    for (int j=0; j<p1_ship->len; j++) {
      p1_screen[p1_ship->row-j][p1_ship->col] |= 7; //Set it to white
    }
  }
  else {
    Ship * p2_ship = p2_ships[p2_shipnum];
    for (int k=0; k<p2_ship->len; k++) {
      p2_screen[p2_ship->row-k][p2_ship->col] |= 7; //Set it to white
    }
  }
}


void init_gpiob(){
  RCC->AHBENR |= RCC_AHBENR_GPIOBEN;

  //Configure MODER set PB0~PB12 as output
  GPIOB->MODER |= GPIO_MODER_MODER0_0;
  GPIOB->MODER |= GPIO_MODER_MODER1_0;
  GPIOB->MODER |= GPIO_MODER_MODER2_0;
  GPIOB->MODER |= GPIO_MODER_MODER3_0;
  GPIOB->MODER |= GPIO_MODER_MODER4_0;
  GPIOB->MODER |= GPIO_MODER_MODER5_0;
  GPIOB->MODER |= GPIO_MODER_MODER6_0;
  GPIOB->MODER |= GPIO_MODER_MODER7_0;
  GPIOB->MODER |= GPIO_MODER_MODER8_0;
  GPIOB->MODER |= GPIO_MODER_MODER9_0;
  GPIOB->MODER |= GPIO_MODER_MODER10_0;
  GPIOB->MODER |= GPIO_MODER_MODER11_0;
  GPIOB->MODER |= GPIO_MODER_MODER12_0;
}

void init_gpioa(){
  RCC->AHBENR |= RCC_AHBENR_GPIOAEN;

  //0,1,2,3,6,8 Output for screen
  GPIOA->MODER |= GPIO_MODER_MODER0_0;
  GPIOA->MODER |= GPIO_MODER_MODER1_0;
  GPIOA->MODER |= GPIO_MODER_MODER2_0;
  GPIOA->MODER |= GPIO_MODER_MODER3_0;
  GPIOA->MODER |= GPIO_MODER_MODER6_0;
  GPIOA->MODER |= GPIO_MODER_MODER8_0;

  // AFR for SPI
    // PA5  SPI1_SCK
    // PA7  SPI1_MOSI
    // PA15 SPI1_NSS
  GPIOA->MODER &= ~(3<<(2*5) | 3<<(2*7) | 3<<(2*15));
  GPIOA->MODER |=  (2<<(2*5) | 2<<(2*7) | 2<<(2*15));

  // DAC
  GPIOA->MODER |= 3<<(2*4) ;
}

void init_gpioc() {
    //===========================================================================
    // Key Pad 1 & 2
    //===========================================================================
    // Enable port C
    RCC->AHBENR |= RCC_AHBENR_GPIOCEN;

    // Outputs [Keypad 1: (PC4-PC7) | Keypad 2: (PC12-PC15)]
    GPIOC->MODER &= ~0xff00ff00;
    GPIOC->MODER |= 0x55005500;

    // Output type open-drain
    GPIOC->OTYPER &= ~0xf0f0;
    GPIOC->OTYPER |= 0xf0f0;

    // Inputs [Keypad 1: (PC0-PC3) | Keypad 2: (PC8-PC11)]
    GPIOC->MODER &= ~0xff00ff;

    // Inputs pulled high
    GPIOC->PUPDR &= ~0xff00ff;
    GPIOC->PUPDR |= 0x550055;
}

void p1_placing_handler(char key) {
  if (key == '2') moveShip(p1_ships[p1_shipnum], 2, 1);
  if (key == '8') moveShip(p1_ships[p1_shipnum], 8, 1);
  if (key == '4') moveShip(p1_ships[p1_shipnum], 4, 1);
  if (key == '6') moveShip(p1_ships[p1_shipnum], 6, 1);
  if (key == '*') placeShip(p1_ships[p1_shipnum], 1);
  if (key == '#') rotate_ship(p1_ships[p1_shipnum],1);

}
void p2_placing_handler(char key) {
  if (key == ('2' | 0x80)) moveShip(p2_ships[p2_shipnum], 2, 2);
  if (key == ('8' | 0x80)) moveShip(p2_ships[p2_shipnum], 8, 2);
  if (key == ('4' | 0x80)) moveShip(p2_ships[p2_shipnum], 4, 2);
  if (key == ('6' | 0x80)) moveShip(p2_ships[p2_shipnum], 6, 2);
  if (key == ('*' | 0x80)) placeShip(p2_ships[p2_shipnum], 2);
  if (key == ('#' | 0x80)) rotate_ship(p2_ships[p2_shipnum],2);  
}

void p1_playing(char key) {
  if (key == '2') movePoint(2, 1);
  if (key == '8') movePoint(8, 1);
  if (key == '4') movePoint(4, 1);
  if (key == '6') movePoint(6, 1);
  if (key == '*') fire(1);
}

void p2_playing(char key) {
  if (key == ('2' | 0x80)) movePoint(2, 2);
  if (key == ('8' | 0x80)) movePoint(8, 2);
  if (key == ('4' | 0x80)) movePoint(4, 2);
  if (key == ('6' | 0x80)) movePoint(6, 2);
  if (key == ('*' | 0x80)) fire(2);
}

void init_tim6(void)
{
  RCC->APB1ENR |= RCC_APB1ENR_TIM6EN;
  TIM6->PSC = 100-1;
  TIM6->ARR = (480000/KPRATE)-1;
  TIM6->DIER |= TIM_DIER_UIE;
  TIM6->CR1 |= TIM_CR1_CEN;
  NVIC->ISER[0] |= 1<<TIM6_DAC_IRQn;
}

void TIM6_DAC_IRQHandler()
{
  TIM6->SR &= ~TIM_SR_UIF;
  char key = pop_queue();
  if (game_mode == 's' && (key == '5' || key==('5' | 0x80)))
  {
    clearScreen();
    init_shipScreen(1);
    init_shipScreen(2);
    update_placing(1);
    game_mode='p';
  }
  if (game_mode == 'p')
  {
    if (p1_place_done && p2_place_done) 
      {
        copy_ships(); // copy p1 ships to p2_screen array
        init_points();
        update_turn(1);
        game_mode='g';
      }
    if (!p1_place_done) p1_placing_handler(key);
    if (!p2_place_done) p2_placing_handler(key);
  }
  if (game_mode == 'g')
  {
    if (p1_win || p2_win) {
      displayWinner();
      game_mode = 'e';
    }
    if (p1_turn) 
    {
      p1_playing(key);
    }
    else 
    {
      p2_playing(key);
    }
  }

}

void update_placing() {
  for (int i = 0; i < 16; i++) {
     p1_screen[0][i] = 4;
     p1_screen[16][i] = 4;
     p1_screen[15][i] = 4;
     p1_screen[31][i] = 4;

     p2_screen[0][i] = 4;
     p2_screen[16][i] = 4;
     p2_screen[15][i] = 4;
     p2_screen[31][i] = 4;
   }
  for (int j = 0; j < 32; j++){
     p1_screen[j][0] = 4;
     p2_screen[j][0] = 4;
     p1_screen[j][15] = 4;
     p2_screen[j][15] = 4;
   }
   
}

void update_turn(int player){
 if (player == 1) {
   for (int j = 0; j < 32; j++){
     p1_screen[j][0] = 2;
     p2_screen[j][0] = 1;
     p1_screen[j][15] = 2;
     p2_screen[j][15] = 1;
   }
   for (int i = 0; i < 16; i++) {
     p1_screen[0][i] = 2;
     p1_screen[16][i] = 2;
     p1_screen[15][i] = 2;
     p1_screen[31][i] = 2;

     p2_screen[0][i] = 1;
     p2_screen[16][i] = 1;
     p2_screen[15][i] = 1;
     p2_screen[31][i] = 1;

   }
 }
 else {
   for (int j = 0; j < 32; j++){
     p1_screen[j][0] = 1;
     p2_screen[j][0] = 2;
     p1_screen[j][15] = 1;
     p2_screen[j][15] = 2;
   }
   for (int i = 0; i < 16; i++) {
     p1_screen[0][i] = 1;
     p1_screen[16][i] = 1;
     p1_screen[15][i] = 1;
     p1_screen[31][i] = 1;

     p2_screen[0][i] = 2;
     p2_screen[16][i] = 2;
     p2_screen[15][i] = 2;
     p2_screen[31][i] = 2;

   }
 }
}

int main(void)
{
  init_gpiob();
  init_gpioa();
  init_gpioc();
  init_tim6();
  init_tim7();
  init_tim15();
  init_wavetable_hybrid2();
  init_dac();
  init_tim2();
  MIDI_Player *mp = midi_init(pirates);
  init_tim3(10417);

  
  init_spi1();
  spi1_init_oled();
  spi1_setup_dma();
  spi1_enable_dma();



  while(1) {
    for (int col =0; col <8; col++) {
      // transferScreen();
      for (int row = 32; row > -1; row--) {
        GPIOB->ODR = (p1_screen[row][col] & 0x7   )   | (p1_screen[row][col+8]<<3 & 0x38)
                   | (p2_screen[row][col]<<6 & 0x1C0) | (p2_screen[row][col+8]<<9 & 0xE00);; //set output
        GPIOA->BSRR |= 1<<3; //CLK high
        //nano_wait(10);
        GPIOA->BRR |= 1<<3; //CLK low
        //nano_wait(10);
      }
      GPIOA->BSRR |= 1<<6; //OE high
      GPIOA->BRR |= 0x7;
      GPIOA->BSRR |= col;
      GPIOA->BSRR |= 1<<8; //LA high
      GPIOA->BRR  |= 1<<8; //LA low
      GPIOA->BRR |= 1<<6; //OE low
      // nano_wait(10000000);
    }
  }

}


void rotate_ship(Ship * ship, int player) {
  if (player == 1) {
    deleteShip(ship, 1);
    if (ship->rotate == 'v') 
      {ship->rotate ='h';}
    else 
      {ship->rotate = 'v';}
    displayShip(ship, false, 1);    
  }
  else {
    deleteShip(ship, 2);
    if (ship->rotate == 'v') 
      {ship->rotate ='h';}
    else 
      {ship->rotate = 'v';}
    displayShip(ship, false, 2);        
  }

}

void deleteShip(Ship *ship, int player){
  if (player == 1) {
    if(ship->rotate == 'v'){
      for(int i = 0; i < ship->len; i++){
        if (!(p1_screen[ship->row - i][ship->col] & (1<<7)))
          {p1_screen[ship->row - i][ship->col] &= ~0x7;}
      }
    }
    else{
      for(int i = 0; i < ship->len; i++){
        if(!(p1_screen[ship->row][ship->col + i] & (1<<7)))
          {p1_screen[ship->row][ship->col + i] &= ~0x7;}
      }
    }
  }
  else {
    if(ship->rotate == 'v'){
      for(int i = 0; i < ship->len; i++){
        if (!(p2_screen[ship->row - i][ship->col] & (1<<7)))
          {p2_screen[ship->row - i][ship->col] &= ~0x7;}
      }
    }
    else{
      for(int i = 0; i < ship->len; i++){
        if(!(p2_screen[ship->row][ship->col + i] & (1<<7)))
          {p2_screen[ship->row][ship->col + i] &= ~0x7;}
      }
    }    
  }
  

}

void displayShip(Ship *ship, bool place, int player){
  if (player == 1) {
    if (ship->rotate == 'v'){
      for (int i = 0; i < ship->len; i++){
        if (place) 
          p1_screen[ship->row - i][ship->col] |= (1<<7 | (p1_shipnum<<3 & 0x38));
        else
          p1_screen[ship->row - i][ship->col] |= 0x7;
      }
    }
    else {
      for (int i =0; i<ship->len; i++) {
        if (place)
          p1_screen[ship->row][ship->col+i] |= (1<<7 | (p1_shipnum<<3 & 0x38)) ;
        else
          p1_screen[ship->row][ship->col+i] |= 0x7;
      }
    }
  }
  else {
    if (ship->rotate == 'v'){
      for (int i = 0; i < ship->len; i++){
        if (place) 
          p2_screen[ship->row - i][ship->col] |= (1<<7 | (p2_shipnum<<3 & 0x38));
        else
          p2_screen[ship->row - i][ship->col] |= 0x7;
      }
    }
    else {
      for (int i =0; i<ship->len; i++) {
        if (place)
          p2_screen[ship->row][ship->col+i] |= (1<<7 | (p2_shipnum<<3 & 0x38)) ;
        else
          p2_screen[ship->row][ship->col+i] |= 0x7;
      }
    }    
  }

}

bool checkPlace(Ship *ship, int player){
  if (player == 1){
    if (ship->rotate == 'v'){
      for (int i = 0; i < ship->len; i++){
        if (p1_screen[ship->row - i][ship->col] & 1<<7)
          return false;
      }
    }
    else {
      for (int i =0; i<ship->len; i++) {
        if (p1_screen[ship->row][ship->col+i] & 1<<7) 
          return false;
      }
    }
    return true;
  }
  else {
    if (ship->rotate == 'v'){
      for (int i = 0; i < ship->len; i++){
        if (p2_screen[ship->row - i][ship->col] & 1<<7)
          return false;
      }
    }
    else {
      for (int i =0; i<ship->len; i++) {
        if (p2_screen[ship->row][ship->col+i] & 1<<7) 
          return false;
      }
    }
    return true;    
  }

}

void moveShip(Ship * ship, int moveVal, int player){
  if (player == 1) {
    deleteShip(ship, 1);
    if (moveVal == 2){ //move up
      if ((ship->row - ship->len + 1) > 17) {
        ship->row -= 1;
      }
    }
    else if (moveVal == 8){ //move down
      if (ship->row < 30){
        ship->row += 1;
      }
    }
    else if (moveVal == 4){ //move left
      if (ship->col > 1){
        ship->col -= 1;
      }
      
    }
    else { //move right
      if ((ship->col + ship->len) < 15){
        ship->col += 1;
      }
        
    }
    displayShip(ship, false, 1);
  }
  else {
    deleteShip(ship, 2);
     if (moveVal == 2){ //move up
      if ((ship->row - ship->len + 1) > 17) {
        ship->row -= 1;
      }
    }
    else if (moveVal == 8){ //move down
      if (ship->row < 30){
        ship->row += 1;
      }
    }
    else if (moveVal == 4){ //move left
      if (ship->col > 1){
        ship->col -= 1;
      }
      
    }
    else { //move right
      if ((ship->col + ship->len) < 15){
        ship->col += 1;
      }
        
    }
    displayShip(ship, false, 2);
  }

}


void placeShip(Ship* ship, int player) {
  if (player == 1) {
    bool place = checkPlace(ship, 1);
    if (place){
      ship ->placed = true;
      displayShip(ship, true, 1);
      if (p1_shipnum !=4) {p1_shipnum++;}
      else {
        p1_place_done = true;
        return;
      }
      
      init_shipScreen(1);
    }    
  }
  else {
    bool place = checkPlace(ship, 2);
    if (place){
      ship ->placed = true;
      displayShip(ship, true, 2);
      if (p2_shipnum !=4) {p2_shipnum++;}
      else {
        p2_place_done = true;
        return;
      }
      
      init_shipScreen(2);
    }        
  }

}

void spi1_setup_dma(void)
{
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    DMA1_Channel3->CCR &= ~DMA_CCR_EN;// Disable Channel
    DMA1_Channel3->CPAR = &(SPI1->DR);// Set CPAR to the address of the SPI1_DR register.
    DMA1_Channel3->CMAR = (uint32_t)display; // Set CMAR to the display array base address
    DMA1_Channel3->CNDTR = 34; // Set CNDTR to 34
    DMA1_Channel3->CCR |= DMA_CCR_DIR; // Set the DIRection for copying from-memory-to-peripheral
    DMA1_Channel3->CCR |= DMA_CCR_MINC; // Set the MINC to increment the CMAR for every transfer.
    DMA1_Channel3->CCR |= DMA_CCR_MSIZE_0; // Set the memory datum size to 16-bit.
    DMA1_Channel3->CCR |= DMA_CCR_PSIZE_0; // Set the peripheral datum size to 16-bit.
    DMA1_Channel3->CCR |= DMA_CCR_CIRC; // Set the channel for CIRCular operation.
    SPI1->CR2 |= SPI_CR2_TXDMAEN;
}

void spi1_enable_dma(void)
{
    DMA1_Channel3->CCR |= 0x1;
}

void init_spi1() {
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

    SPI1->CR1 &= ~SPI_CR1_SPE;
    SPI1->CR1 |= SPI_CR1_BR;
    SPI1->CR1 |= SPI_CR1_MSTR;
    SPI1->CR2 = SPI_CR2_DS_0 | SPI_CR2_DS_3;
    SPI1->CR2 |= SPI_CR2_NSSP;
    SPI1->CR2 |= SPI_CR2_TXDMAEN;
    SPI1->CR2 |= SPI_CR2_SSOE;
    SPI1->CR1 |= SPI_CR1_SPE;
}

void spi1_init_oled() {
    nano_wait(1000000);
    spi_cmd(0x38);
    spi_cmd(0x08);
    spi_cmd(0x01);
    nano_wait(2000000);
    spi_cmd(0x06);
    spi_cmd(0x02);
    spi_cmd(0x0c);
}

void spi_cmd(unsigned int data) {
    while(!(SPI1->SR & SPI_SR_TXE)) {}
    SPI1->DR = data;
}

void small_delay(void) {
    nano_wait(50000000);
}
void display_string(char string [])
{
    int j = 0;
    int diff = 0;
    int string_size = strlen(string);
    if (string_size < display_size-1)
        diff = display_size - string_size;
    for (int i = 0; i <= display_size; i++)
    {
        if (i != 0 && i != 17)
        {
            if (j >= display_size - diff)
                display[i] = 0x200+' ';
            else{
                display[i] = 0x200+string[j];
                j++;
            }
        }
    }
}

void init_tim15(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_TIM15EN;
    TIM15->PSC = 48000-1;
    TIM15->ARR = 200-1;
    TIM15->DIER |= TIM_DIER_UIE;
    NVIC->ISER[0] = 1<<20;
    TIM15->CR1 |= TIM_CR1_CEN;
}

void TIM15_IRQHandler()
{
  TIM15->SR &= ~TIM_SR_UIF;
  unsigned int player1_ships = p1_shipnum + 1;
  unsigned int player2_ships = p2_shipnum + 1;
  char charreplace;
  char start [] = "Press 5 to Start";
  char instruct1 [] = "2=U 8=D 4=L 6=Ri#=Rotate *=Place";
  char game [] = "P1: x P2: x     Press * to FIRE!";
  char endGame [] = "Player x wins!  PlayAgain->Reset";
  itoa(player2_ships, &charreplace, 10);
  game[10] = charreplace;
  itoa(player1_ships, &charreplace, 10);
  game[4] = charreplace;
  if (game_mode == 's')
    display_string(start);
  else if (game_mode == 'p')
    display_string(instruct1);
  else if (game_mode == 'g')
    display_string(game);
  else if (game_mode == 'e'){
    if (p1_shipnum == 0)
      endGame[7] = '2';
    else if (p2_shipnum == 0)
      endGame[7] = '1';
    display_string(endGame);
  }
}

void checkSink(Ship * ship, int player) {
  if (ship->hit == ship->len) {
    sink_sound = true;
    ship->alive = false;
    if (player == 1) {
      if (p2_shipnum == 0) {
        p1_win = true;
        return;
      }
      p2_shipnum--;
    }
    else if (player ==2) {
      if (p1_shipnum == 0) {
        p2_win = true;
        return;
      }
      p1_shipnum--;
    }
  }
}

void hit_handler(player) {
  hit_sound = true;
  //mark hit, turn white, check for which ship is hit, then check sink
  if (player == 1) {
    p1_screen[p1_tarRow][p1_tarCol] |= 1<<6 | 0x7;
    unsigned int ship_hit = (p1_screen[p1_tarRow][p1_tarCol] & 0x38) >> 3;
    p2_ships[ship_hit] -> hit += 1;
    checkSink( p2_ships[ship_hit], player);
  }
  else {
    p2_screen[p2_tarRow][p2_tarCol] |= 1<<6 | 0x7;
    unsigned int ship_hit = (p2_screen[p2_tarRow][p2_tarCol] & 0x38) >> 3;
    p1_ships[ship_hit] -> hit += 1;
    checkSink( p1_ships[ship_hit], player);
  }
}

void fire(int player) {
  bool hit = false;
  // Check if that point being hit or miss before
  if (player == 1) {
    if ((p1_screen[p1_tarRow][p1_tarCol] & (1<<6)) || (p1_screen[p1_tarRow][p1_tarCol] & (1<<8)))
      return;
  }
  else {
    if ((p2_screen[p2_tarRow][p2_tarCol] & (1<<6)) || (p2_screen[p2_tarRow][p2_tarCol] & (1<<8)))
      return;
  }

  // Then do the normal
  if (player == 1) {
    if (p1_screen[p1_tarRow][p1_tarCol] & (1<<7))
      hit = true;
    if (hit)
    {
      hit_handler(player);
    }
    //miss mark blue, mark it miss in the screen
    else {
      p1_screen[p1_tarRow][p1_tarCol] &= ~0x7;
      p1_screen[p1_tarRow][p1_tarCol] |= 1<<8 | 1<<2;
    } 
      
    p1_turn = false;
    update_turn(2);
    p2_turn = true;
  }
  else {
    if (p2_screen[p2_tarRow][p2_tarCol] & (1<<7))
      hit = true;
    if (hit) {
      hit_handler(player);
    }
    //miss mark blue, mark it miss in the screen
    else {
      p2_screen[p2_tarRow][p2_tarCol] &= ~0x7;
      p2_screen[p2_tarRow][p2_tarCol] |= 1<<8 | 1<<2;
    } 
    p2_turn = false;
    update_turn(1);
    p1_turn = true;
  }
}

void copy_ships() {
  for (int row=1; row<15; row++) {
    for (int col=1; col<14; col++) {
      p1_screen[row][col] = p2_screen[row+16][col] & 0xFFF8;
      p2_screen[row][col] = p1_screen[row+16][col] & 0xFFF8;
    }
  }
}


void displayWinner() {
  // if (p1_win) {
  //   for (int row=0; row<32; row++) {
  //     for (int col=0; col<16; col++) {
  //       p1_screen[row][col] = 2;
  //       p2_screen[row][col] = 1;
  //     }
  //   }    
  // }
  // else {
  //   for (int row=0; row<32; row++) {
  //     for (int col=0; col<16; col++) {
  //       p1_screen[row][col] = 1;
  //       p2_screen[row][col] = 2;
  //     }
  //   }    
  // }
  for (int row=0; row<32; row++){
    for (int col=0; col<16; col++) {
      if (p1_win)
        p1_screen[row][col] = win_screen[row][col];
      else
        p2_screen[row][col] = win_screen[row][col];
    }
  }  
}


void TIM2_IRQHandler(void){
  TIM2->SR &= ~TIM_SR_UIF;
  //GPIOA->IDR = 0xf;
  int sample = 0;
  for(int x=0; x < sizeof voice / sizeof voice[0]; x++) {
    if (voice[x].in_use) {
        voice[x].offset += voice[x].step;
        if (voice[x].offset >= N<<16)
            voice[x].offset -= N<<16;
          sample += (wavetable[voice[x].offset>>16] * (voice[x].volume)) >> 4;
        }
    }
  sample = (sample >> 10) + 2048;
  if (sample > 4095)
    sample = 4095;
  else if (sample < 0)
    sample = 0;
  DAC->DHR12R1 = sample;
}

void init_dac(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_DACEN;
    DAC->CR |= DAC_CR_TEN1;
    DAC->CR &= ~(DAC_CR_TSEL1);
    DAC->CR |= DAC_CR_TSEL1_2;
    DAC->CR |= DAC_CR_EN1;
}

void init_tim2(void)
{
    // TODO: you fill this in.
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    TIM2->PSC = 1-1;
    TIM2->ARR = (48000000/RATE)-1;
  //  TIM6->ARR = 24000000;
    TIM2->DIER |= TIM_DIER_UIE;
    TIM2-> CR2 |= TIM_CR2_MMS_1;
    TIM2->CR1 |= TIM_CR1_CEN;
    NVIC->ISER[0] |= 1<<TIM2_IRQn;
    NVIC_SetPriority(TIM2_IRQn,0);
}


void note_off(int time, int chan, int key, int velo)
{
    int n;
    for(n=0; n<sizeof voice / sizeof voice[0]; n++) {
        if (voice[n].in_use && voice[n].note == key) {
            voice[n].in_use = 0; // disable it first...
            voice[n].chan = 0;   // ...then clear its values
            voice[n].note = key;
            voice[n].step = step[key];
            return;
        }
    }
}

// Find an unused voice, and use it to play a note.
void note_on(int time, int chan, int key, int velo)
{
    if (velo == 0) {
        note_off(time, chan, key, velo);
        return;
    }
    int n;
    for(n=0; n<sizeof voice / sizeof voice[0]; n++) {
        if (voice[n].in_use == 0) {
            voice[n].note = key;
            voice[n].step = step[key];
            voice[n].offset = 0;
            voice[n].volume = velo;
            voice[n].chan = chan;
            voice[n].in_use = 1;
            return;
        }
    }
}

void set_tempo(int time, int value, const MIDI_Header *hdr)
{
    // This assumes that the TIM2 prescaler divides by 48.
    // It sets the timer to produce an interrupt every N
    // microseconds, where N is the new tempo (value) divided by
    // the number of divisions per beat specified in the MIDI file header.
    TIM3->ARR = value/hdr->divisions - 1;
}




struct {
    int when;
    uint8_t note;
    uint8_t volume;
} events[] = {
        {480,84,0x73}, {556,84,0x00}, {960,84,0x74}, {1008,84,0x00},
        {1440,91,0x76}, {1520,91,0x00}, {1920,91,0x79}, {1996,91,0x00},
        {2400,93,0x76}, {2472,93,0x00}, {2640,94,0x67}, {2720,94,0x00},
        {2880,96,0x67}, {2960,96,0x00}, {3120,93,0x6d}, {3180,93,0x00},
        {3360,91,0x79}, {3440,91,0x00}, {4320,89,0x70}, {4408,89,0x00},
        {4800,89,0x73}, {4884,89,0x00}, {5280,88,0x73}, {5360,88,0x00},
        {5760,91,0x79}, {5836,91,0x00}, {6240,86,0x79}, {6308,86,0x00},
        {6720,86,0x76}, {6768,86,0x00}, {7200,84,0x76}, {7252,84,0x00},
        {8160,84,0x73}, {8236,84,0x00}, {8640,84,0x74}, {8688,84,0x00},
        {9120,91,0x76}, {9200,91,0x00}, {9600,91,0x79}, {9676,91,0x00},
        {10080,93,0x76}, {10152,93,0x00}, {10320,94,0x67}, {10400,94,0x00},
        {10560,96,0x67}, {10640,96,0x00}, {10800,93,0x6d}, {10860,93,0x00},
        {11040,91,0x79}, {11120,91,0x00}, {12000,86,0x76}, {12080,86,0x00},
        {12480,86,0x73}, {12552,86,0x00}, {13440,84,0x6d}, {13440,88,0x73},
        {13508,88,0x00}, {13512,84,0x00}, {13920,86,0x76}, {14004,86,0x00},
        {14400,86,0x76}, {14472,86,0x00}, {15152,81,0x3b}, {15184,83,0x44},
        {15188,81,0x00}, {15220,84,0x46}, {15228,83,0x00}, {15248,86,0x57},
        {15264,84,0x00}, {15284,88,0x5c}, {15292,86,0x00}, {15308,89,0x68},
        {15320,88,0x00}, {15336,91,0x6d}, {15344,89,0x00}, {15364,93,0x6d},
        {15368,91,0x00}, {15460,93,0x00},
};

int time = 0;
int n = 0;


uint8_t notes[] = { 60,62,64,65,67,69,71,72,71,69,67,65,64,62,60,0 };
uint8_t num = sizeof notes / sizeof notes[0] - 1;
const float pitch_array[] = {
0.943874, 0.945580, 0.947288, 0.948999, 0.950714, 0.952432, 0.954152, 0.955876,
0.957603, 0.959333, 0.961067, 0.962803, 0.964542, 0.966285, 0.968031, 0.969780,
0.971532, 0.973287, 0.975046, 0.976807, 0.978572, 0.980340, 0.982111, 0.983886,
0.985663, 0.987444, 0.989228, 0.991015, 0.992806, 0.994599, 0.996396, 0.998197,
1.000000, 1.001807, 1.003617, 1.005430, 1.007246, 1.009066, 1.010889, 1.012716,
1.014545, 1.016378, 1.018215, 1.020054, 1.021897, 1.023743, 1.025593, 1.027446,
1.029302, 1.031162, 1.033025, 1.034891, 1.036761, 1.038634, 1.040511, 1.042390,
1.044274, 1.046160, 1.048051, 1.049944, 1.051841, 1.053741, 1.055645, 1.057552,
};

void pitch_wheel_change(int time, int chan, int value)
{
    //float multiplier = pow(STEP1, (value - 8192.0) / 8192.0);
    float multiplier = pitch_array[value >> 8];
    for(int n=0; n<sizeof voice / sizeof voice[0]; n++) {
        if (voice[n].in_use && voice[n].chan == chan) {
            voice[n].step = step[voice[n].note] * multiplier;
        }
    }
}

void TIM3_IRQHandler(void)
{
    // TODO: Remember to acknowledge the interrupt here!
    TIM3->SR &= ~TIM_SR_UIF;
    // Look at the next item in the event array and check if it is
    // time to play that note.
    while(events[n].when == time) {
        // If the volume is 0, that means turn the note off.
        note_on(0,0,events[n].note, events[n].volume);
        n++;
    }

    // Increment the time by one tick.
    time += 1;

    // When we reach the end of the event array, start over.
    if ( n >= sizeof events / sizeof events[0]) {
        n = 0;
        time = 0;
    }
}

void init_tim3(int n) {
    // TODO: you fill this in.
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
    TIM3->PSC = 48-1;
    TIM3->ARR = n-1;
    TIM3->DIER |= TIM_DIER_UIE;
    TIM3->CR1 |= TIM_CR1_CEN;
    NVIC->ISER[0] |= 1<<TIM3_IRQn;
    TIM3->CR1 |= TIM_CR1_ARPE;
    NVIC_SetPriority(TIM3_IRQn,1);
}

