#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <EEPROM.h>

// -------------------- Display pins --------------------
#define TFT_CS 15
#define TFT_DC 2

// -------------------- Joystick pins --------------------
#define JOYSTICK_HORZ 34
#define JOYSTICK_VERT 35
#define JOYSTICK_BUTTON 32

// -------------------- Buzzer pin --------------------
#define BUZZER_PIN 27

Adafruit_ILI9341 tft(TFT_CS, TFT_DC);

// -------------------- Screen settings --------------------
const int SCREEN_WIDTH = 320;
const int SCREEN_HEIGHT = 240;

// Top area reserved for score information
const int PLAY_AREA_TOP = 20;

//Snake block occupies
const int BLOCK_SIZE = 10;

// -------------------- Level 2 digit barrier --------------------

const int DIGIT_ONE_CELL_COUNT = 14;

// Each row contains: {X position, Y position}
const int digitOneCells[DIGIT_ONE_CELL_COUNT][2] = {
  {150, 70},
  {160, 60},
  {160, 70},
  {160, 80},
  {160, 90},
  {160, 100},
  {160, 110},
  {160, 120},
  {160, 130},
  {160, 140},
  {160, 150},
  {150, 160},
  {160, 160},
  {170, 160}
};

// Outer boundaries of the digit
const int BARRIER_MIN_X = 150;
const int BARRIER_MAX_X = 170;
const int BARRIER_MIN_Y = 60;
const int BARRIER_MAX_Y = 160;

// Food remain distance
const int BARRIER_MARGIN = 20;

// -------------------- Snake settings --------------------
const int MAX_SNAKE_LENGTH = 100;

int snakeX[MAX_SNAKE_LENGTH];
int snakeY[MAX_SNAKE_LENGTH];

int snakeLength = 3;

// -------------------- Food position --------------------
int foodX;
int foodY;

// -------------------- Level 4+ bad foods --------------------

const int MAX_BAD_FOODS = 10;

// Position arrays for multiple red foods
int badFoodX[MAX_BAD_FOODS];
int badFoodY[MAX_BAD_FOODS];

// Number currently displayed
int badFoodCount = 0;

// -------------------- Score and level --------------------
int score = 0;
int highScore = 0;
int level = 1;

// -------------------- EEPROM settings --------------------
const int EEPROM_SIZE = 16;
const int HIGH_SCORE_ADDRESS = 0;

// Last value stored in EEPROM
int savedHighScore = 0;

// -------------------- Menu settings --------------------
enum ScreenMode {
  MENU_SCREEN,
  GAME_SCREEN,
  HIGH_SCORE_SCREEN
};

ScreenMode currentScreen = MENU_SCREEN;

int menuSelection = 0;

bool menuJoystickMoved = false;

// -------------------- Game state --------------------
bool gameOver = false;
int previousButtonState = HIGH;

// -------------------- Direction --------------------
enum Direction {
  UP,
  DOWN,
  LEFT,
  RIGHT
};

Direction currentDirection = RIGHT;
Direction requestedDirection = RIGHT;

// -------------------- Movement timing --------------------
unsigned long previousMoveTime = 0;

// Initial speed used for Levels 1 to 4
const unsigned long BASE_MOVE_INTERVAL = 250;

// Current movement speed
unsigned long moveInterval = BASE_MOVE_INTERVAL;

// Prevent the snake from becoming uncontrollably fast
const unsigned long MIN_MOVE_INTERVAL = 60;

// -------------------- Level 3 food timer --------------------
const unsigned long FOOD_LIFETIME = 5000;

unsigned long foodCreatedTime = 0;
int lastDisplayedCountdown = -1;
// -------------------- Function declarations --------------------
// Menu functions
void drawMainMenu();
void drawMenuOptions();
void handleMainMenu();
void drawHighScoreScreen();
void handleHighScoreScreen();
void startNewGame();
void showStartCountdown();
bool joystickButtonPressed();

// EEPROM functions
void loadHighScore();
void saveHighScore();

void initialiseSnake();
void readJoystick();
void moveSnake();

void eraseSnake();
void drawSnake();

void generateFood();
void drawFood();
bool isFoodOnSnake();
bool hasSnakeEatenFood();

void drawHeader();
void updateHeader();
void updateLevel();

// Level 2 functions
void drawDigitBarrier();
bool isBarrierCell(int x, int y);
bool hasSnakeHitBarrier();
bool isFoodNearBarrier();
bool isSnakeNearBarrier();
void moveSnakeToCorner();
void activateLevelTwo();

// Level 3 functions
void resetFoodTimer();
void updateFoodTimer();
void drawCountdownValue(int seconds);

// Level 4 functions
void generateBadFoods();
void eraseFoodItems();

bool isBadFoodOnSnake(int x, int y);
bool isBadFoodNearBarrier(int x, int y);
bool isBadFoodOnNormalFood(int x, int y);
bool isBadFoodOnExistingBadFood(
    int x,
    int y,
    int numberAlreadyGenerated
);

int getEatenBadFoodIndex();

void playBadFoodSound();

// Level 5 and onwards
void updateGameSpeed();
int calculateBadFoodCount();

