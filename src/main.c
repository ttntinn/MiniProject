#include "stm32f0xx.h"
#include "ship.h"
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
Ship p1_ship0 = {.len=5, .row=31, .col=0, .rotate='v', .alive=true};
Ship p1_ship1 = {.len=4, .row=31, .col=0, .rotate='v', .alive=true};
Ship p1_ship2 = {.len=3, .row=31, .col=0, .rotate='v', .alive=true};
Ship p1_ship3 = {.len=3, .row=31, .col=0, .rotate='v', .alive=true};
Ship p1_ship4 = {.len=3, .row=31, .col=0, .rotate='v', .alive=true};
Ship* p1_ships[5] = {&p1_ship0, &p1_ship1, &p1_ship2, &p1_ship3, &p1_ship4}; //Array of p1_ships

Ship p2_ship0 = {.len=5, .row=31, .col=0, .rotate='v', .alive=true};
Ship p2_ship1 = {.len=4, .row=31, .col=0, .rotate='v', .alive=true};
Ship p2_ship2 = {.len=3, .row=31, .col=0, .rotate='v', .alive=true};
Ship p2_ship3 = {.len=3, .row=31, .col=0, .rotate='v', .alive=true};
Ship p2_ship4 = {.len=3, .row=31, .col=0, .rotate='v', .alive=true};
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

bool sink = false;
bool hit  = false;
bool miss = false;

//Game Screen Player 1
/*
Each point is 8 bit
point[2:0] RGB
point[4:3] ship num
point[5]   miss
point[6]   hit
point[7]   place 

*/
uint8_t p1_screen [32][16] = {
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,7,7,7,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,7,7,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,7,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,7,7,0,0,0,0,0,0,0,0,0,0,0,0},
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
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {7,1,1,1,1,1,1,1,2,2,7,7,7,7,7,7},
};

//Game Screen Player 2
uint8_t p2_screen [32][16] = {
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,7,7,7,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,7,7,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,7,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,7,7,0,0,0,0,0,0,0,0,0,0,0,0},
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
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {7,1,1,1,1,1,1,1,2,2,7,7,7,7,7,7},
};
  /*{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,7,7,7,7,7,7,7,7,7,7,7,0,0},
  {0,0,7,7,7,7,7,7,7,7,7,7,7,7,0,0},
  {0,7,7,7,0,0,0,0,0,0,0,0,7,0,0,0},
  {0,7,7,7,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,7,7,7,7,7,7,7,7,7,7,7,7,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,7,7,7,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,7,7,7,0},
  {0,7,7,7,7,7,7,7,7,7,7,7,7,7,0,0},
  {0,7,7,7,7,7,7,7,7,7,7,7,7,7,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,7,7,7,7,7,7,7,7,7,7,7,0,0},
  {0,0,7,7,7,7,7,7,7,7,7,7,7,7,0,0},
  {0,7,7,7,0,0,0,0,0,0,0,0,7,0,0,0},
  {0,7,7,7,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,7,7,7,7,7,7,7,7,7,7,7,7,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,7,7,7,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,7,7,7,0},
  {0,7,7,7,7,7,7,7,7,7,7,7,7,7,0,0},
  {0,7,7,7,7,7,7,7,7,7,7,7,7,7,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
};*/

// uint8_t *** p_screen = malloc(sizeof(uint8_t**) * 2);
// p_screen[0] = p1_screen;
// p_screen[1] = p2_screen;


int p1_tarRow = 14;
int p1_tarCol = 0;
int p2_tarRow = 14;
int p2_tarCol = 0;
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
    else if (p1_screen[p1_tarRow][p1_tarCol] & (1<<5)) { //missed already
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
    else if (p2_screen[p2_tarRow][p2_tarCol] & (1<<5)) { //missed already
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
    if (moveVal == 2) p1_tarRow--;
    if (moveVal == 8) p1_tarRow++;
    if (moveVal == 4) p1_tarCol--;
    if (moveVal == 6) p1_tarCol++;
    displayPoint(player);
  }
  else {
    deletePoint(player);
    if (moveVal == 2) p2_tarRow--;
    if (moveVal == 8) p2_tarRow++;
    if (moveVal == 4) p2_tarCol--;
    if (moveVal == 6) p2_tarCol++;
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
     p1_screen[16][i] = 1;
     p1_screen[15][i] = 1;
     p2_screen[16][i] = 1;
     p2_screen[15][i] = 1;
   }
}

