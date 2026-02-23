// Link to wokwi simulation: https://wokwi.com/projects/438081155878376449

#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"

// SCK, MOSI, DC, RST, CS connected to 10k Ohm resistors 

#define TFT_DC 9              
#define TFT_CS 10             
#define TFT_RST 8             
#define TFT_MISO 12           
#define TFT_MOSI 11           
#define TFT_SCK 13  
#define BUZZER 3

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCK, TFT_RST, TFT_MISO);

#define BLACK 0x0000
#define BLUE 0x001F
#define RED 0xF800
#define GREEN 0x07E0
#define CYAN 0x07FF
#define MAGENTA 0xF81F
#define YELLOW 0xFFE0
#define ORANGE 0xEC84
#define WHITE 0xFFFF

volatile uint8_t buttonFlags = 0;
volatile uint8_t lastPortDState;

const unsigned int colours[9] = {BLACK, CYAN, YELLOW, MAGENTA, GREEN, RED, BLUE, ORANGE, WHITE};
const int tetromino[7][16] = 
  {{0, 0, 1, 0,
    0, 0, 1, 0,
    0, 0, 1, 0,
    0, 0, 1, 0},
   {0, 0, 0, 0,
    0, 2, 2, 0,
    0, 2, 2, 0,
    0, 0, 0, 0},
   {0, 0, 3, 0,
    0, 3, 3, 0,
    0, 0, 3, 0,
    0, 0, 0, 0},
   {0, 4, 0, 0,
    0, 4, 4, 0,
    0, 0, 4, 0,
    0, 0, 0, 0},
   {0, 0, 5, 0,
    0, 5, 5, 0,
    0, 5, 0, 0,
    0, 0, 0, 0},
   {0, 0, 6, 0,
    0, 0, 6, 0,
    0, 6, 6, 0,
    0, 0, 0, 0},
   {0, 7, 0, 0,
    0, 7, 0, 0,
    0, 7, 7, 0,
    0, 0, 0, 0}};

int field[264] = {0};
int prev[264] = {0};
const int fieldWidth = 12;
const int fieldHeight = 22;
const int screenWidth = 240;
const int screenHeight = 320;
const int side = 10;

int currentPiece;
int currentX;
int currentY;
int currentRotation;
bool gameOver;
bool forceDown;
bool updateScore;
int speedCount;
int speed;
int score;
int completedLines[4];
unsigned long prevMillis = 0;
unsigned long period = 500;

ISR(PCINT2_vect){
  // XOR operator to find changed states
  uint8_t change = lastPortDState ^ PIND;
  lastPortDState = PIND;

  buttonFlags |= (change & 0b11110000) & ~(PIND & 0b11110000);
}

void initialize(){
  memset(field, 0, sizeof(prev));
  memset(prev, 0, sizeof(prev));
  for (int x = 0; x < fieldWidth; x++)
    for (int y = 0; y < fieldHeight; y++){
      if (x == 0 || x == fieldWidth - 1 || y == fieldHeight - 1)
        field[y * fieldWidth + x] = 8;
    }
  currentPiece = random(7);
  currentX = fieldWidth / 2;
  currentY = 0;
  currentRotation = 0;
  gameOver = false;
  forceDown = false;
  updateScore = true;
  speedCount = 0;
  speed = 300;
  score = 0;
  for (int i = 0; i < 4; i++){
    completedLines[i] = -1;
  }
}

int rotate(int px, int py, int r){
  int pi = 0;
  switch (r % 4){
  case 0: // 0 degrees          // 0  1  2  3
    pi = py * 4 + px;           // 4  5  6  7
    break;                      // 8  9 10 11
                                //12 13 14 15

  case 1: // 90 degrees         //12  8  4  0
    pi = 12 + py - (px * 4);    //13  9  5  1
    break;                      //14 10  6  2
                                //15 11  7  3

  case 2: // 180 degrees        //15 14 13 12
    pi = 15 - (py * 4) - px;    //11 10  9  8
    break;                      // 7  6  5  4
                                // 3  2  1  0

  case 3: // 270 degrees        // 3  7 11 15
    pi = 3 - py + (px * 4);     // 2  6 10 14
    break;                      // 1  5  9 13
  }                             // 0  4  8 12
  return pi;
}

bool doesPieceFit(int n, int rotation, int posX, int posY){
  for (int px = 0; px < 4; px++)
    for (int py = 0; py < 4; py++){
      int pi = rotate(px, py, rotation);
      int fi = (posY + py) * fieldWidth + (posX + px);
      // short-circuit evaluation to ensure not accessing memory out of bounds
      if ((posX + px >= 0 && posX + px < fieldWidth) &&
          (posY + py >= 0 && posY + py < fieldHeight) &&
          (tetromino[n][pi] != 0 && field[fi] != 0)){
          return false;
      }
    }
  return true;
}