bool hasSnakeHitBody();
void showGameOver();
void restartGame();

void playStartSound();
void playFoodSound();
void playGameOverSound();

// -------------------------------------------------------
// Initial hardware and game setup
// -------------------------------------------------------
void setup() {
  Serial.begin(115200);

  // Configure joystick pins
  pinMode(JOYSTICK_HORZ, INPUT);
  pinMode(JOYSTICK_VERT, INPUT);
  pinMode(JOYSTICK_BUTTON, INPUT_PULLUP);

  // Configure buzzer pin
  pinMode(BUZZER_PIN, OUTPUT);

  // Start the TFT display
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(ILI9341_BLACK);

  // Different random food positions
  randomSeed(micros());

// EEPROM emulation
EEPROM.begin(EEPROM_SIZE);

// Read the previously saved high score
loadHighScore();

// Open the main menu
currentScreen = MENU_SCREEN;
menuSelection = 0;

drawMainMenu();

previousButtonState =
    digitalRead(JOYSTICK_BUTTON);

Serial.println("Main menu opened");
}

void loop() {
// -------------------------------------------------------
  // MAIN MENU
// -------------------------------------------------------
  if (currentScreen == MENU_SCREEN) {
    handleMainMenu();
    delay(10);
    return;
  }

 // -------------------------------------------------------
  // HIGH-SCORE SCREEN
// -------------------------------------------------------
  if (currentScreen == HIGH_SCORE_SCREEN) {
    handleHighScoreScreen();
    delay(10);
    return;
  }
 // -------------------------------------------------------
  // PART 1: GAME-OVER MODE
// -------------------------------------------------------
  if (gameOver) {
    int currentButtonState =
        digitalRead(JOYSTICK_BUTTON);

    if (currentButtonState == LOW &&
        previousButtonState == HIGH) {

      delay(30); 

      // Confirm the button is still pressed
      if (digitalRead(JOYSTICK_BUTTON) == LOW) {

        // Return to the main menu
        gameOver = false;
        currentScreen = MENU_SCREEN;
        menuSelection = 0;
        menuJoystickMoved = false;

        drawMainMenu();

        previousButtonState = LOW;
      }
    }

    previousButtonState = currentButtonState;

    delay(10);

    return;
  }

  // Level 3 food countdown and timeout check
  updateFoodTimer();
// -------------------------------------------------------
  // PART 2: NORMAL GAMEPLAY
// -------------------------------------------------------
  readJoystick();

  unsigned long currentTime = millis();

  if (currentTime - previousMoveTime >= moveInterval) {
    previousMoveTime = currentTime;

    // Save the old tail position before moving
    int oldTailX = snakeX[snakeLength - 1];
    int oldTailY = snakeY[snakeLength - 1];

    moveSnake();

// -------------------------------------------------------
    // SELF-COLLISION CHECK
// -------------------------------------------------------
if (hasSnakeHitBody()) {
  gameOver = true;

  tft.fillRect(
    snakeX[0],
    snakeY[0],
    BLOCK_SIZE,
    BLOCK_SIZE,
    ILI9341_RED
  );

  playGameOverSound();

  delay(250);

  showGameOver();
  return;
}

// -------------------------------------------------------
// LEVEL 2 BARRIER COLLISION CHECK
// -------------------------------------------------------
if (hasSnakeHitBarrier()) {
  gameOver = true;

  // Change the collision cell color
  tft.fillRect(
    snakeX[0],
    snakeY[0],
    BLOCK_SIZE,
    BLOCK_SIZE,
    ILI9341_RED
  );

  Serial.println("Snake hit the Level 2 digit barrier");

  playGameOverSound();

  delay(250);

  showGameOver();
  return;
}

// -------------------------------------------------------
    // FOOD CHECK
// -------------------------------------------------------
bool normalFoodEaten = hasSnakeEatenFood();

int eatenBadFoodIndex =
    getEatenBadFoodIndex();

bool badFoodEaten =
    eatenBadFoodIndex >= 0;

// -------------------------------------------------------
// NORMAL FOOD EATEN
// -------------------------------------------------------
if (normalFoodEaten) {
  // Remove old food drawings before creating new ones
  eraseFoodItems();

  score++;

  if (score > highScore) {
    highScore = score;
  }

  // Normal food grows the snake
  if (snakeLength < MAX_SNAKE_LENGTH) {
    snakeX[snakeLength] = oldTailX;
    snakeY[snakeLength] = oldTailY;
    snakeLength++;
  }

  // Check whether a new level was reached
  updateLevel();

  // Generate normal food and bad food
  generateFood();

  updateHeader();
  drawFood();
  drawSnake();

  playFoodSound();

  Serial.println("Normal food eaten!");
}

// -------------------------------------------------------
// BAD RED FOOD EATEN
// -------------------------------------------------------
else if (badFoodEaten) {
  // Remove the previous normal and bad foods
  eraseFoodItems();

   // Bad food reduces the score by one.
  if (score > 0) {
    score--;
  }

  //Bad food does not grow the snake.

  tft.fillRect(
    oldTailX,
    oldTailY,
    BLOCK_SIZE,
    BLOCK_SIZE,
    ILI9341_BLACK
  );

  // Generate new normal and bad food
  generateFood();

  updateHeader();
  drawFood();
  drawSnake();

  playBadFoodSound();

  Serial.println("Bad food eaten - score reduced!");
}

// -------------------------------------------------------
// NO FOOD EATEN
// -------------------------------------------------------
else {
  // Erase the previous tail position
  tft.fillRect(
    oldTailX,
    oldTailY,
    BLOCK_SIZE,
    BLOCK_SIZE,
    ILI9341_BLACK
  );

  drawSnake();
}

// Continue directly with Serial output here
    Serial.print("Head X: ");
    Serial.print(snakeX[0]);

    Serial.print(" | Head Y: ");
    Serial.print(snakeY[0]);

    Serial.print(" | Score: ");
    Serial.print(score);

    Serial.print(" | Length: ");
    Serial.println(snakeLength);

    Serial.print(" | Level: ");
Serial.println(level);
  }

  delay(10);
}
// -------------------------------------------------------
// Set the initial snake position
// -------------------------------------------------------
void initialiseSnake() {
  snakeLength = 3;
  score = 0;
  level = 1;

  // Reset Level 5 settings
  moveInterval = BASE_MOVE_INTERVAL;
  badFoodCount = 0;

  // Head
  snakeX[0] = 160;
  snakeY[0] = 120;

  // First body block
  snakeX[1] = 150;
  snakeY[1] = 120;

  // Second body block
  snakeX[2] = 140;
  snakeY[2] = 120;

  currentDirection = RIGHT;
  requestedDirection = RIGHT;
}

