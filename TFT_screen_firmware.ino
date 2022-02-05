/*

 
 Derived from original Hobbytronics Arduino TFT Serial Driver
  
*/

#define INCLUDE_IMAGE true

#include <Adafruit_GFX.h>    // Core graphics library
#include "Adafruit_ST7735.h" // Hardware-specific library



#define TFT_CS  10          // Chip select line for TFT display
#define TFT_DC   8          // Data/command line for TFT
#define TFT_RST  NULL       // Reset line for TFT (or connect to +5V)
#define lcdBacklight  7     // 7 on Serial board, 9 on TFT Shield

#ifdef INCLUDE_IMAGE
#include <SPI.h>
#include <SD.h>
#define SD_CS    4          // Chip select line for SD card
unsigned char sd_card=0;       // SD Card inserted?
#endif

#define MODE_COMMAND 1
#define MODE_TEXT    0
#define COMMAND_START 0x7B
#define COMMAND_END 0x7D
#define SCREEN_WIDTH 160
#define SCREEN_HEIGHT 128

unsigned char x_pos=0;
unsigned char y_pos=0;
unsigned char text_size=2;
unsigned char mode=MODE_TEXT;
unsigned char rotation=3;              // default Landscape
unsigned int foreground=ST7735_WHITE;
unsigned int background=ST7735_BLACK;
unsigned char bl_brightness=100;

