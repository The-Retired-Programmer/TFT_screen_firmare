/*
 Derived from original Hobbytronics Arduino TFT Serial Driver
*/

#include "Adafruit_ST7735.h" // Hardware-specific library
#include <Fonts/TomThumb.h>
#include <Fonts/Picopixel.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans24pt7b.h>

#define TFT_CS  10          // Chip select line for TFT display
#define TFT_DC   8          // Data/command line for TFT
#define TFT_RST  NULL       // Reset line for TFT (or connect to +5V)
#define lcdBacklight  7     // 7 on Serial board, 9 on TFT Shield

#define COMMAND_START 0x7B
#define COMMAND_END 0x7D
#define SCREEN_WIDTH 160
#define SCREEN_HEIGHT 128

unsigned char x_pos=0;
unsigned char y_pos=0;
unsigned char text_size=1;
boolean readingcommand = false;
unsigned char rotation=0; 
uint16_t foreground=ST7735_WHITE;
uint16_t background=ST7735_BLACK;

uint16_t colours[8] = {
    ST7735_BLACK,
    ST7735_BLUE,
    ST7735_RED,
    ST7735_GREEN,
    ST7735_CYAN,
    ST7735_MAGENTA,
    ST7735_YELLOW,
    ST7735_WHITE
};
//
unsigned char inputString[255];         // a string to hold incoming data
int inputStringIndex = 0;

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

void setup(void) {
  pinMode(TFT_CS, OUTPUT);
  tft.initR(INITR_BLACKTAB);   // initialize a ST7735R chip, black and blue tab 
  tft.setFont(&TomThumb); 
  pinMode(lcdBacklight, OUTPUT);
  digitalWrite(lcdBacklight, HIGH);
  tft.fillScreen(background);
  Serial.begin(9600);
}

struct cmddef {
    char* command;
    void (*commandfunction)();
    int paramcount;
    bool (*paramcheckfunction)();
};

struct cmddef  commands[] ={
    "close-bracket", *print_closebracket, 0, *params_none,
    "text-colour", *tft_set_fg_color, 1,*params_1colour,
    "background-colour", *tft_set_bg_color, 1,*params_1colour,
    "orienataion", *tft_rotation, 1, *params_1orientation,
    "font", *tft_font, 1, *params_1font,
    "goto-startofline", *tft_bol,0, *params_none,
    "goto-text", *tft_text_goto,2, *params_2int,
    "goto-pixel", *tft_pix_goto,2, *params_2int,
    "line", *tft_draw_line,5, *params_5colour,
    "box", *tft_draw_box,5, *params_5colour,
    "filled-box", *tft_fill_box,5, *params_5colour,
    "circle", *tft_draw_circle,4, *params_4colour,
    "filled-circle", *tft_fill_circle,4, *params_4colour,
    "clear-screen", *tft_clear, 0, *params_none,
    "txt", *tft_text ,1, *params_none
};

#define NUM_COMMANDS (sizeof commands / sizeof commands[0])

#define MAXPCOUNT 6
char* params[MAXPCOUNT];
int16_t p_int[MAXPCOUNT];
uint16_t p_colour[MAXPCOUNT];
uint8_t paramscount;
#define BADINT 65535

bool parse() {
    char* cp = inputString;
    paramscount = 0;
    params[paramscount] = inputString;
    while (true) {
        switch (*cp) {
        case ',':
            *cp = '\0';
            if (paramscount >= MAXPCOUNT - 1)  return false;
            cp++;
            params[++paramscount] = cp;
            break;
        case '\0':
            return true;
        default:
            cp++;
        }
    }
}

bool params_none() {
    return true;
}

bool params_toint(int p_no) {
    p_int[p_no] = to_int(params[p_no]);
    return p_int[p_no] != BADINT;
}

bool params_1colour() {
    return params_1int() && param_colour(1);
}

bool params_1orientation() {
    return params_1int() && param_orientation(1);
}

bool params_1font() {
    return params_1int() && param_font(1);
}

bool params_1int() {
    return params_toint(1);
}

bool params_2int() {
    return params_1int() && params_toint(2);
}

bool params_3int() {
    return params_2int() && params_toint(3);
}

bool params_4int() {
    return params_3int() && params_toint(4);
}

bool params_4colour() {
    return params_4int() && param_colour(4);
}

bool params_5int() {
    return params_4int() && params_toint(5);
}

bool params_5colour() {
    return params_5int() && param_colour(5);
}

boolean param_font(int p_no) {
    return (p_int[p_no] >=1) && (p_int[p_no] <=4);
}