// -------------------------------------------------------
// Request a joystick direction
// -------------------------------------------------------
void readJoystick() {
  int horizontalValue = analogRead(JOYSTICK_HORZ);
  int verticalValue = analogRead(JOYSTICK_VERT);


  // Joystick right
  if (horizontalValue < 1000 &&
      currentDirection != LEFT) {

    requestedDirection = RIGHT;
  }

  // Joystick left
  else if (horizontalValue > 3000 &&
           currentDirection != RIGHT) {

    requestedDirection = LEFT;
  }

  // Joystick down
  if (verticalValue < 1000 &&
      currentDirection != UP) {

    requestedDirection = DOWN;
  }

  // Joystick up
  else if (verticalValue > 3000 &&
           currentDirection != DOWN) {

    requestedDirection = UP;
  }
}

// -------------------------------------------------------
// Move the complete snake
// -------------------------------------------------------
void moveSnake() {
  currentDirection = requestedDirection;

  // Move body blocks from the tail towards the head
  for (int i = snakeLength - 1; i > 0; i--) {
    snakeX[i] = snakeX[i - 1];
    snakeY[i] = snakeY[i - 1];
  }

  // Move the head
  switch (currentDirection) {
    case UP:
      snakeY[0] -= BLOCK_SIZE;
      break;

    case DOWN:
      snakeY[0] += BLOCK_SIZE;
      break;

    case LEFT:
      snakeX[0] -= BLOCK_SIZE;
      break;

    case RIGHT:
      snakeX[0] += BLOCK_SIZE;
      break;
  }

  // -------------------- Edge wrapping --------------------

  if (snakeX[0] < 0) {
    snakeX[0] = SCREEN_WIDTH - BLOCK_SIZE;
  }

  if (snakeX[0] >= SCREEN_WIDTH) {
    snakeX[0] = 0;
  }

  if (snakeY[0] < PLAY_AREA_TOP) {
    snakeY[0] = SCREEN_HEIGHT - BLOCK_SIZE;
  }

  if (snakeY[0] >= SCREEN_HEIGHT) {
    snakeY[0] = PLAY_AREA_TOP;
  }
}

// -------------------------------------------------------
// Erase the current snake
// -------------------------------------------------------
void eraseSnake() {
  for (int i = 0; i < snakeLength; i++) {
    tft.fillRect(
      snakeX[i],
      snakeY[i],
      BLOCK_SIZE,
      BLOCK_SIZE,
      ILI9341_BLACK
    );
  }
}

// -------------------------------------------------------
// Draw the snake
// -------------------------------------------------------
void drawSnake() {
  for (int i = 0; i < snakeLength; i++) {

    // Yellow head
    if (i == 0) {
      tft.fillRect(
        snakeX[i],
        snakeY[i],
        BLOCK_SIZE,
        BLOCK_SIZE,
        ILI9341_YELLOW
      );
    }

    // Green body
    else {
      tft.fillRect(
        snakeX[i],
        snakeY[i],
        BLOCK_SIZE,
        BLOCK_SIZE,
        ILI9341_GREEN
      );
    }
  }
}

// -------------------------------------------------------
// Generate food at a random valid grid position
// -------------------------------------------------------
void generateFood() {
  do {
    foodX = random(
      0,
      SCREEN_WIDTH / BLOCK_SIZE
    ) * BLOCK_SIZE;

    foodY = random(
      PLAY_AREA_TOP / BLOCK_SIZE,
      SCREEN_HEIGHT / BLOCK_SIZE
    ) * BLOCK_SIZE;

} while (
  isFoodOnSnake() ||
  isFoodNearBarrier()
);

// Generate all red bad foods required for this level
generateBadFoods();

// Start or restart the five-second timer
resetFoodTimer();

  Serial.print("New food X: ");
  Serial.print(foodX);

  Serial.print(" | New food Y: ");
  Serial.println(foodY);
}

