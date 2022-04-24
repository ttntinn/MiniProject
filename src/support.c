#include "stm32f0xx.h"

void nano_wait(unsigned int n) {
    asm(    "        mov r0,%0\n"
            "repeat: sub r0,#83\n"
            "        bgt repeat\n" : : "r"(n) : "r0", "cc");
}

//===========================================================================
// Keypad 1 Configurations: GPIOC
//===========================================================================
// 16 history bytes.  Each byte represents the last 8 samples of a button.
uint8_t hist[16];
char queue[2];  // A two-entry queue of button press/release events.
int qin;        // Which queue entry is next for input
int qout;       // Which queue entry is next for output

const char keymap[] = "DCBA#9630852*741";

void push_queue(int n) {
    queue[qin] = n;
    qin ^= 1;
}

char pop_queue() {
    char tmp = queue[qout];
    queue[qout] = 0;
    qout ^= 1;
    return tmp;
}

void update_history(int c, int rows)
{
    // We used to make students do this in assembly language.
    for(int i = 0; i < 4; i++) {
        hist[4*c+i] = (hist[4*c+i]<<1) + ((rows>>i)&1);
        if (hist[4*c+i] == 0x01)
            push_queue(0x80 | keymap[4*c+i]);
    }
}

void drive_column(int c)
{
    GPIOC->BSRR = 0xf00000 | (~(1 << (c + 4)) & 0xff00ff);

}

int read_rows()
{
    return (~GPIOC->IDR) & 0xf;
}

char get_key_event(void) {
    for(;;) {
        asm volatile ("wfi" : :);   // wait for an interrupt
        if (queue[qout] != 0)
            break;
    }
    return pop_queue();
}

char get_keypress() {
    char event;
    for(;;) {
        // Wait for every button event...
        event = get_key_event();
        // ...but ignore if it's a release.
        //if (event & 0x80)
        break;
    }
    return event;
}

//===========================================================================
// Keypad 2 Configurations: GPIOC
//===========================================================================
// 16 history bytes.  Each byte represents the last 8 samples of a button.
uint8_t hist2[16];
char queue2[2];  // A two-entry queue of button press/release events.
int qin2;        // Which queue entry is next for input
int qout2;       // Which queue entry is next for output

void update_history2(int c, int rows)
{
    // We used to make students do this in assembly language.
    for(int i = 0; i < 4; i++) {
        hist2[4*c+i] = (hist2[4*c+i]<<1) + ((rows>>i)&1);
        if (hist2[4*c+i] == 0x01)
            push_queue(keymap[4*c+i]);
    }
}

void drive_column2(int c)
{
    GPIOC->BSRR = 0xf0000000 | ((~(1 << (c + 12)) & 0xf000f000));

}

int read_rows2()
{
    return ((~GPIOC->IDR) & 0xf00) >> 8;
}