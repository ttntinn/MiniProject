#include "stm32f0xx.h"

//===========================================================================
// Function Declarations
//===========================================================================
void nano_wait(unsigned int);
void drive_column(int);   // energize one of the column outputs
void drive_column2(int);
int  read_rows();         // read the four row inputs
int read_rows2();
void update_history(int col, int rows); // record the buttons of the driven column
void update_history2(int col, int rows); // record the buttons of the driven column
char get_keypress(void);  // wait for only a button press event.
void instruction(void);
void controls(void);
void game(uint16_t, uint16_t);
void shipcount(void);
void spi_cmd(unsigned int);
void users_inputs(void);
void error(void);
void shipcount(void);
void enter_number(void);
void press_hash(void);
void number_disp(uint16_t, uint16_t);


//===========================================================================
// Global Variables
//===========================================================================
uint16_t msg[8] = { 0x0000,0x0100,0x0200,0x0300,0x0400,0x0500,0x0600,0x0700 };
uint8_t col; // the column being scanned
uint8_t col2;
uint16_t numbers[10] = { 0x200+'0',0x200+'1',0x200+'2',0x200+'3',0x200+'4',0x200+'5',0x200+'6',0x200+'7',0x200+'8',0x200+'9'};
char keypad1_numbers[10] = {'0' | 0x80, '1' | 0x80, '2' | 0x80, '3' | 0x80, '4' | 0x80, '5' | 0x80, '6' | 0x80, '7' | 0x80, '8' | 0x80, '9' | 0x80};
char keypad2_numbers[10] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};
char keypad1_char[6] = {'*' | 0x80, '#' | 0x80, 'D' | 0x80, 'C' | 0x80, 'B' | 0x80, 'A' | 0x80};
char keypad2_char[6] = {'*', '#', 'D', 'C', 'B', 'A'};
// Displays String on LCD
uint16_t display[34] = {
        0x002, // Command to set the cursor at the first position line 1
        0x200+'C', 0x200+'o', 0x200+'n', 0x200+'t', 0x200+'r', + 0x200+'o', 0x200+'l', 0x200+':',
        0x200+' ', 0x200+'P', 0x200+'r', 0x200+'e', + 0x200+'s', 0x200+'s', 0x200+' ', 0x200+'5',
        0x0c0, // Command to set the cursor at the first position line 2
        0x200+'B', 0x200+'e', 0x200+'g', 0x200+'i', 0x200+'n', + 0x200+':', 0x200+' ', 0x200+' ',
        0x200+' ', 0x200+'P', 0x200+'r', 0x200+'e', + 0x200+'s', 0x200+'s', 0x200+' ', 0x200+'2',
};

//===========================================================================
// Functions
//===========================================================================
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
    // PA5  SPI1_SCK
    // PA6  SPI1_MISO
    // PA7  SPI1_MOSI
    // PA15 SPI1_NSS

    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    GPIOA->MODER &= ~0xc000fc00;
    GPIOA->MODER |= 0x8000a800;

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

