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
void shipcount(void);
void spi_cmd(unsigned int);

//===========================================================================
// Global Variables
//===========================================================================
uint16_t msg[8] = { 0x0000,0x0100,0x0200,0x0300,0x0400,0x0500,0x0600,0x0700 };
uint8_t col; // the column being scanned
uint8_t col2;
uint16_t numbers[5] = { 0x200+'0',0x200+'1',0x200+'2',0x200+'3',0x200+'4'};
// Displays String on LCD
uint16_t display[34] = {
        0x002, // Command to set the cursor at the first position line 1
        0x200+'C', 0x200+'o', 0x200+' n', 0x200+'t', 0x200+'r', + 0x200+'o', 0x200+'l', 0x200+':',
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
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;//1<<0;
    DMA1_Channel3->CCR &= ~DMA_CCR_EN;// ~(1<<0); // Disable Channel
    DMA1_Channel3->CPAR = &(SPI1->DR);//&(GPIOB->ODR); // Set CPAR to the address of the GPIOB_ODR register.
    DMA1_Channel3->CMAR = (uint32_t)display; // Set CMAR to the display array base address
    DMA1_Channel3->CNDTR = 34; // Set CNDTR to 16
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
    //GPIOA->ODR &= ~(0)
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
    // Key Pad 1
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

    //===========================================================================
    // Keypad 2
    //===========================================================================
    /*//Enable port B
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;

    // Outputs (PB4-PB7)
    GPIOB->MODER &= ~0xff00;
    GPIOB->MODER |= 0x5500;

    // Output type open-drain
    GPIOB->OTYPER &= ~0xf0;
    GPIOB->OTYPER |= 0xf0;

    // Inputs (PB0-PB3)
    GPIOB->MODER &= ~0xff;

    // Inputs pulled high
    GPIOB->PUPDR &= ~0xff;
    GPIOB->PUPDR |= 0x55;

    //Enable port C
    RCC->AHBENR |= RCC_AHBENR_GPIOCEN;

    // Outputs (PC4-PC7)
    GPIOC->MODER&= ~0xff00;
    GPIOC->MODER |= 0x5500;

    // Output type open-drain
    GPIOC->OTYPER &= ~0xf0;
    GPIOC->OTYPER |= 0xf0;

    // Inputs (PC0-PC3)
    GPIOC->MODER &= ~0xff;

    // Inputs pulled high
    GPIOC->PUPDR &= ~0xff;
    GPIOC->PUPDR |= 0x55;*/
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
    TIM7->PSC = 480-1;
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

void shipcount(void)
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
    display[15] = 0x200+' ';
    display[16] = 0x200+'5';
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
    display[32] = 0x200+' ';
    display[33] = 0x200+'5';
    int x = 5;
    int y = 5;
    for(;;) {
        char key = get_keypress();
        if (key & 0x80)
        {
                if (x > 0)
                    display[16] = numbers[--x];
                else if (x == 0)
                    display[16] = numbers[0];
        }
        else
        {
            if (y>0)
                display[33] = numbers[--y];
            else if (y==0)
                display[33] = numbers[0];
        }
    }
}

int main(void)
{
    // OLED LCD Call
    init_spi1();
    spi1_init_oled();
    spi1_setup_dma();
    spi1_enable_dma();

    // Keypad 1 (PC Inputs)
    enable_ports();
    init_tim7();
    controls();
}