unsigned int colours[8] = {
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
unsigned char inputString[40];         // a string to hold incoming data
int inputStringIndex = 0;
unsigned long currentTime;
unsigned long blTime;

int timer1_counter;

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

void setup(void) {
  pinMode(TFT_CS, OUTPUT);

  
  // If your TFT's plastic wrap has a Red Tab, use the following:
  //tft.initR(INITR_REDTAB);   // initialize a ST7735R chip, red tab
  // If your TFT's plastic wrap has a Green Tab, use the following:
  //tft.initR(INITR_GREENTAB); // initialize a ST7735R chip, green tab
  // If your TFT's plastic wrap has a Black Tab, use the following:
  tft.initR(INITR_BLACKTAB);   // initialize a ST7735R chip, black and blue tab  
  
  tft.setRotation(rotation);          // Set to landscape mode
  //analogWrite(lcdBacklight, 255);   // Turn Backlight on full
  pinMode(lcdBacklight, OUTPUT);
  digitalWrite(lcdBacklight, HIGH);
  
#ifdef INCLUDE_IMAGE
  pinMode(SD_CS, OUTPUT);
  // Check for SD Card
  if (!SD.begin(SD_CS)) 
  {
    // No SD card, or failed to initialise card
    sd_card=0;
    // Arduino SD library does something to SPI speed when it fails
    // So need to reset otherwise screen is slow.
    SPI.setClockDivider(SPI_CLOCK_DIV4); // 16/4 MHz
  }  
  else sd_card=1;
#endif
  blTime = micros();  
  tftInit();
  
  Serial.begin(9600);

  // initialize timer1 
  noInterrupts();           // disable all interrupts
  TCCR1A = 0;
  TCCR1B = 0;

  // Set timer1_counter to the correct value for our interrupt interval
  timer1_counter = 65474;   // 3200Hz
  
  TCNT1 = timer1_counter;   // preload timer
  TCCR1B |= (1 << CS11);    // 8 prescaler 
  TIMSK1 |= (1 << TOIE1);   // enable timer overflow interrupt
  interrupts();             // enable all interrupts
}

ISR(TIMER1_OVF_vect)        // interrupt service routine - used for backlight
{
  // AnalogWrite not available on pin 7 (oops), so we modulate LCD backlight manually at 32Hz
  TCNT1 = timer1_counter;   // preload timer
  currentTime = micros();
  if(currentTime >= (blTime + ((unsigned long)bl_brightness)*32)){ 
     digitalWrite(lcdBacklight, LOW);
     if(currentTime >= (blTime + 3200)){
        digitalWrite(lcdBacklight, HIGH);
        blTime = micros();
     }  
  } 
}

struct cmddef {
    int command;
    void (*commandfunction)();
    int paramcount;
    bool (*paramcheckfunction)();
};

struct cmddef  commands[] ={
    0, *tft_clear, 0, *params_none,
    1, *tft_set_fg_color, 1,*params_1colour,
    2, *tft_set_bg_color, 1,*params_1colour,
    3, *tft_rotation, 1, *params_1orientation,
    4, *tft_fontsize, 1, *params_1fontsize,
    5, *tft_bol,0, *params_none,
    6, *tft_text_goto,2, *params_2int,
    7, *tft_pix_goto,2, *params_2int,
    8, *tft_draw_line,4, *params_4int,
    9, *tft_draw_box,4, *params_4int,
    10, *tft_fill_box,4, *params_4int,
    11, *tft_draw_circle,3, *params_3int,
    12, *tft_fill_circle,3, *params_3int,
    13, *tft_bitmap, 3, *params_2int,
    14, *tft_backlight, 1 ,*params_1int
};

#define NUM_COMMANDS (sizeof commands / sizeof commands[0])

#define MAXPCOUNT 4
char* params[MAXPCOUNT];
uint8_t p_int[MAXPCOUNT];
unsigned int p_colour[MAXPCOUNT];
uint8_t paramscount; 

bool parse() {
    char* cp = inputString;
    paramscount = 0;
    params[paramscount] = inputString;
    while (true) {
        switch (*cp) {
        case ',':
            *cp = '\0';
            p_int[paramscount] = to_int(params[paramscount]);
            if (paramscount >= MAXPCOUNT - 1)  return false;
            cp++;
            params[++paramscount] = cp;
            break;
        case '\0':
            p_int[paramscount] = to_int(params[paramscount]);
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
    return true;
}

bool params_1colour() {
    return params_1int() && param_colour(1);
}

bool params_1orientation() {
    return params_1int() && param_orientation(1);
}

bool params_1fontsize() {
    return params_1int() && param_fontsize(1);
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

boolean param_fontsize(int p_no) {
    return (p_int[p_no] >=1) && (p_int[p_no] <=3);
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

uint8_t to_int(char* string) {
    uint8_t val = 0;
    while (*string) {
        if ( (*string >= '0' ) && (*string <= '9') ) {
            val = val*10+(*string++-'0');
        } else {
            return 0; // bad integer
        }
    }
    return val;
}

void executecommand() {
    for (int i=0; i< NUM_COMMANDS; i++) {
        if(commands[i].command == p_int[0]
                && commands[i].paramcount == paramscount
                && (*commands[i].paramcheckfunction)() ){
            (*commands[i].commandfunction)();
            return;
        }
    }
}

void loop() {
 
}

/*
  SerialEvent occurs whenever a new data comes in the
 hardware serial RX.  This routine is run between each
 time loop() runs, so using delay inside loop can delay
 response.  Multiple bytes of data may be available.
 
 NOTE: Arduino UNO buffer size is 64 bytes
 */
void serialEvent() {
  while (Serial.available()) 
  {
    // get the new byte:
    int inChar = Serial.read()&0x7F; 
    if(mode==MODE_TEXT)
    {    
      if (inChar == COMMAND_START) 
      {
        //COMMAND char - command mode until COMMAND_END
        mode=MODE_COMMAND;
      }
      else if (inChar == '\r') 
      {
        x_pos=0;
        y_pos+=(text_size*8);
        tft.setCursor(x_pos, y_pos);      
      }    
      else if (inChar == '\n') 
      {
        //ignore
      }     
      else
      {
        tft.print((char)inChar);
        x_pos+=(text_size*6);
      } 
    }  
    else
    {
      // in COMMAND MODE
      if (inChar == COMMAND_END)
      {
        // End of command received - validate and run command
        
        inputString[inputStringIndex] = '\0';

        if (parse()){
            executecommand();
 
        }
        inputString[0] = '\0';
        inputStringIndex = 0;
        mode=MODE_TEXT;    // Back to Text mode   
      }    
      else
      {
        // Command mode -accumulate command
        inputString[inputStringIndex] = inChar;
        inputStringIndex++;
      }      
    }  
     
    if((((rotation==1) || (rotation==3)) & (x_pos>=(SCREEN_WIDTH-(text_size*6)))) ||
       (((rotation==0) || (rotation==2)) & (x_pos>=(SCREEN_HEIGHT-(text_size*6)))) )
    {
      //can't fit next char on screen - wrap
      x_pos=0;
      y_pos+=(text_size*8);
      tft.setCursor(x_pos, y_pos);
    }      
  }
}

void tftInit()
{
  // Clear screen
  tft.setTextWrap(false);
  tft.fillScreen(ST7735_BLACK);
  tft.setTextSize(2);
  
  tft.setCursor(x_pos, y_pos);
  tft.setTextColor(foreground, background);
}

void tft_clear()
{
  tft.fillScreen(background);
  x_pos=0;
  y_pos=0;
  tft.setCursor(x_pos, y_pos);
}    

void tft_fontsize()
{
    text_size=p_int[1];
    tft.setTextSize(text_size);
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
  // Goto beginning of line
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
  // Goto pixel position
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
  // Draw Line, from X1,Y1 to X2,Y2
  tft.drawLine(p_int[1], p_int[2], p_int[3], p_int[4], foreground);
}

void tft_draw_box()
{
  // Draw Box, from X1,Y1 to X2,Y2
  tft.drawRect(p_int[1], p_int[2], p_int[3] - p_int[1] + 1, p_int[4] - p_int[2] + 1, foreground);
}

void tft_fill_box()
{
  // Draw Box, from X1,Y1 to X2,Y2 and fill it with colour
  tft.fillRect(p_int[1], p_int[2], p_int[3] - p_int[1] + 1, p_int[4] - p_int[2] + 1, foreground); 
}

void tft_draw_circle()
{
  // Draw circle at x, y, radius
  tft.drawCircle(p_int[1], p_int[2], p_int[3], foreground);
}

void tft_fill_circle()
{
  // Draw circle at x, y, radius and fill
  tft.fillCircle(p_int[1], p_int[2], p_int[3], foreground);
}

void tft_rotation() {
      rotation=p_int[1];
      tft.setRotation((int16_t)p_int[1]);
}

void tft_backlight()
{
  // Set backlight
  bl_brightness = p_int[1]>= 100 ? 100 : p_int[1];
} 

void tft_bitmap(void)
{
  // display bitmap
  if(sd_card==1)
  {
     bmpDraw(params[3], p_int[1], p_int[2]);
  }
}

// This function opens a Windows Bitmap (BMP) file and
// displays it at the given coordinates.  It's sped up
// by reading many pixels worth of data at a time
// (rather than pixel by pixel).  Increasing the buffer
// size takes more of the Arduino's precious RAM but
// makes loading a little faster.  20 pixels seems a
// good balance.

#define BUFFPIXEL 10

void bmpDraw(char *filename, uint8_t x, uint8_t y) {

  File     bmpFile;
  int      bmpWidth, bmpHeight;   // W+H in pixels
  uint8_t  bmpDepth;              // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t rowSize;               // Not always = bmpWidth; may have padding
  uint8_t  sdbuffer[3*BUFFPIXEL]; // pixel buffer (R+G+B per pixel)
  uint8_t  buffidx = sizeof(sdbuffer); // Current position in sdbuffer
  boolean  goodBmp = false;       // Set to true on valid header parse
  boolean  flip    = true;        // BMP is stored bottom-to-top
  int      w, h, row, col;
  uint8_t  r, g, b;
  uint32_t pos = 0;

  if((x >= tft.width()) || (y >= tft.height())) return;

  noInterrupts();   // disable interrupts
  
  // Open requested file on SD card
  if ((bmpFile = SD.open(filename)) == NULL) {
    tft.print("File not found (");
    tft.print(filename);
    tft.println(")");  
    interrupts();     
    return;
  }
  
  // Parse BMP header
  if(read16(bmpFile) == 0x4D42) { // BMP signature
    (void)read32(bmpFile); // Read and ignore file size
    (void)read32(bmpFile); // Read & ignore creator bytes
    bmpImageoffset = read32(bmpFile); // Start of image data
    // Read DIB header
    (void)read32(bmpFile); // Ignore header
    bmpWidth  = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    if(read16(bmpFile) == 1) { // # planes -- must be '1'
      bmpDepth = read16(bmpFile); // bits per pixel
      if((bmpDepth == 24) && (read32(bmpFile) == 0)) { // 0 = uncompressed
        interrupts(); 
        goodBmp = true; // Supported BMP format -- proceed!
        
        // BMP rows are padded (if needed) to 4-byte boundary
        rowSize = (bmpWidth * 3 + 3) & ~3;

        // If bmpHeight is negative, image is in top-down order.
        // This is not canon but has been observed in the wild.
        if(bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip      = false;
        }

        // Crop area to be loaded
        w = bmpWidth;
        h = bmpHeight;
        if((x+w-1) >= tft.width())  w = tft.width()  - x;
        if((y+h-1) >= tft.height()) h = tft.height() - y;

        // Set TFT address window to clipped image bounds
        tft.setAddrWindow(x, y, x+w-1, y+h-1);

        for (row=0; row<h; row++) { // For each scanline...

          // Seek to start of scan line.  It might seem labor-
          // intensive to be doing this on every line, but this
          // method covers a lot of gritty details like cropping
          // and scanline padding.  Also, the seek only takes
          // place if the file position actually needs to change
          // (avoids a lot of cluster math in SD library).
          if(flip) // Bitmap is stored bottom-to-top order (normal BMP)
            pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
          else     // Bitmap is stored top-to-bottom
            pos = bmpImageoffset + row * rowSize;
          if(bmpFile.position() != pos) { // Need seek?
            bmpFile.seek(pos);
            buffidx = sizeof(sdbuffer); // Force buffer reload
          }

          for (col=0; col<w; col++) { // For each pixel...
            // Time to read more pixel data?
            if (buffidx >= sizeof(sdbuffer)) { // Indeed
              bmpFile.read(sdbuffer, sizeof(sdbuffer));
              buffidx = 0; // Set index to beginning
            }

            // Convert pixel from BMP to TFT format, push to display
            b = sdbuffer[buffidx++];
            g = sdbuffer[buffidx++];
            r = sdbuffer[buffidx++];
            tft.pushColor(tft.color565(r,g,b));
          } // end pixel
        } // end scanline
      } // end goodBmp
    }
  }

  bmpFile.close();
  if(!goodBmp) tft.println("BMP error.");
  
  interrupts(); 
}

// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t read16(File f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(File f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}
