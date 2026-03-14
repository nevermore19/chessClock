#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

/* LCD Pin Definitions - Connected to Port D */
#define LCD_RS PD2
#define LCD_EN PD3
#define LCD_D4 PD4
#define LCD_D5 PD5
#define LCD_D6 PD6
#define LCD_D7 PD7

/* Button Pin Definitions - Connected to Port B */
#define BUTTON1 PB2   // Left Button (Pin 10)
#define BUTTONMID PB1 // Middle Button (Pin 9)
#define BUTTON2 PB0   // Right Button (Pin 8)

/* GLOBAL STATE  VARIABLES*/
uint8_t state = 0;              // 0: Menu, 1: Game, 2: Win, 3: Pause
uint8_t currentPlayer = 0;      // 1 for Player 1, 2 for Player 2
uint32_t player1Time;           // Remaining time for P1 (ms)
uint32_t player2Time;           // Remaining time for P2 (ms)
uint32_t chosenTime = 60000;    // Default game time from menu
uint32_t lastUpdate = 0;        // Timestamp for delta time calculations
uint32_t lastLCDupdate = 0;     // Timestamp to limit LCD refresh rate
uint8_t option = 0;             // Currently selected menu index

bool winnerRendered = false;    // Prevents LCD flickering in Win screen
uint32_t lastAnimTime = 0;      // Timestamp for victory animation
bool armState = false;          // Toggle for "waving" animation frames

/* --- MILLIS IMPLEMENTATION --- */
volatile uint32_t millis_counter = 0;
ISR(TIMER0_COMPA_vect) {
    millis_counter++;   // Increment every 1ms
}

uint32_t millis() {
    uint32_t m;
    cli();                  // Disable interrupts to read 32-bit value safely
    m = millis_counter;
    sei();                  // Re-enable interrupts
    return m;
}

void timer0_init() {
    TCCR0A |= (1 << WGM01);                 // Set CTC mode (Clear Timer on Compare)
    OCR0A = 249;                            // Match value for 1ms at 16MHz/64 prescaler
    TCCR0B |= (1 << CS01) | (1 << CS00);    // 64 Prescaler
    TIMSK0 |= (1 << OCIE0A);                // Enable interrupt
}

/* --- LCD LOW-LEVEL FUNCTIONS --- */
void lcd_enable() {
    PORTD |= (1 << LCD_EN);
    _delay_us(1);
    PORTD &= ~(1 << LCD_EN);
}

void lcd_send4(uint8_t data) {
    // Distribute 4 bits of data to the corresponding PORTD pins
    if(data & 1) PORTD |= (1<<LCD_D4); else PORTD &= ~(1<<LCD_D4);
    if(data & 2) PORTD |= (1<<LCD_D5); else PORTD &= ~(1<<LCD_D5);
    if(data & 4) PORTD |= (1<<LCD_D6); else PORTD &= ~(1<<LCD_D6);
    if(data & 8) PORTD |= (1<<LCD_D7); else PORTD &= ~(1<<LCD_D7);
    lcd_enable();
}

void lcd_command(uint8_t cmd) {
    PORTD &= ~(1<<LCD_RS);      // RS Low for command
    lcd_send4(cmd >> 4);        // Send high nibble
    lcd_send4(cmd & 0x0F);      // Send low nibble
    _delay_ms(2);
}

void lcd_data(uint8_t data) {
    PORTD |= (1<<LCD_RS);       // RS High for data
    lcd_send4(data >> 4);
    lcd_send4(data & 0x0F);
    _delay_ms(2);
}

void lcd_init() {
    DDRD |= (1<<LCD_RS)|(1<<LCD_EN)|(1<<LCD_D4)|(1<<LCD_D5)|(1<<LCD_D6)|(1<<LCD_D7);
    _delay_ms(50);
    PORTD &= ~(1<<LCD_RS);
    // Initialize HD44780 in 4-bit mode
    lcd_send4(0x03); _delay_ms(5);
    lcd_send4(0x03); _delay_ms(5);
    lcd_send4(0x03); _delay_ms(1);
    lcd_send4(0x02);
    lcd_command(0x28);  // 2 lines, 5x8 matrix
    lcd_command(0x08);  // Display Off
    lcd_command(0x01);  // Clear display
    _delay_ms(3);
    lcd_command(0x06);  // Increment cursor
    lcd_command(0x0C);  // Display on, cursor off
}