void update_turn(int player){
 if (player == 1) {
   for (int i = 0; i < 16; i++) {
     p1_screen[16][i] = 2;
     p1_screen[15][i] = 2;
     p2_screen[16][i] = 1;
     p2_screen[15][i] = 1;
   }
 }
 else {
   for( int i = 0; i < 16; i++) {
     p2_screen[16][i] = 2;
     p2_screen[15][i] = 2;
     p1_screen[16][i] = 1;
     p1_screen[15][i] = 1;
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

//the player specified is the one that fires
// void fire(int player){
//     if (player == 1){ //player 1 fires
//         if (p2_screen[targetRow][targetCol] == 7){
//             p1_screen[targetRow][targetCol] = 0;
//             p2_screen[targetRow][targetCol] = 0;
//             p1_screen[targetRow][targetCol] = 1;
//             p2_screen[targetRow][targetCol] = 1;
//         }
//     }
//     else { //player 2 fires
//         if (p1_screen[targetRow][targetCol] == 7){
//             p1_screen[targetRow][targetCol] = 0;
//             p2_screen[targetRow][targetCol] = 0;
//             p1_screen[targetRow][targetCol] = 1;
//             p2_screen[targetRow][targetCol] = 1;
//         }
//     }
// }

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
          p1_screen[ship->row - i][ship->col] |= (1<<7 | (p1_shipnum<<3 & 0x10));
        else
          p1_screen[ship->row - i][ship->col] |= 0x7;
      }
    }
    else {
      for (int i =0; i<ship->len; i++) {
        if (place)
          p1_screen[ship->row][ship->col+i] |= (1<<7 | (p1_shipnum<<3 & 0x10)) ;
        else
          p1_screen[ship->row][ship->col+i] |= 0x7;
      }
    }
  }
  else {
    if (ship->rotate == 'v'){
      for (int i = 0; i < ship->len; i++){
        if (place) 
          p2_screen[ship->row - i][ship->col] |= (1<<7 | (p2_shipnum<<3 & 0x10));
        else
          p2_screen[ship->row - i][ship->col] |= 0x7;
      }
    }
    else {
      for (int i =0; i<ship->len; i++) {
        if (place)
          p2_screen[ship->row][ship->col+i] |= (1<<7 | (p2_shipnum<<3 & 0x10)) ;
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
      if (ship->row < 32){
        ship->row += 1;
      }
    }
    else if (moveVal == 4){
      ship->col -= 1;
    }
    else {
      ship->col += 1;
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
      if (ship->row > 31){
        ship->row += 1;
      }
    }
    else if (moveVal == 4){
      ship->col -= 1;
    }
    else {
      ship->col += 1;
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
  int player1_ships = p1_shipnum;
  int player2_ships = p2_shipnum;
  player1_ships++;
  char charreplace;
  char start [] = "Press 5 to Start";
  char instruct1 [] = "2=U 8=D 4=L 6=Ri#=Rotate *=Place";
  char game [] = "P1: x P2: x     Press * to FIRE!";
  char endGame [] = "Player x wins!  PlayAgain->Reset";
  itoa(player1_ships, &charreplace, 10);
  game[4] = charreplace;
  itoa(player2_ships, &charreplace, 10);
  game[10] = charreplace;
  if (game_mode == 's')
    display_string(start);
  else if (game_mode == 'p')
    display_string(instruct1);
  else if (game_mode == 'g')
    display_string(game);
  else if (game_mode == 'e'){
    if (p1_shipnum == 0)
      endGame[8] = "2";
    else if (p2_shipnum == 0)
      endGame[8] = "1";
    display_string(endGame);
  }
}

void checkSink(Ship * ship, int player) {
  if (ship->hit == ship->len) {
    sink = true;
    ship->alive = false;
    if (sink && player == 1) {
      p2_shipnum--;
    }
    else if (sink && player ==2) {
      p1_shipnum--;
    }
  }
}

void hit_handler(player) {
  hit = true;
  //mark hit, turn white, check for which ship is hit, then check sink
  if (player == 1) {
    p1_screen[p1_tarRow][p1_tarCol] |= 1<<6 | 0x7;
    unsigned int ship_hit = (p1_screen[p1_tarRow][p1_tarCol] & 0x18) >> 3;
    p2_ships[ship_hit] -> hit += 1;
    checkSink( p2_ships[ship_hit], player);
  }
  else {
    p2_screen[p2_tarRow][p2_tarCol] |= 1<<6 | 0x7;
    unsigned int ship_hit = (p2_screen[p2_tarRow][p2_tarCol] & 0x18) >> 3;
    p1_ships[ship_hit] -> hit += 1;
    checkSink( p1_ships[ship_hit], player);
  }
}

void fire(int player) {
  bool hit;
  // Check if that point being hit or miss before
  if (player == 1) {
    if ((p1_screen[p1_tarRow][p1_tarCol] & (1<<6)) || (p1_screen[p1_tarRow][p1_tarCol] & (1<<5)))
      return;
  }
  else {
    if ((p2_screen[p2_tarRow][p2_tarCol] & (1<<6)) || (p2_screen[p2_tarRow][p2_tarCol] & (1<<5)))
      return;
  }

  // Then do the normal
  if (player == 1) {
    if (p1_screen[p1_tarRow][p1_tarCol] & (1<<7))
      hit = true;
    if (hit)
      hit_handler(player);
    //miss mark blue, mark it miss in the screen
    else {
      p1_screen[p1_tarRow][p1_tarCol] &= ~0x7;
      p1_screen[p1_tarRow][p1_tarCol] |= 1<<5 | 1<<2;
    } 
      
    p1_turn = false;
    update_turn(2);
    p2_turn = true;
  }
  else {
    if (p2_screen[p2_tarRow][p2_tarCol] & (1<<7))
      hit = true;
    if (hit)
      hit_handler(player);
    //miss mark blue, mark it miss in the screen
    else {
      p2_screen[p2_tarRow][p2_tarCol] &= ~0x7;
      p2_screen[p2_tarRow][p2_tarCol] |= 1<<5 | 1<<2;
    } 
    p2_turn = false;
    update_turn(1);
    p1_turn = true;
  }
}

void copy_ships() {
  for (int row=0; row<15; row++) {
    for (int col=0; col<15; col++) {
      p1_screen[row][col] = p2_screen[row+17][col];
      p2_screen[row][col] = p1_screen[row+17][col];
    }
  }
}


void displayWinner() {
  if (p1_win) {
    for (int row=0; row<32; row++) {
      for (int col=0; col<15; col++) {
        p1_screen[row][col] = 2;
        p2_screen[row][col] = 1;
      }
    }    
  }
  else {
    for (int row=0; row<32; row++) {
      for (int col=0; col<15; col++) {
        p1_screen[row][col] = 1;
        p2_screen[row][col] = 2;
      }
    }    
  }
}