//---------------------------------------------------
// Identyfied th Red Food Levels
// -------------------------------------------------------

int calculateBadFoodCount() {
  /*
    Level 1-3 = no bad food
    Level 4   = one bad food
    Level 5   = two bad foods
    Level 6   = three bad foods
  */

  if (level < 4) {
    return 0;
  }

  int requiredCount = level - 3;

  if (requiredCount > MAX_BAD_FOODS) {
    requiredCount = MAX_BAD_FOODS;
  }

  return requiredCount;
}

// -------------------------------------------------------
// Generate all red bad-food positions
// -------------------------------------------------------
void generateBadFoods() {
  badFoodCount = calculateBadFoodCount();

  for (int i = 0; i < badFoodCount; i++) {
    int candidateX;
    int candidateY;

    do {
      candidateX = random(
        0,
        SCREEN_WIDTH / BLOCK_SIZE
      ) * BLOCK_SIZE;

      candidateY = random(
        PLAY_AREA_TOP / BLOCK_SIZE,
        SCREEN_HEIGHT / BLOCK_SIZE
      ) * BLOCK_SIZE;

    } while (
      isBadFoodOnSnake(candidateX, candidateY) ||
      isBadFoodNearBarrier(candidateX, candidateY) ||
      isBadFoodOnNormalFood(candidateX, candidateY) ||
      isBadFoodOnExistingBadFood(
        candidateX,
        candidateY,
        i
      )
    );

    badFoodX[i] = candidateX;
    badFoodY[i] = candidateY;

    Serial.print("Bad food ");
    Serial.print(i + 1);

    Serial.print(" X: ");
    Serial.print(badFoodX[i]);

    Serial.print(" | Y: ");
    Serial.println(badFoodY[i]);
  }
}

// -------------------------------------------------------
// Check whether a candidate red food is on the snake
// -------------------------------------------------------
bool isBadFoodOnSnake(int x, int y) {
  for (int i = 0; i < snakeLength; i++) {
    if (
      x == snakeX[i] &&
      y == snakeY[i]
    ) {
      return true;
    }
  }

  return false;
}

// -------------------------------------------------------
// Keep candidate red food away from the digit barrier
// -------------------------------------------------------
bool isBadFoodNearBarrier(int x, int y) {
  if (level < 2) {
    return false;
  }

  bool insideHorizontalArea =
      x >= BARRIER_MIN_X - BARRIER_MARGIN &&
      x <= BARRIER_MAX_X + BARRIER_MARGIN;

  bool insideVerticalArea =
      y >= BARRIER_MIN_Y - BARRIER_MARGIN &&
      y <= BARRIER_MAX_Y + BARRIER_MARGIN;

  return insideHorizontalArea &&
         insideVerticalArea;
}

// -------------------------------------------------------
// Prevent red food from overlapping normal food
// -------------------------------------------------------
bool isBadFoodOnNormalFood(int x, int y) {
  return x == foodX &&
         y == foodY;
}

// -------------------------------------------------------
// Prevent red foods from overlapping each other
// -------------------------------------------------------
bool isBadFoodOnExistingBadFood(
    int x,
    int y,
    int numberAlreadyGenerated
) {
  for (
    int i = 0;
    i < numberAlreadyGenerated;
    i++
  ) {
    if (
      x == badFoodX[i] &&
      y == badFoodY[i]
    ) {
      return true;
    }
  }

  return false;
}

// -------------------------------------------------------
// Check whether the generated food is inside the snake
// -------------------------------------------------------
bool isFoodOnSnake() {
  for (int i = 0; i < snakeLength; i++) {
    if (
      foodX == snakeX[i] &&
      foodY == snakeY[i]
    ) {
      return true;
    }
  }

  return false;
}

// -------------------------------------------------------
// Draw normal food and all red bad foods
// -------------------------------------------------------
void drawFood() {
  // Draw normal cyan food
  tft.fillRect(
    foodX + 1,
    foodY + 1,
    BLOCK_SIZE - 2,
    BLOCK_SIZE - 2,
    ILI9341_CYAN
  );

  // Draw every active red bad food
  for (int i = 0; i < badFoodCount; i++) {
    tft.fillRect(
      badFoodX[i] + 1,
      badFoodY[i] + 1,
      BLOCK_SIZE - 2,
      BLOCK_SIZE - 2,
      ILI9341_RED
    );
  }
}
// -------------------------------------------------------
// Check whether the snake ate the normal cyan food
// -------------------------------------------------------
bool hasSnakeEatenFood() {
  return snakeX[0] == foodX &&
         snakeY[0] == foodY;
}
// -------------------------------------------------------
// Erase normal food and all red bad foods
// -------------------------------------------------------
void eraseFoodItems() {
  // Erase normal food
  tft.fillRect(
    foodX,
    foodY,
    BLOCK_SIZE,
    BLOCK_SIZE,
    ILI9341_BLACK
  );

  // Erase every active red bad food
  for (int i = 0; i < badFoodCount; i++) {
    tft.fillRect(
      badFoodX[i],
      badFoodY[i],
      BLOCK_SIZE,
      BLOCK_SIZE,
      ILI9341_BLACK
    );
  }
}