/* --- LCD HELPER FUNCTIONS --- */
void lcd_clear() { lcd_command(0x01); }
void lcd_setCursor(uint8_t col, uint8_t row) {
    uint8_t addr = (row==0 ? 0x00 : 0x40) + col;
    lcd_command(0x80 | addr);
}
void lcd_print(char *str) { while(*str) lcd_data(*str++); }
void lcd_blink() { lcd_command(0x0F); }     // Show blinking cursor
void lcd_noBlink() { lcd_command(0x0C); }   // Hide cursor


/* Button reading with Active-High logic */
bool readButton(uint8_t pin) {
    return (PINB & (1 << pin)); 
}

/* Converts milliseconds to MM:SS.t string format */
void formatTime(char *buffer, uint32_t ms) {
    uint8_t minutes = (ms / 60000) % 60;
    uint8_t seconds = (ms / 1000) % 60;
    uint8_t mili = (ms % 1000) / 100;
    sprintf(buffer, "%02d:%02d.%d", minutes, seconds, mili);
}

void displayOptions() {
    lcd_setCursor(2, 0);
    lcd_print("Chess Clock");
    lcd_setCursor(0, 1);
    lcd_print("1m 3m 10m 30m 1h");
}

void displayGame() {
    char buf[16];
    lcd_setCursor(3, 0); lcd_print("P1");
    lcd_setCursor(11, 0); lcd_print("P2");
    formatTime(buf, player1Time);
    lcd_setCursor(0, 1); lcd_print(buf);
    formatTime(buf, player2Time);
    lcd_setCursor(9, 1); lcd_print(buf);
}

/* Custom characters for victory animation (stick figures) */
uint8_t armsDown[8] = {
  0b00100, 0b01010, 0b00100, 0b00100, 0b01110, 0b10101, 0b00100, 0b01010
};

uint8_t armsUp[8] = {
  0b00100, 0b01010, 0b00100, 0b10101, 0b01110, 0b00100, 0b00100, 0b01010
};

/* Uploads custom character pattern to LCD's CGRAM */
void lcd_createChar(uint8_t location, uint8_t charmap[]) {
    location &= 0x7;
    lcd_command(0x40 | (location << 3));
    for (uint8_t i = 0; i < 8; i++) {
        lcd_data(charmap[i]);
    }
    lcd_command(0x80);  // Return to DDRAM (normal display)
}

void setup() {
    // Configure buttons as inputs (Active-High assumes external pull-down resistors)
    DDRB &= ~((1<<BUTTON1)|(1<<BUTTONMID)|(1<<BUTTON2));
    PORTB &= ~((1<<BUTTON1)|(1<<BUTTONMID)|(1<<BUTTON2));
    
    lcd_init();     // Initialize LCD first
    
    // Register custom victory characters
    lcd_createChar(3, armsDown);
    lcd_createChar(4, armsUp);

    timer0_init();  // Start millis timer
    sei();          // Enable global interrupts
}