void setup() {
  Serial.begin(9600);
  tft.begin();  
  tft.fillScreen(BLACK);

  // enable pin change interrupt control register for port D
  PCICR |= (1 << PCIE2);

  // which pins in port D to monitor changes i.e. pins 4 - 7
  PCMSK2 |= (1 << PCINT20) | (1 << PCINT21) | (1 << PCINT22) | (1 << PCINT23);

  // 4 - right
  // 5 - down
  // 6 - rotate
  // 7 - left
  for (int i = 4; i < 8; i++){
    pinMode(i, INPUT_PULLUP);
  }

  pinMode(BUZZER, OUTPUT);

  lastPortDState = PIND;

  randomSeed(analogRead(A0));
  initialize();
}

void loop() {
  
  if (!gameOver){
    forceDown = ((millis() - prevMillis) > period);
    if (buttonFlags & (1 << 4)){
      currentX += (doesPieceFit(currentPiece, currentRotation, currentX + 1, currentY)) ? 1 : 0;
      buttonFlags &= ~(1 << 4);
    }
    if (buttonFlags & (1 << 5)){
      currentY += (doesPieceFit(currentPiece, currentRotation, currentX, currentY + 1)) ? 1 : 0;
      buttonFlags &= ~(1 << 5);
    }
    if (buttonFlags & (1 << 6)){
      currentRotation += (doesPieceFit(currentPiece, currentRotation + 1, currentX, currentY)) ? 1 : 0;
      buttonFlags &= ~(1 << 6);
    }
    if (buttonFlags & (1 << 7)){
      currentX -= (doesPieceFit(currentPiece, currentRotation, currentX - 1, currentY)) ? 1 : 0;
      buttonFlags &= ~(1 << 7);
    }

    if (forceDown){
      prevMillis = millis();
      if (doesPieceFit(currentPiece, currentRotation, currentX, currentY + 1)){
        currentY++;
      } else {
        // entering piece into field
        for (int px = 0; px < 4; px++)
          for (int py = 0; py < 4; py++){
            int block = tetromino[currentPiece][rotate(px, py, currentRotation)];
            if (block != 0) field[(currentY + py) * fieldWidth + (currentX + px)] = block;
          }

        // identifying completed lines and removing if any
        for (int py = 0; py < 4; py++){
          bool complete = true;
          for (int fx = 1; fx < fieldWidth - 1; fx++){
            int fi = (currentY + py) * fieldWidth + fx;
            if (field[fi] == 0 || field[fi] == 8) 
              complete = false;
          }
          if (complete){
            tone(BUZZER, 800, 200);
            completedLines[py] = currentY + py; 
            score += 100;
            updateScore = true;
            for (int fx = 1; fx < fieldWidth - 1; fx++)
              field[completedLines[py] * fieldWidth + fx] = 0;
          }
        }
        currentPiece = random(7);
        currentX = fieldWidth / 2;
        currentY = 0;
        currentRotation = 0;

        gameOver = !doesPieceFit(currentPiece, currentRotation, currentX, currentY);
      }
    }

    // entering current piece into field
    int del[4];
    int iPiece = 0;
    for (int px = 0; px < 4; px++)
      for (int py = 0; py < 4; py++){
        int block = tetromino[currentPiece][rotate(px, py, currentRotation)];
        if (block != 0){
          int fi = (currentY + py) * fieldWidth + (currentX + px);
          field[fi] = block;
          del[iPiece] = fi;
          iPiece++;
        }
      }

    // drawing field
    for (int x = 0; x < fieldWidth; x++)
      for (int y = 0; y < fieldHeight; y++){
        int fi = y * fieldWidth + x;
        if (field[fi] != prev[fi])
          tft.fillRect(x * side + 60, y * side + 30, side, side, colours[field[y * fieldWidth + x]]);
      }
    
    // copying field to prev field
    for (int fx = 0; fx < fieldWidth; fx++)
      for (int fy = 0; fy < fieldHeight; fy++){
        int fi = fy * fieldWidth + fx;
        prev[fi] = field[fi];
      }

    // removing current piece from field
    for (int i = 0; i < 4; i++){
      field[del[i]] = 0;
    }
    
    if (updateScore){
      tft.setCursor(2, 2);
      tft.setTextColor(WHITE);
      tft.setTextSize(2);
      tft.print("Score: " );
      tft.fillRect(70, 0, 170, 30, BLACK);
      tft.print(score);
      updateScore = false;
    }

    // moving blocks above completed line down (if any)
    for (int i = 0; i < 4; i++)
      if (completedLines[i] != -1){
        for (int fx = 1; fx < fieldWidth - 1; fx++)
          for (int fy = completedLines[i]; fy > 0; fy--){
            field[fy * fieldWidth + fx] = field[(fy - 1) * fieldWidth + fx];
          }
        completedLines[i] = -1;
      }

  } else {
    tft.fillScreen(BLACK);
    tft.setCursor(30, 100);
    tft.setTextColor(RED);
    tft.setTextSize(3);
    tft.print("GAME OVER!");
    tft.setCursor(30, 140);
    tft.setTextColor(GREEN);
    tft.setTextSize(2);
    tft.print("Score: ");
    tft.print(score);
    while(true){
      if (buttonFlags & (1 << 4)){
        tft.fillScreen(BLACK);
        initialize();
        break;
      }
    }
  }
}