// Keypad configuration
void enable_ports(void)
{
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

void controls(void)
{
    char key = get_keypress();
    while (key != '5' && key != ('5' | 0x80) && key != '2' && key != ('2' | 0x80)){
        key = get_keypress();
    }
    if (key == ('5' | 0x80) || key == '5'){
        display[1] = 0x200+'U';
        display[2] = 0x200+'p';
        display[3] = 0x200+'=';
        display[4] = 0x200+'2';
        display[5] = 0x200+' ';
        display[6] = 0x200+'D';
        display[7] = 0x200+'o';
        display[8] = 0x200+'w';
        display[9] = 0x200+'n';
        display[10] = 0x200+'=';
        display[11] = 0x200+'8';
        display[12] = 0x200+' ';
        display[13] = 0x200+' ';
        display[14] = 0x200+' ';
        display[15] = 0x200+' ';
        display[16] = 0x200+' ';
        display[18] = 0x200+'L';
        display[19] = 0x200+'e';
        display[20] = 0x200+'f';
        display[21] = 0x200+'t';
        display[22] = 0x200+'=';
        display[23] = 0x200+'4';
        display[24] = 0x200+' ';
        display[25] = 0x200+'N';
        display[26] = 0x200+'e';
        display[27] = 0x200+'x';
        display[28] = 0x200+'t';
        display[29] = 0x200+'=';
        display[30] = 0x200+'5';
        display[31] = 0x200+' ';
        display[32] = 0x200+' ';
        display[33] = 0x200+' ';
        char cont = get_keypress();
        while (cont != '5' && cont != ('5' | 0x80))
            cont = get_keypress();
        if (cont == '5' || cont == ('5' | 0x80))
            instruction();
    }
    else if (key == '2' || key == ('2' | 0x80))
        shipcount();
}

void instruction(void)
{
        display[1] = 0x200+'R';
        display[2] = 0x200+'i';
        display[3] = 0x200+'g';
        display[4] = 0x200+'h';
        display[5] = 0x200+'t';
        display[6] = 0x200+'=';
        display[7] = 0x200+'6';
        display[8] = 0x200+' ';
        display[9] = 0x200+'E';
        display[10] = 0x200+'n';
        display[11] = 0x200+'t';
        display[12] = 0x200+'e';
        display[13] = 0x200+'r';
        display[14] = 0x200+'=';
        display[15] = 0x200+'5';
        display[16] = 0x200+' ';
        display[18] = 0x200+'P';
        display[19] = 0x200+'r';
        display[20] = 0x200+'e';
        display[21] = 0x200+'s';
        display[22] = 0x200+'s';
        display[23] = 0x200+' ';
        display[24] = 0x200+'5';
        display[25] = 0x200+' ';
        display[26] = 0x200+'t';
        display[27] = 0x200+'o';
        display[28] = 0x200+' ';
        display[29] = 0x200+'B';
        display[30] = 0x200+'e';
        display[31] = 0x200+'g';
        display[32] = 0x200+'i';
        display[33] = 0x200+'n';
    char begin = get_keypress();
    while (begin != '5' && begin != ('5' | 0x80))
        begin = get_keypress();
    if (begin == '5' || begin == ('5' | 0x80))
        shipcount();
}

void error(void)
{
    display[1] = 0x200+'E';
    display[2] = 0x200+'r';
    display[3] = 0x200+'r';
    display[4] = 0x200+'o';
    display[5] = 0x200+'r';
    display[6] = 0x200+'!';
    display[7] = 0x200+' ';
    display[8] = 0x200+'E';
    display[9] = 0x200+'n';
    display[10] = 0x200+'t';
    display[11] = 0x200+'e';
    display[12] = 0x200+'r';
    display[13] = 0x200+' ';
    display[14] = 0x200+'0';
    display[15] = 0x200+'-';
    display[16] = 0x200+'9';
    display[18] = 0x200+'o';
    display[19] = 0x200+'n';
    display[20] = 0x200+' ';
    display[21] = 0x200+'t';
    display[22] = 0x200+'h';
    display[23] = 0x200+'e';
    display[24] = 0x200+' ';
    display[25] = 0x200+'K';
    display[26] = 0x200+'e';
    display[27] = 0x200+'y';
    display[28] = 0x200+'p';
    display[29] = 0x200+'a';
    display[30] = 0x200+'d';
    display[31] = 0x200+' ';
    display[32] = 0x200+' ';
    display[33] = 0x200+' ';
    enter_number();
}

void shipcount(void)
{
    display[1] = 0x200+'H';
    display[2] = 0x200+'o';
    display[3] = 0x200+'w';
    display[4] = 0x200+' ';
    display[5] = 0x200+'M';
    display[6] = 0x200+'a';
    display[7] = 0x200+'n';
    display[8] = 0x200+'y';
    display[9] = 0x200+' ';
    display[10] = 0x200+'S';
    display[11] = 0x200+'h';
    display[12] = 0x200+'i';
    display[13] = 0x200+'p';
    display[14] = 0x200+'s';
    display[15] = 0x200+'?';
    display[16] = 0x200+' ';
    display[18] = 0x200+'P';
    display[19] = 0x200+'r';
    display[20] = 0x200+'e';
    display[21] = 0x200+'s';
    display[22] = 0x200+'s';
    display[23] = 0x200+' ';
    display[24] = 0x200+'#';
    display[25] = 0x200+' ';
    display[26] = 0x200+'t';
    display[27] = 0x200+'o';
    display[28] = 0x200+' ';
    display[29] = 0x200+'E';
    display[30] = 0x200+'n';
    display[31] = 0x200+'t';
    display[32] = 0x200+'e';
    display[33] = 0x200+'r';
    enter_number();
}

void enter_number(void)
{
    uint16_t ship_num1, ship_num2;
    char keypad1_miss[6] = {'*' | 0x80, 'D' | 0x80, 'C' | 0x80, 'B' | 0x80, 'A' | 0x80};
    char keypad2_miss[6] = {'*', 'D', 'C', 'B', 'A'};
    // first digit
    char first = get_keypress();
    for (int k = 0; k <= 6; k++)
    {
        if (first == keypad1_char[k] || first == keypad2_char[k]){
            error();
            return;
        }
    }
    for (int i = 0; i <= 10; i++)
    {
        if (first == keypad1_numbers[i] || first == keypad2_numbers[i]){
            ship_num1 = numbers[i];
            break;
        }
    }
    ship_num2 = 0x200+' ';
    number_disp(ship_num1, ship_num2);

    // Second digit
    char second = get_keypress();
    for (int y = 0; y <= 6; y++)
    {
        if (second == keypad1_miss[y] || second == keypad2_miss[y]){
            error();
            return;
        }
    }
    if (second == ('#' | 0x80) || second == '#')
    {
        ship_num2 = 0x200+' ';
        number_disp(ship_num1, ship_num2);
        game(ship_num1, ship_num2);
    }
    else
    {
        for (int j = 0; j <= 10; j++)
        {
            if (second == keypad1_numbers[j] || second == keypad2_numbers[j])
                ship_num2 = numbers[j];
        }
        number_disp(ship_num1, ship_num2);
        char enter = get_keypress();
        while (enter != keypad1_char[1] && enter != keypad2_char[1])
        {
            press_hash();
            enter = get_keypress();
        }
        if (enter == ('#' | 0x80) || enter == '#')
            game(ship_num2, ship_num1);
    }
}

void press_hash(void)
{
    display[1] = 0x200+'P';
    display[2] = 0x200+'l';
    display[3] = 0x200+'e';
    display[4] = 0x200+'a';
    display[5] = 0x200+'s';
    display[6] = 0x200+'e';
    display[7] = 0x200+' ';
    display[8] = 0x200+'P';
    display[9] = 0x200+'r';
    display[10] = 0x200+'e';
    display[11] = 0x200+'s';
    display[12] = 0x200+'s';
    display[13] = 0x200+' ';
    display[14] = 0x200+'#';
    display[15] = 0x200+' ';
    display[16] = 0x200+' ';
    display[18] = 0x200+'-';
    display[19] = 0x200+'-';
    display[20] = 0x200+'-';
    display[21] = 0x200+'-';
    display[22] = 0x200+'-';
    display[23] = 0x200+'-';
    display[24] = 0x200+'>';
    display[25] = 0x200+'#';
    display[26] = 0x200+'<';
    display[27] = 0x200+'-';
    display[28] = 0x200+'-';
    display[29] = 0x200+'-';
    display[30] = 0x200+'-';
    display[31] = 0x200+'-';
    display[32] = 0x200+'-';
    display[33] = 0x200+'-';
}

void number_disp(uint16_t first, uint16_t second)
{
    display[1] = 0x200+'E';
    display[2] = 0x200+'n';
    display[3] = 0x200+'t';
    display[4] = 0x200+'e';
    display[5] = 0x200+'r';
    display[6] = 0x200+'i';
    display[7] = 0x200+'n';
    display[8] = 0x200+'g';
    display[9] = 0x200+':';
    display[10] = 0x200+' ';
    display[11] = first;
    display[12] = second;
    display[13] = 0x200+' ';
    display[14] = 0x200+' ';
    display[15] = 0x200+' ';
    display[16] = 0x200+' ';
    display[18] = 0x200+'P';
    display[19] = 0x200+'r';
    display[20] = 0x200+'e';
    display[21] = 0x200+'s';
    display[22] = 0x200+'s';
    display[23] = 0x200+' ';
    display[24] = 0x200+'#';
    display[25] = 0x200+' ';
    display[26] = 0x200+'t';
    display[27] = 0x200+'o';
    display[28] = 0x200+' ';
    display[29] = 0x200+'E';
    display[30] = 0x200+'n';
    display[31] = 0x200+'t';
    display[32] = 0x200+'e';
    display[33] = 0x200+'r';
}

void game(uint16_t first, uint16_t second)
{
    display[1] = 0x200+'P';
    display[2] = 0x200+'1';
    display[3] = 0x200+' ';
    display[4] = 0x200+'S';
    display[5] = 0x200+'h';
    display[6] = 0x200+'i';
    display[7] = 0x200+'p';
    display[8] = 0x200+' ';
    display[9] = 0x200+'C';
    display[10] = 0x200+'o';
    display[11] = 0x200+'u';
    display[12] = 0x200+'n';
    display[13] = 0x200+'t';
    display[14] = 0x200+':';
    display[15] = second;
    display[16] = first;
    display[18] = 0x200+'P';
    display[19] = 0x200+'2';
    display[20] = 0x200+' ';
    display[21] = 0x200+'S';
    display[22] = 0x200+'h';
    display[23] = 0x200+'i';
    display[24] = 0x200+'p';
    display[25] = 0x200+' ';
    display[26] = 0x200+'C';
    display[27] = 0x200+'o';
    display[28] = 0x200+'u';
    display[29] = 0x200+'n';
    display[30] = 0x200+'t';
    display[31] = 0x200+':';
    display[32] = second;
    display[33] = first;
}

void users_inputs(void)
{
    int x, y, j, k;
    int size = (sizeof(numbers)/sizeof(numbers[0])-1);
    for (int i=0; i<=size; i++)
    {
        if (display[16] == numbers[i])
            x = i;
        if (display[15] == numbers[i])
            j = i;
        else if (display[15] == 0x200+' ')
            j = 0;
        if (display[33] == numbers[i])
            y = i;
        if (display[32] == numbers[i])
            k = i;
        else if (display[32] == 0x200+' ')
            k = 0;
    }
    for(;;)
    {
        char key = get_keypress();
        while (key != ('2' | 0x80) && key != '2' &&
                key != ('4' | 0x80) && key != '4' &&
                key != ('5' | 0x80) && key != '5' &&
                key != ('6' | 0x80) && key != '6' &&
                key != ('8' | 0x80) && key != '8'){
            key = get_keypress();
        }
        //===========================================================================
        // Player 1 Controls
        //===========================================================================
        // Incrementing P1 count
        if (key == ('2' | 0x80) || key == ('6' | 0x80)){
            if (x >= size){
                if (j == size && x == size){
                    display[16] = numbers[size];
                    display[15] = numbers[size];
                    x = 8;
                }
                else{
                    display[15] = numbers[++j];
                    x = -1;
                }
            }
            if (key == ('6' | 0x80)){
                if (x == 8 || x == 9){
                    if (j == 9){
                        x = 7;
                        display[15] = numbers[size];
                    }
                    else if (x == 8){
                        x = -2;
                        display[15] = numbers[++j];
                    }
                }
            }
        }
        // If decrementing key
        if (key == ('8' | 0x80) || key == ('4' | 0x80)){
            // Condition when P1 count = 10
            if (x == 0 && j == 1){
                --j;
                display[15] = 0x200+' ';
                x = size + 1;
            }
            // Condition when P1 count is tens (except 10) 20, 30, 40, 50...
            else if (x == 0 && j > 1){
                display[15] = numbers[--j];
                x = size + 1;
            }
            // Condition when P1 count is 0
            else if (x == 0 && j == 0)
                x = 1;
            if (key == ('4' | 0x80))
            {
                if (x == 1 && j == 1)
                {
                    --j;
                    display[15] = 0x200+' ';
                    x = 11;
                }
                else if (x == 0 && j == 0)
                {
                    display[15] = 0x200+' ';
                    x = 2;
                }
                else if (x==1 && j == 0)
                {
                    display[15] = 0x200+' ';
                    x = 2;
                }
                else if (x==1 && j>1)
                {
                    display[15] = numbers[--j];
                    x = 11;
                }
            }
        }
        //===========================================================================
        // Player 2 Controls
        //===========================================================================
        // Incrementing P2 count
        if (key == '2' || key == '6'){
            if (y >= size){
                if (k == size && y == size){
                    display[33] = numbers[size];
                    display[32] = numbers[size];
                    y = 8;
                }
                else{
                    display[32] = numbers[++k];
                    y = -1;
                }
            }
            if (key == '6'){
                if (y == 8 || y == 9){
                    if (k == 9){
                        y = 7;
                        display[32] = numbers[size];
                    }
                    else if (y == 8){
                        y = -2;
                        display[32] = numbers[++k];
                    }
                }
            }
        }
        // If decrementing key
        if (key == '8' || key == '4'){
            // Condition when P1 count = 10
            if (y == 0 && k == 1){
                --k;
                display[32] = 0x200+' ';
                y = size + 1;
            }
            // Condition when P1 count is tens (except 10) 20, 30, 40, 50...
            else if (y == 0 && k > 1){
                display[32] = numbers[--k];
                y = size + 1;
            }
            // Condition when P1 count is 0
            else if (y == 0 && k == 0)
                y = 1;
            if (key == '4')
            {
                if (y == 1 && k == 1){
                    --k;
                    display[32] = 0x200+' ';
                    y = 11;
                }
                else if (y == 0 && k == 0){
                    display[32] = 0x200+' ';
                    y = 2;
                }
                else if (y==1 && k==0){
                    display[32] = 0x200+' ';
                    y = 2;
                }
                else if (y==1 && k>1){
                    display[32] = numbers[--k];
                    y = 11;
                }
            }
        }
        //===========================================================================
        // Ship Count Calculations
        //===========================================================================
        if (key == '2' )
            display[33] = numbers[++y];
        else if (key == ('2' | 0x80))
            display[16] = numbers[++x];

        else if (key == '8')
            display[33] = numbers[--y];
        else if (key == ('8' | 0x80))
            display[16] = numbers[--x];

        else if (key == '4'){
            --y;
            display[33] = numbers[--y];
        }
        else if (key == ('4' | 0x80)){
            --x;
            display[16] = numbers[--x];
        }
        else if (key == '6'){
            ++y;
            display[33] = numbers[++y];
        }
        else if (key == ('6' | 0x80)){
            ++x;
            display[16] = numbers[++x];
        }
        else if (key == '5'){
            y = 5;
            k = 0;
            display[33] = numbers[y];
            display[32] = 0x200+' ';
        }
        else if (key == ('5' | 0x80)){
            x = 5;
            j = 0;
            display[16] = numbers[x];
            display[15] = 0x200+' ';
        }
    }
}

int main(void)
{
    enable_ports();

    // OLED LCD Call
    init_spi1();
    spi1_init_oled();
    spi1_setup_dma();
    spi1_enable_dma();

    // Keypad 1 & 2
    init_tim7();
    controls();

    users_inputs();
}