boolean param_orientation(int p_no) {
    return (p_int[p_no] >=0) && (p_int[p_no] <=3);
}

boolean param_colour(int p_no) {
    return (p_int[p_no] >=0) && (p_int[p_no] <=7) && selectcolour(p_no);
}

boolean selectcolour(int p_no) {
    p_colour[p_no] = colours[p_int[p_no]];
    return true;
}

uint16_t to_int(char* string) {
    uint16_t val = 0;
    while (*string) {
        if ( (*string >= '0' ) && (*string <= '9') ) {
            val = val*10+(*string++-'0');
        } else {
            return BADINT; // bad integer
        }
    }
    return val;
}

boolean sequals(byte* s1, byte* s2) {
    do {
        if (*s1 != *s2++) return false;
    } while (*s1++ != '\0');
    return true;
}

void executecommand() {
    for (int i=0; i< NUM_COMMANDS; i++) {
        if(sequals(commands[i].command,params[0])
                && commands[i].paramcount == paramscount
                && (*commands[i].paramcheckfunction)() ){
            unsigned long stime = micros();
            (*commands[i].commandfunction)();
            unsigned long ftime = micros();
            docommandpace('+', ftime-stime);
            return;
        }
    }
    docommandpace('-',0);
}

void docommandpace(char prefix, unsigned long interval) {
//    retain for speed testing only
//    Serial.print(prefix);
//    Serial.print(params[0]);
//    Serial.print(" - ");
//    Serial.println(interval);
    Serial.println(prefix);
}

void tft_text() {
    tft.print(params[1]);
}

void loop() {
 
}

void serialEvent() {
  while (Serial.available()) 
  {
    // get the new byte:
    int inChar = Serial.read()&0x7F; 
    if(readingcommand) {
      if (inChar == COMMAND_END) {
        readingcommand=false;
        inputString[inputStringIndex] = '\0';
        if (parse()){
            executecommand();
        }
        inputString[0] = '\0';
        inputStringIndex = 0;
      } else inputString[inputStringIndex++] = inChar;
    } else {    
      if (inChar == COMMAND_START) readingcommand = true;
    }  
  }
}

void print_closebracket() {
    tft.print('}');
}

void tft_clear()
{
  tft.fillScreen(background);
  x_pos=0;
  y_pos=0;
  tft.setCursor(x_pos, y_pos);
}    

void tft_font()
{
    switch(p_int[1]) {
    case 1:
        tft.setFont(&TomThumb);
        break;
    case 2:
        tft.setFont(&Picopixel);
        break;
    case 3:
        tft.setFont(&FreeSans9pt7b);
        break;
    case 4:
        tft.setFont(&FreeSans24pt7b);
    }
}

void tft_set_fg_color()
{
    foreground=p_colour[1];
    tft.setTextColor(foreground, background);  
}

void tft_set_bg_color()
{
    background=p_colour[1];
    tft.setTextColor(foreground, background);
}

void tft_bol()
{
  x_pos=0;
  tft.setCursor(x_pos, y_pos);
} 

void tft_text_goto()
{
    // Goto text X,Y
        x_pos=(text_size*6)*(p_int[1]);
        y_pos=(text_size*8)*(p_int[2]);
        tft.setCursor(x_pos, y_pos);
} 

void tft_pix_goto()
{
        if((((rotation==1) || (rotation==3)) & ((p_int[1]<SCREEN_WIDTH) & (p_int[2]<SCREEN_HEIGHT))) ||
        (((rotation==0) || (rotation==2)) & ((p_int[1]<SCREEN_HEIGHT) & (p_int[2]<SCREEN_WIDTH))))
        {
            x_pos=p_int[1];
            y_pos=p_int[2];  
            tft.setCursor(x_pos, y_pos);
        }
}

void tft_draw_line()
{
  tft.drawLine(p_int[1], p_int[2], p_int[3], p_int[4], p_colour[5]);
}

void tft_draw_box()
{
  tft.drawRect(p_int[1], p_int[2], p_int[3] - p_int[1] + 1, p_int[4] - p_int[2] + 1, p_colour[5]);
}

void tft_fill_box()
{
  tft.fillRect(p_int[1], p_int[2], p_int[3] - p_int[1] + 1, p_int[4] - p_int[2] + 1, p_colour[5]); 
}

void tft_draw_circle()
{
  tft.drawCircle(p_int[1], p_int[2], p_int[3], p_colour[4]);
}

void tft_fill_circle()
{
  tft.fillCircle(p_int[1], p_int[2], p_int[3], p_colour[4]);
}

void tft_rotation() {
      rotation=p_int[1];
      tft.setRotation((int16_t)p_int[1]);
}