// -------------------------------------------------------
// Return the index of the red food eaten by the snake
// -------------------------------------------------------
int getEatenBadFoodIndex() {
  for (int i = 0; i < badFoodCount; i++) {
    if (
      snakeX[0] == badFoodX[i] &&
      snakeY[0] == badFoodY[i]
    ) {
      return i;
    }
  }

  // No red food was eaten
  return -1;
}

// -------------------------------------------------------
// Check whether the snake head has touched its body
// -------------------------------------------------------
bool hasSnakeHitBody() {
  for (int i = 1; i < snakeLength; i++) {

    bool sameXPosition =
        snakeX[0] == snakeX[i];

    bool sameYPosition =
        snakeY[0] == snakeY[i];

    if (sameXPosition && sameYPosition) {
      Serial.print("Collision with body block: ");
      Serial.println(i);

      return true;
    }
  }

  return false;
}
void showGameOver() {
  Serial.println("GAME OVER");

  // Save a new record in EEPROM
  saveHighScore();

  // Draw the black Game Over box
  tft.fillRect(
    45,
    65,
    230,
    120,
    ILI9341_BLACK
  );

  // Draw the red border
  tft.drawRect(
    45,
    65,
    230,
    120,
    ILI9341_RED
  );

  tft.drawRect(
    46,
    66,
    228,
    118,
    ILI9341_RED
  );

  // GAME OVER text
  tft.setTextColor(ILI9341_RED);
  tft.setTextSize(3);
  tft.setCursor(70, 80);
  tft.print("GAME OVER");

  // Final score
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(100, 125);
  tft.print("Score: ");
  tft.print(score);

  // Restart instruction
  tft.setTextSize(1);
  tft.setCursor(73, 160);
tft.print("Press joystick for menu");

  previousButtonState =
      digitalRead(JOYSTICK_BUTTON);
}
// -------------------------------------------------------
void restartGame() {
  Serial.println("Restarting game...");

  // Exit Game Over mode
  gameOver = false;

  // Clear the display
  tft.fillScreen(ILI9341_BLACK);

  // Reset score, snake length, position and direction
  initialiseSnake();

  // Generate new food
  generateFood();

  // Redraw the game
drawHeader();
drawFood();
drawSnake();

playStartSound();

previousMoveTime = millis();
previousButtonState = LOW;
}
/// -------------------------------------------------------
// Calculate and activate the next level
// -------------------------------------------------------
void updateLevel() {
  int calculatedLevel = (score / 2) + 1;


  if (calculatedLevel > level) {
    int previousLevel = level;

    level = calculatedLevel;

    Serial.print("LEVEL UP! New level: ");
    Serial.println(level);

    // Activate the Level 2 barrier
    if (previousLevel < 2 && level >= 2) {
      activateLevelTwo();
    }

    // Update speed for Level 5 and above
    updateGameSpeed();

    Serial.print("Number of bad foods: ");
    Serial.println(calculateBadFoodCount());
  }
}
// -------------------------------------------------------
// Increase snake speed by 20% from Level 5 onwards
// -------------------------------------------------------
void updateGameSpeed() {
  moveInterval = BASE_MOVE_INTERVAL;

  for (int currentLevel = 5;
       currentLevel <= level;
       currentLevel++) {

    moveInterval =
        (moveInterval * 5) / 6;
  }

  // Apply a safe minimum interval
  if (moveInterval < MIN_MOVE_INTERVAL) {
    moveInterval = MIN_MOVE_INTERVAL;
  }

  Serial.print("Movement interval: ");
  Serial.print(moveInterval);
  Serial.println(" ms");
}
//
// -------------------------------------------------------
// Draw the barrier
// -------------------------------------------------------
void drawDigitBarrier() {
  if (level < 2) {
    return;
  }

  for (int i = 0; i < DIGIT_ONE_CELL_COUNT; i++) {
    tft.fillRect(
      digitOneCells[i][0],
      digitOneCells[i][1],
      BLOCK_SIZE,
      BLOCK_SIZE,
      ILI9341_WHITE
    );
  }
}

// -------------------------------------------------------
// Check whether one grid position belongs to the digit
// -------------------------------------------------------
bool isBarrierCell(int x, int y) {
  for (int i = 0; i < DIGIT_ONE_CELL_COUNT; i++) {
    if (
      x == digitOneCells[i][0] &&
      y == digitOneCells[i][1]
    ) {
      return true;
    }
  }

  return false;
}

// -------------------------------------------------------
// Check whether the snake head hit the digit
// -------------------------------------------------------
bool hasSnakeHitBarrier() {
  if (level < 2) {
    return false;
  }

  return isBarrierCell(
    snakeX[0],
    snakeY[0]
  );
}

