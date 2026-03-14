#include <LiquidCrystal.h>
#include <time.h>
#include <stdio.h>

const LiquidCrystal lcd(2, 3, 4, 5, 6, 7); // rs, en, d4, d5, d6, d7
const int button1Pin = 10;
const int buttonMidPin = 9;
const int button2Pin = 8;

// 0 - chosing time
// 1 - playing
// 2 - time ended
// 3 - pouse
int state; 
int currentPlayer; // 0 for none, 1 for first, 2 for second
unsigned long long player1Time;
unsigned long long player2Time;
unsigned long long lastUpdate = 0;

unsigned long chosenTime = 0;

int option = 0;

static bool lastButton1State = LOW;
static bool lastButton2State = LOW;


/* Formats ms into "MM:SS:m"
 buffer: The char array to store the result
 size: The size of the buffer (should be at least 8 bytes)
 t: The ms value to convert
*/
void formatTime(char *buffer, size_t size, unsigned long ms) {
    int minutes = (ms / 60000) % 60;
    int seconds = (ms / 1000) % 60;
    int miliDigit = ms % 1000;

    snprintf(buffer, size, "%02d:%02d.%d", minutes, seconds, miliDigit);
}

void displayOptions() {
  lcd.setCursor(2, 0);
  lcd.print("Chess  Clock");
  lcd.setCursor(0, 1);
  lcd.print("1m 3m 10m 30m 1h");
}

void displayGame() {
  lcd.setCursor(3, 0);
  lcd.print("P1");
  lcd.setCursor(11, 0);
  lcd.print("P2");

  char timeStr[8];


  // Player 1
  formatTime(timeStr, sizeof(timeStr), player1Time);
  lcd.setCursor(0, 1);
  lcd.print(timeStr);

  // Player 2
  formatTime(timeStr, sizeof(timeStr), player2Time);
  lcd.setCursor(9, 1);
  lcd.print(timeStr);
}

void displayPause() {
  displayGame();
  lcd.setCursor(7, 0);
  lcd.print("||");
}

byte armsDown[8] = {
  0b00100,
  0b01010,
  0b00100,
  0b00100,
  0b01110,
  0b10101,
  0b00100,
  0b01010
};

byte armsUp[8] = {
  0b00100,
  0b01010,
  0b00100,
  0b10101,
  0b01110,
  0b00100,
  0b00100,
  0b01010
};


void displayVictory() {
  lcd.setCursor(0,1);
  lcd.write(3);
  delay(450);
  lcd.setCursor(0, 1);
  lcd.write(4);
  delay(450);

  lcd.setCursor(15,1);
  lcd.write(3);
  delay(450);
  
  lcd.setCursor(15, 1);
  lcd.write(4);
  delay(450);
}

void setup() {
  Serial.begin(9600);
  lcd.begin(16, 2);
  lcd.clear();

  lcd.createChar(3, armsDown);
  lcd.createChar(4, armsUp);

  state = 0; // menu
  currentPlayer = 0; // none
}

void loop() {
  int button1State = digitalRead(button1Pin);
  int buttonMidState = digitalRead(buttonMidPin);
  int button2State = digitalRead(button2Pin);
  unsigned long now = millis();
  unsigned long delta = now - lastUpdate;
  lastUpdate = now;

  static unsigned long pressStartTime = 0;
  static bool lastButtonState = LOW;
  bool triggerShortPress = false;


  if (buttonMidState == HIGH && lastButtonState == LOW) {
    pressStartTime = now;
  } 
  
  if (buttonMidState == HIGH) {
    if (now - pressStartTime >= 3000) {
      state = 0;
      currentPlayer = 0;
      lcd.clear();
      lcd.print("RESET...");
      delay(1500);

      pressStartTime = millis();
      pressStartTime = millis() - 3000;
      lcd.clear();
    }
  }

  if (buttonMidState == LOW && lastButtonState == HIGH) {
    // Check if it was a short press
    if (now - pressStartTime < 2000) {
      triggerShortPress = true;
    }
  }
  lastButtonState = buttonMidState;

  switch (state) {
    case 0: // menu
    
      displayOptions();

      if (button1State == HIGH && lastButton1State == LOW) {
        option = (option == 0) ? 4 : option - 1;
        lcd.clear();
      }
      lastButton1State = button1State;

      // Logika dla przycisku 2 (Wstecz)
      if (button2State == HIGH && lastButton2State == LOW) {
        option = (option + 1) % 5;
        lcd.clear();
      }
      lastButton2State = button2State;

      switch (option) {
        case 0:
               chosenTime = 1000;
               lcd.setCursor(0, 1);
               break;
        case 1:
               chosenTime = 60000 * 3;
               lcd.setCursor(3, 1);
               break; 
        case 2:
               chosenTime = 60000 * 10;
               lcd.setCursor(6, 1);
               break; 
        case 3:
               chosenTime = 60000 * 30;
               lcd.setCursor(10, 1);
               break; 
        case 4:
               chosenTime = 60000 * 60;
               lcd.setCursor(14, 1);
               break; 
      }
      lcd.blink();      

      if (triggerShortPress) { 
        // time in ms
        lcd.noBlink();
        player1Time = chosenTime;  // 60 minutes (when setup as 10 * 60 * 1000 it tends to have strange outcomes)
        player2Time = chosenTime; // 60 minutes 

        state = 3;
        lcd.clear();
        break;
      }

      // TODO: full menu
      break;
    case 1: // game
      // TODO: reset on button held for 3 seconds
      // TODO: stop time after time <= 0

      if (triggerShortPress) {
        state = 3; // Pauza
        currentPlayer = 0;
        break;
      } else if (button1State == HIGH) {
        currentPlayer = 2;
      } else if (button2State == HIGH) {
        currentPlayer = 1;
      }

      if (currentPlayer == 1) {
        if (player1Time > delta) {
          player1Time -= delta;
        } else {
          player1Time = 0; 
          state = 2; 
        }
      } else if (currentPlayer == 2) {
        if (player2Time > delta) {
          player2Time -= delta;
        } else {
          player2Time = 0; 
          state = 2; 
        }
      }

      displayGame();

      break;
    case 2: // time ended
      lcd.clear();
      lcd.setCursor(4,0);

      if (player1Time == 0) {
        lcd.print("Player 2");
      } else {
        lcd.print("Player 1");
      }

      lcd.setCursor(6,1);
      lcd.print("Won");

      displayVictory();
      break;
    case 3: // pouse
      // TODO: reset on button held for 3 seconds
      displayPause();

      if (button1State == HIGH) { 
        lcd.clear();
        currentPlayer = 2;
        state = 1;
        break;
      } else if (button2State == HIGH) {
        lcd.clear();
        currentPlayer = 1;
        state = 1;
        break;
      }
      break;
  }
  delay(30);
}