int main() {
    setup();

    uint32_t now;
    uint32_t pressStartTime = 0;
    bool isPressingMid = false;
    bool triggerShortPress = false;
    uint8_t lastOption = 255;   // Used to trigger refresh only when menu selection changes
    
    // Edge detection variables
    bool lastB1 = readButton(BUTTON1);
    bool lastB2 = readButton(BUTTON2);

    /* --- MAIN LOOP --- */
    while(1) {
        now = millis(); 
        bool b1 = readButton(BUTTON1);
        bool b2 = readButton(BUTTON2);
        bool mid = readButton(BUTTONMID);

        /* MIDDLE BUTTON LOGIC:
           - Short press: Pause / Select
           - Long press (3s): Full System Reset */
        if (mid) {
            if (!isPressingMid) {
                isPressingMid = true;
                pressStartTime = now;
            } else {
                if (now - pressStartTime >= 3000) {     // Long press detected
                    state = 0;
                    currentPlayer = 0;
                    lcd_clear();
                    lcd_print("RESET");
                    for(int i=0; i<3; i++) {_delay_ms(200); lcd_print(".");}
                    _delay_ms(1500);
                    lcd_clear();
                    isPressingMid = false;
                    triggerShortPress = false;
                    lastOption = 255;
                    while(readButton(BUTTONMID)); // Wait for release before continuing
                }
            }
        } else {
            if (isPressingMid) {
                // Short press: released before 2s but after 50ms (debounce)
                if (now - pressStartTime < 2000 && now - pressStartTime > 50) {
                    triggerShortPress = true;
                }
                isPressingMid = false;
            }
        }

        // Side buttons edge detection (detects the moment button is pressed)
        bool press1 = b1 && !lastB1;
        bool press2 = b2 && !lastB2;
        lastB1 = b1;
        lastB2 = b2;

        /* STATE MACHINE */
        switch(state) {
            case 0: // MENU: Choose starting time
                if (option != lastOption) {
                    lcd_clear();
                    displayOptions();
                    lastOption = option;
                }

                if(press1) { option = (option == 0) ? 4 : option - 1; }
                if(press2) { option = (option + 1) % 5; }

                // Move blinking cursor to selected time
                switch(option) {
                    case 0: chosenTime = 60000;   lcd_setCursor(0, 1);  break; // 1min
                    case 1: chosenTime = 180000;  lcd_setCursor(3, 1);  break; // 3min
                    case 2: chosenTime = 600000;  lcd_setCursor(6, 1);  break; // 10min
                    case 3: chosenTime = 1800000; lcd_setCursor(10, 1); break; // 30min
                    case 4: chosenTime = 3599999; lcd_setCursor(14, 1); break; // 1h
                }
                lcd_blink();

                if(triggerShortPress) {
                    triggerShortPress = false;
                    lcd_noBlink();
                    player1Time = player2Time = chosenTime;
                    state = 3; // Start in PAUSE state to allow players to prepare
                    lcd_clear();
                    lastUpdate = millis();
                }
                break;

            case 1: // GAME: Active countdown
                if(now - lastLCDupdate > 100) {     // Refresh LCD every 100ms for performance
                    displayGame();
                    lastLCDupdate = now;
                }
                if(triggerShortPress) {
                    triggerShortPress = false;
                    state = 3; break;               // Enter Pause
                }
                // Switch active player
                if(press1) currentPlayer = 2;
                if(press2) currentPlayer = 1;

                // Time deduction logic
                uint32_t delta = now - lastUpdate;
                lastUpdate = now;
                if(currentPlayer == 1) {
                    if(player1Time > delta) player1Time -= delta;
                    else { player1Time = 0; state = 2; lcd_clear(); }   // P1 timeout
                } else if(currentPlayer == 2) {
                    if(player2Time > delta) player2Time -= delta;
                    else { player2Time = 0; state = 2; lcd_clear(); }   // P2 timeout
                }
                break;

            case 2: // WINNING: Show winner and play animation

                if (!winnerRendered) {
                    lcd_clear();
                    lcd_setCursor(4, 0);
                    lcd_print(player1Time == 0 ? "Player 2" : "Player 1");
                    lcd_setCursor(6, 1);
                    lcd_print("Won");
                    winnerRendered = true;
                }
            
                // Waving stick figures animation every 450ms
                if (now - lastAnimTime > 450) {
                    armState = !armState;
                    lastAnimTime = now;
                    
                    lcd_setCursor(0, 1);
                    lcd_data(armState ? 3 : 4); // Reset to menu
                    
                    lcd_setCursor(15, 1);
                    lcd_data(armState ? 3 : 4);
                }

                if (triggerShortPress) {
                    state = 0;
                    winnerRendered = false;
                    currentPlayer = 0;
                    lcd_clear();
                    lcd_print("NEW GAME");
                    for(int i=0; i<3; i++) {_delay_ms(200); lcd_print(".");}
                    _delay_ms(1000);
                    lcd_clear();
                    isPressingMid = false;
                    triggerShortPress = false;
                    lastOption = 255;
                }
                break;

            case 3: // PAUSE: Clock frozen
                if(now - lastLCDupdate > 200) {
                    displayGame();
                    lcd_setCursor(7, 0); lcd_print("||");   // Show pause icon
                    lastLCDupdate = now;
                }
                if(triggerShortPress) {
                    triggerShortPress = false;
                    state = 1; lastUpdate = millis();       // Resume
                }
                // Resume game by pressing any player's button
                if(press1) { currentPlayer = 2; state = 1; lcd_clear(); lastUpdate = millis(); }
                if(press2) { currentPlayer = 1; state = 1; lcd_clear(); lastUpdate = millis(); }
                break;
        }
        _delay_ms(20);  // Small delay to reduce CPU load and assist debouncing
    }
}