// -------------------------------------------------------
// Prevent food from appearing on or near the digit
// -------------------------------------------------------
bool isFoodNearBarrier() {
  if (level < 2) {
    return false;
  }

  bool insideHorizontalArea =
      foodX >= BARRIER_MIN_X - BARRIER_MARGIN &&
      foodX <= BARRIER_MAX_X + BARRIER_MARGIN;

  bool insideVerticalArea =
      foodY >= BARRIER_MIN_Y - BARRIER_MARGIN &&
      foodY <= BARRIER_MAX_Y + BARRIER_MARGIN;

  return insideHorizontalArea &&
         insideVerticalArea;
}

// -------------------------------------------------------
// Check whether any snake block is near the centre digit
// -------------------------------------------------------
bool isSnakeNearBarrier() {
  for (int i = 0; i < snakeLength; i++) {

    bool insideHorizontalArea =
        snakeX[i] >= BARRIER_MIN_X - BARRIER_MARGIN &&
        snakeX[i] <= BARRIER_MAX_X + BARRIER_MARGIN;

    bool insideVerticalArea =
        snakeY[i] >= BARRIER_MIN_Y - BARRIER_MARGIN &&
        snakeY[i] <= BARRIER_MAX_Y + BARRIER_MARGIN;

    if (
      insideHorizontalArea &&
      insideVerticalArea
    ) {
      return true;
    }
  }

  return false;
}

// -------------------------------------------------------
// Move the complete snake to the top-left corner
// -------------------------------------------------------
void moveSnakeToCorner() {

  const int cornerHeadX = 70;
  const int cornerY = 40;

  for (int i = 0; i < snakeLength; i++) {
    snakeX[i] =
        cornerHeadX - (i * BLOCK_SIZE);

    snakeY[i] = cornerY;
  }

  currentDirection = RIGHT;
  requestedDirection = RIGHT;

  Serial.println(
    "Snake moved to corner before digit appeared"
  );
}

// -------------------------------------------------------
// Activate Level 2
// -------------------------------------------------------

void activateLevelTwo() {
  Serial.println("Activating Level 2 digit barrier");

  // Check whether the snake is close to the centre
  bool snakeMustMove = isSnakeNearBarrier();

  if (snakeMustMove) {
    tft.fillRect(
      0,
      PLAY_AREA_TOP,
      SCREEN_WIDTH,
      SCREEN_HEIGHT - PLAY_AREA_TOP,
      ILI9341_BLACK
    );

    moveSnakeToCorner();

  // Draw the Level 2 digit immediately
  drawDigitBarrier();

  // Redraw the snake immediately
  drawSnake();

  // Update the displayed level immediately
  updateHeader();
}
}
// -------------------------------------------------------
// Display only the countdown value
// -------------------------------------------------------
void drawCountdownValue(int seconds) {

  tft.fillRect(
    285,
    2,
    32,
    16,
    ILI9341_BLUE
  );

  tft.setTextColor(
    ILI9341_WHITE,
    ILI9341_BLUE
  );

  tft.setTextSize(1);
  tft.setCursor(285, 6);

  if (seconds < 0) {
    tft.print("-");
  } else {
    tft.print(seconds);
  }
}

// -------------------------------------------------------
// Start a new timer whenever food appears
// -------------------------------------------------------
void resetFoodTimer() {
  foodCreatedTime = millis();
  lastDisplayedCountdown = -1;

  if (level >= 3) {
    lastDisplayedCountdown = 5;
    drawCountdownValue(5);
  }
}

void updateFoodTimer() {
  // Timer is active y from Level 3 
    if (level < 3 || gameOver) {
    return;
  }

  unsigned long elapsed =
      millis() - foodCreatedTime;

  // Food has existed for five seconds
  if (elapsed >= FOOD_LIFETIME) {
  // Remove expired food items
  eraseFoodItems();

  Serial.println(
    "Food expired - generating new food"
  );

  // Generate new positions and reset the timer
  generateFood();

  // Display the new food
  drawFood();

  return;
}



 int remainingSeconds =
      (FOOD_LIFETIME - elapsed + 999) / 1000;

  if (remainingSeconds != lastDisplayedCountdown) {
    lastDisplayedCountdown = remainingSeconds;
    drawCountdownValue(remainingSeconds);
  }
}

// -------------------------------------------------------
// Draw the header background and fixed labels
// -------------------------------------------------------
void drawHeader() {

  tft.fillRect(
    0,
    0,
    SCREEN_WIDTH,
    PLAY_AREA_TOP,
    ILI9341_BLUE
  );

  tft.setTextColor(
    ILI9341_WHITE,
    ILI9341_BLUE
  );

  tft.setTextSize(1);

  tft.setCursor(5, 6);
  tft.print("Score:");

  tft.setCursor(85, 6);
  tft.print("Length:");

  tft.setCursor(180, 6);
  tft.print("Level:");

  tft.setCursor(250, 6);
  tft.print("Time:");

  updateHeader();
}
// -------------------------------------------------------
// Sound played when a new game starts
// -------------------------------------------------------
void playStartSound() {
  tone(BUZZER_PIN, 800, 80);
  delay(100);

  tone(BUZZER_PIN, 1200, 120);
  delay(140);

  noTone(BUZZER_PIN);
}

