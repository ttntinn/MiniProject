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
char get_key_event(void); // wait for a button event (press or release)
char get_keypress(void);  // wait for only a button press event.
float getfloat(void);     // read a floating-point number from keypad
void show_keys(void);     // demonstrate get_key_event()

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
        0x200+'P', 0x200+'1', 0x200+' ', 0x200+'S', 0x200+'h', + 0x200+'i', 0x200+'p', 0x200+' ',
        0x200+'C', 0x200+'o', 0x200+'u', 0x200+'n', + 0x200+'t', 0x200+':', 0x200+' ', 0x200+'5',
        0x0c0, // Command to set the cursor at the first position line 2
        0x200+'P', 0x200+'2', 0x200+' ', 0x200+'S', 0x200+'h', + 0x200+'i', 0x200+'p', 0x200+' ',
        0x200+'C', 0x200+'o', 0x200+'u', 0x200+'n', + 0x200+'t', 0x200+':', 0x200+' ', 0x200+'5',
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

    // Outputs (PC4-PC7)
    GPIOC->MODER &= ~0xff00;
    GPIOC->MODER |= 0x5500;

    // Output type open-drain
    GPIOC->OTYPER &= ~0xf0;
    GPIOC->OTYPER |= 0xf0;

    // Inputs (PC0-PC3)
    GPIOC->MODER &= ~0xff;

    // Inputs pulled high
    GPIOC->PUPDR &= ~0xff;
    GPIOC->PUPDR |= 0x55;

    //===========================================================================
    // Keypad 2
    //===========================================================================
    // Enable port B
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;

    // Outputs (PC4-PC7)
    GPIOB->MODER &= ~0xff00;
    GPIOB->MODER |= 0x5500;

    // Output type open-drain
    GPIOB->OTYPER &= ~0xf0;
    GPIOB->OTYPER |= 0xf0;

    // Inputs (PC0-PC3)
    GPIOB->MODER &= ~0xff;

    // Inputs pulled high
    GPIOB->PUPDR &= ~0xff;
    GPIOB->PUPDR |= 0x55;
}

void TIM7_IRQHandler()
{
    TIM7->SR &= ~TIM_SR_UIF;
    // Keypad 1
    int rows = read_rows();
    update_history(col, rows);
    col = (col + 1) & 3;
    drive_column(col);

    /*// Keypad 2
    int rows2 = read_rows2();
    update_history(col2, rows2);
    col2 = (col2 + 1) & 3;
    drive_column2(col2); */
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
    int x = 5;
    char pad[16] = {'0','1', '2', '3', '4', '5', '6','7','8','9','A','B','C','D','*','#'}; //
    for(;;) {
        char key = get_keypress();
        for (int y = 0; y <= 16; y++)
        {
            if(key == pad[y])
            {
                if (x > 0)
                    display[16] = numbers[x--];
                else if (x == 0)
                    display[16] = numbers[0];
            }
        }
    }

}