// -------------------------------------------------------
// Sound played when red bad food is eaten
// -------------------------------------------------------
void playBadFoodSound() {
  tone(BUZZER_PIN, 350, 120);
  delay(140);

  tone(BUZZER_PIN, 220, 120);
  delay(180);

  noTone(BUZZER_PIN);
}

// -------------------------------------------------------
// Sound played when food is eaten
// -------------------------------------------------------
void playFoodSound() {

  tone(BUZZER_PIN, 1500, 80);
}

// -------------------------------------------------------
// Sound played when the game is over
// -------------------------------------------------------
void playGameOverSound() {
  tone(BUZZER_PIN, 700, 100);
  delay(120);

  tone(BUZZER_PIN, 500, 130);
  delay(150);

  tone(BUZZER_PIN, 300, 180);
  delay(200);

  noTone(BUZZER_PIN);
}

// -------------------------------------------------------
// Detect one joystick-button press
// -------------------------------------------------------
bool joystickButtonPressed() {
  int currentButtonState =
      digitalRead(JOYSTICK_BUTTON);

  bool buttonPressed =
      currentButtonState == LOW &&
      previousButtonState == HIGH;

  previousButtonState = currentButtonState;

  if (buttonPressed) {
    delay(30);

    return digitalRead(JOYSTICK_BUTTON) == LOW;
  }

  return false;
}

// -------------------------------------------------------
// Load the past high score from EEPROM
// -------------------------------------------------------
void loadHighScore() {
  EEPROM.get(
    HIGH_SCORE_ADDRESS,
    savedHighScore
  );

   if (
    savedHighScore < 0 ||
    savedHighScore > 999999
  ) {
    savedHighScore = 0;

    EEPROM.put(
      HIGH_SCORE_ADDRESS,
      savedHighScore
    );

    EEPROM.commit();
  }

  highScore = savedHighScore;

  Serial.print("Loaded high score: ");
  Serial.println(highScore);
}

// -------------------------------------------------------
// Save a new high score into EEPROM
// -------------------------------------------------------
void saveHighScore() {

  if (highScore > savedHighScore) {
    EEPROM.put(
      HIGH_SCORE_ADDRESS,
      highScore
    );

    EEPROM.commit();

    savedHighScore = highScore;

    Serial.print("New high score saved: ");
    Serial.println(savedHighScore);
  }
}

// -------------------------------------------------------
// Draw the opening menu
// -------------------------------------------------------
void drawMainMenu() {
  // Clear the screen 
  tft.fillScreen(ILI9341_BLACK);

  // Blue bar
  tft.fillRect(
    0,
    0,
    SCREEN_WIDTH,
    PLAY_AREA_TOP,
    ILI9341_BLUE
  );

  tft.setTextColor(
    ILI9341_WHITE,
    ILI9341_BLUE
  );

  tft.setTextSize(1);
  tft.setCursor(125, 6);
  tft.print("SNAKE GAME");

  // Draw the current menu selection
  drawMenuOptions();

  // Navigation instruction
  tft.setTextColor(
    ILI9341_WHITE,
    ILI9341_BLACK
  );

  tft.setTextSize(1);
  tft.setCursor(64, 205);
  tft.print("Move joystick and press to select");
}

// -------------------------------------------------------
// Update only the menu options
// -------------------------------------------------------
void drawMenuOptions() {
  
  tft.fillRect(
    40,
    65,
    260,
    40,
    ILI9341_BLACK
  );

  tft.fillRect(
    40,
    110,
    260,
    40,
    ILI9341_BLACK
  );

  tft.setTextSize(2);

  // Start New Game option
  if (menuSelection == 0) {
    tft.setTextColor(
      ILI9341_YELLOW,
      ILI9341_BLACK
    );

    tft.setCursor(55, 80);
    tft.print("> START NEW GAME");
  } else {
    tft.setTextColor(
      ILI9341_WHITE,
      ILI9341_BLACK
    );

    tft.setCursor(55, 80);
    tft.print("  START NEW GAME");
  }

  // View High Score option
  if (menuSelection == 1) {
    tft.setTextColor(
      ILI9341_YELLOW,
      ILI9341_BLACK
    );

    tft.setCursor(55, 125);
    tft.print("> VIEW HIGH SCORE");
  } else {
    tft.setTextColor(
      ILI9341_WHITE,
      ILI9341_BLACK
    );

    tft.setCursor(55, 125);
    tft.print("  VIEW HIGH SCORE");
  }
}
// -------------------------------------------------------
// Read joystick while the menu is displayed
// -------------------------------------------------------

void handleMainMenu() {
  int verticalValue =
      analogRead(JOYSTICK_VERT);

  if (!menuJoystickMoved) {

    // UP always selects Start New Game
    if (verticalValue > 3000) {
      if (menuSelection != 0) {
        menuSelection = 0;

            drawMenuOptions();
      }

      menuJoystickMoved = true;
    }

    // DOWN always selects View High Score
    else if (verticalValue < 1000) {
      if (menuSelection != 1) {
        menuSelection = 1;

        // Update only the menu options
        drawMenuOptions();
      }

      menuJoystickMoved = true;
    }
  }

  // Allow another movement after joystick returns to centre
  if (
    verticalValue > 1500 &&
    verticalValue < 2500
  ) {
    menuJoystickMoved = false;
  }

  // Select the option
  if (joystickButtonPressed()) {
    if (menuSelection == 0) {
      startNewGame();
    } else {
      currentScreen = HIGH_SCORE_SCREEN;

      drawHighScoreScreen();

      previousButtonState = LOW;
    }
  }
}

// -------------------------------------------------------
// Show countdown before starting a new game
// -------------------------------------------------------
void showStartCountdown() {
  tft.fillScreen(ILI9341_BLACK);

  tft.fillRect(
    0,
    0,
    SCREEN_WIDTH,
    PLAY_AREA_TOP,
    ILI9341_BLUE
  );

  tft.setTextColor(
    ILI9341_WHITE,
    ILI9341_BLUE
  );

  tft.setTextSize(1);
  tft.setCursor(130, 6);
  tft.print("GET READY");

  // Set the countdown 
  tft.setTextSize(7);

  // -------------------- Number 3 --------------------
  tft.setTextColor(
    ILI9341_WHITE,
    ILI9341_BLACK
  );

  tft.setCursor(140, 80);
  tft.print("3");

  delay(800);

  // Clear countdown-number area
  tft.fillRect(
    110,
    55,
    100,
    120,
    ILI9341_BLACK
  );

  // -------------------- Number 2 --------------------
  tft.setCursor(140, 80);
  tft.print("2");

  delay(800);

  tft.fillRect(
    110,
    55,
    100,
    120,
    ILI9341_BLACK
  );

  // -------------------- Number 1 --------------------
  tft.setTextColor(
    ILI9341_YELLOW,
    ILI9341_BLACK
  );

  tft.setCursor(140, 80);
  tft.print("1");

  // Play the existing game-start sound 
  playStartSound();

  delay(500);

   tft.fillScreen(ILI9341_BLACK);
}

// -------------------------------------------------------
// Start a completely new game
// -------------------------------------------------------
void startNewGame() {
  Serial.println("Starting new game...");

  currentScreen = GAME_SCREEN;
  gameOver = false;

  // Reset snake, score, level and speed
  initialiseSnake();

  // Keep the high score loaded from EEPROM
  highScore = savedHighScore;

  // Show 3, 2, 1 before gameplay begins
  showStartCountdown();

    //Generate food after the countdown.

    
  generateFood();

  // Draw the initial gameplay screen
  drawHeader();
  drawFood();
  drawSnake();

  // Start movement timing after the countdown
  previousMoveTime = millis();

  previousButtonState =
      digitalRead(JOYSTICK_BUTTON);

  Serial.println("Countdown finished - game started");
}
// -------------------------------------------------------
// Draw the separate high-score screen
// -------------------------------------------------------
void drawHighScoreScreen() {
  tft.fillScreen(ILI9341_BLACK);

    //Displays  past high score,
  
  tft.fillRect(
    0,
    0,
    SCREEN_WIDTH,
    PLAY_AREA_TOP,
    ILI9341_BLUE
  );

  tft.setTextColor(
    ILI9341_WHITE,
    ILI9341_BLUE
  );

  tft.setTextSize(1);
  tft.setCursor(95, 6);

  tft.print("PAST HIGH SCORE: ");
  tft.print(highScore);

  // Return instruction
  tft.setTextColor(
    ILI9341_WHITE,
    ILI9341_BLACK
  );

  tft.setTextSize(2);
  tft.setCursor(38, 100);
  tft.print("HIGH SCORE SAVED");

  tft.setTextSize(1);
  tft.setCursor(75, 205);
  tft.print("Press joystick to return");
}

// -------------------------------------------------------
// Handle the high-score screen
// -------------------------------------------------------
void handleHighScoreScreen() {
  if (joystickButtonPressed()) {
    currentScreen = MENU_SCREEN;
    menuSelection = 0;

    drawMainMenu();

    previousButtonState = LOW;
  }
}
// -------------------------------------------------------
// Update only the changing header values
// -------------------------------------------------------
void updateHeader() {
  // Score value Indicate
  tft.fillRect(
    43,
    2,
    35,
    16,
    ILI9341_BLUE
  );

  // Snake-length
  tft.fillRect(
    130,
    2,
    40,
    16,
    ILI9341_BLUE
  );

  // Level Indicate
  tft.fillRect(
    220,
    2,
    25,
    16,
    ILI9341_BLUE
  );

  tft.setTextColor(
    ILI9341_WHITE,
    ILI9341_BLUE
  );

  tft.setTextSize(1);

  tft.setCursor(43, 6);
  tft.print(score);

  tft.setCursor(130, 6);
  tft.print(snakeLength);

  tft.setCursor(220, 6);
  tft.print(level);

  if (level >= 3) {
    unsigned long elapsed =
        millis() - foodCreatedTime;

    int remainingSeconds = 0;

    if (elapsed < FOOD_LIFETIME) {
      remainingSeconds =
          (FOOD_LIFETIME - elapsed + 999) / 1000;
    }

    drawCountdownValue(remainingSeconds);
  } else {
    drawCountdownValue(-1);
  }
}
