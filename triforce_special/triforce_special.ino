#include <FastLED.h>
#include <MIDIUSB.h>
#define WINDOW_SIZE 128
#define LOG_WINDOW_SIZE 7
uint16_t window[WINDOW_SIZE];

uint8_t window_ptr = 0;
uint16_t analog_sample;
uint16_t rect_sample;
uint32_t sum = 0;
uint32_t counter = 0;
uint16_t avg = 0;
int on_col = 0;
bool forward = true;
/* Upside down trees
uint8_t bitmap[32] = 
{0b00000000,
 0b01000000,
 0b01101000,
 0b01011100,
 0b11001010,
 0b00000001,
 0b11001010,
 0b01011100,
 0b01101000,
 0b01000000,
 0b00000000,
 0b01000000,
 0b01101000,
 0b01011100,
 0b11001010,
 0b00000001,
 0b11001010,
 0b01011100,
 0b01101000,
 0b01000000,
 0b00000000,
 0b01000000,
 0b01101000,
 0b01011100,
 0b11001010,
 0b00000001,
 0b11001010,
 0b01011100,
 0b01101000,
 0b01000000,
 0b00000000,
 0b00000000};
 */

uint8_t dk_symbols[32] = 
{0b00000000,
 0b00000000,
 0b00000000,
 0b00001000,
 0b01101000,
 0b10011111,
 0b01101000,
 0b00001000,
 0b00000000,
 0b00000000,
 0b00110010,
 0b01001110,
 0b01000000,
 0b01001110,
 0b00110010,
 0b00000000,
 0b00000000,
 0b00011000,
 0b00100100,
 0b00010010,
 0b00100100,
 0b00011000,
 0b00000000,
 0b00000000,
 0b00111100,
 0b01101110,
 0b01000010,
 0b01101110,
 0b00111100,
 0b00000000,
 0b00000000,
 0b00000000};

 uint8_t coffins[32] = 
{0b00000000,
 0b00111100,
 0b01000010,
 0b01000010,
 0b10011001,
 0b10111101,
 0b10111101,
 0b10011001,
 0b01011010,
 0b01000010,
 0b01000010,
 0b00100100,
 0b00100100,
 0b00100100,
 0b00011000,
 0b00000000,
 0b00000000,
 0b00011000,
 0b00100100,
 0b00100100,
 0b00100100,
 0b01000010,
 0b01000010,
 0b01011010,
 0b10011001,
 0b10111101,
 0b10111101,
 0b10011001,
 0b01000010,
 0b01000010,
 0b00111100,
 0b00000000};


#define grid_8_32

#ifdef strip_60
#define NUM_ROWS 1
#define NUM_COLS 60
#define NUM_LEDS 60
#endif

#ifdef grid_8_32
#define NUM_ROWS 8
#define NUM_COLS 32
#define NUM_LEDS 256
#endif

CRGB leds[NUM_LEDS];
//uint8_t scroll_vals[NUM_LEDS/2];
uint8_t scroll_vals[NUM_COLS/2];
uint8_t scroll_ptr = 0;

// USB MIDI
uint8_t midi_pgm = 2;
uint8_t midi_color1_r = 0;
uint8_t midi_color1_g = 0;
uint8_t midi_color1_b = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  for (int i = 0; i < WINDOW_SIZE; i++){
    window[i] = 0;
  }
  scroll_ptr = 0;
  for (int i = 0; i < NUM_COLS/2; i++){
    if (i % 4 == 0){
      scroll_vals[i] = 200;
    } else {
      scroll_vals[i] = 0;
    }
  }

  sum = 0;
  FastLED.addLeds<WS2812, 3, GRB>(leds, NUM_LEDS);
}


void loop() {
  // put your main code here, to run repeatedly:
  
  // Read and filter analog audio signal
  analog_sample = analogRead(2);
  if (analog_sample > 524) {
    rect_sample = (analog_sample - 524);
  } else {
    rect_sample = (524 - analog_sample);
  }
  sum += rect_sample;
  sum -= window[window_ptr % WINDOW_SIZE];
  window[window_ptr % WINDOW_SIZE] = rect_sample;
  avg = (sum >> LOG_WINDOW_SIZE) & 0x0FFF;
  //float average = sum / 16.0;
  window_ptr++;
  //Serial.print("z:");
  //Serial.write( 0xff );                                                         
  //Serial.write( (analog_sample >> 8) & 0xff );                                            
  //Serial.write( analog_sample & 0xff );
  //Serial.println(analog_sample);

  // Read any new MIDI messages
  midiEventPacket_t rx;
  do {
    rx = MidiUSB.read();
    if (rx.header != 0) {
      //Serial.print("Received: ");
      //Serial.print(rx.header, HEX);
      //Serial.print("-");
      //Serial.print(rx.byte1, HEX);
      //Serial.print("-");
      //Serial.print(rx.byte2, HEX);
      if (rx.header == 0x0C && rx.byte1 == 0xC0){
        midi_pgm = rx.byte2;
      }
      // Color CCs
      if (rx.header == 0x0B && rx.byte1 == 0xB0) {
        if (rx.byte2 == 0x0E) {
          midi_color1_r = rx.byte3 << 1;
        } else if (rx.byte2 == 0x0F) {
          midi_color1_g = rx.byte3 << 1;
        } else if (rx.byte2 == 0x10) {
          midi_color1_b = rx.byte3 << 1;
        }
      }
      Serial.print("-");
      Serial.println(rx.byte3, HEX);
    }
  } while (rx.header != 0);

  counter++;
  if (counter % 32 == 0){
     #ifdef strip_60
    // Simple VU meter
    /*
    for (int i = 0; i < NUM_LEDS; ++i){
      if (i < avg ){
        leds[i] = CRGB(avg, i, 0);
      } else {
        leds[i] = CRGB(0, 0, 0);
      }
    }
    */
    // VU meter from center
    /*
    for (int i = 0; i < NUM_LEDS/2; ++i){
      if (((NUM_LEDS/2) - i) > avg) {
        leds[i] = CRGB(0, 0, 0);
        leds[NUM_LEDS - i - 1] = CRGB(0, 0, 0);
      } else {
        leds[i] = CRGB(avg, ((NUM_LEDS/2) - i)/2, 0);
        leds[NUM_LEDS - i - 1] = CRGB(avg, ((NUM_LEDS/2) - i)/4, 0);
      }
    }
    */
    
    
    // Scrolling from center
    /*
    if (avg > 50) {
      scroll_vals[scroll_ptr] = avg;
    } else {
        scroll_vals[scroll_ptr] = 0;
    }
    for (uint8_t i = 0; i < (NUM_LEDS/2); i++){
      leds[i] = CRGB(scroll_vals[(scroll_ptr + i) % (NUM_LEDS/2)], 0, 0);
      leds[NUM_LEDS - i - 1] = CRGB(scroll_vals[(scroll_ptr + i) % (NUM_LEDS/2)], 0, 0);   
    }  
    if (scroll_ptr == (NUM_LEDS/2 - 1)){
        scroll_ptr = 0;
      } else {
        scroll_ptr++;
    }  
    */
    #endif

    
    
    
    // O-scope
    /*
    for (int i = 0; i < NUM_LEDS/8; i++){
      uint16_t sample_val = window[i];
      for (int j = 0; j < 8; j++){
        if ((sample_val > j*32) && (sample_val < (j+1)*32)) {
          if (i%2 == 0){
            leds[i*8 + (7-j)] = CRGB(128, 0, 0);
          } else {
            leds[i*8 + j] = CRGB(128, 0, 0);
          }
        } else {
          if (i%2 == 0){
            leds[i*8 + (7-j)] = CRGB(0, 0, 0);
          } else {
            leds[i*8 + j] = CRGB(0, 0, 0);
          }
        }
      }
    }
    */

    if (midi_pgm == 0){
      // black out
      for (int i = 0; i < NUM_LEDS; i++){
        leds[i] = CRGB(0, 0, 0);
      }
    } else if (midi_pgm == 1) {
      // Scrolling graph from center
      for (int i = 0; i < NUM_COLS/2; i++){
        for (int j = 0; j < NUM_ROWS; j++){
          uint8_t led_index;
          uint8_t led_index2;
          if (i%2 == 0){
            led_index = i*NUM_ROWS + ((NUM_ROWS-1)-j);
            led_index2 = (NUM_LEDS - 1) - led_index;
          } else {
            led_index = i*NUM_ROWS + j;
            led_index2 = (NUM_LEDS - 1) - led_index;
          }
          //if (scroll_vals[(scroll_ptr + i) % (NUM_COLS/2)] > j*32 && scroll_vals[(scroll_ptr + i) % (NUM_COLS/2)] < (j+1)*32){
            /*
            leds[led_index] = CRGB(avg/2, 0, avg/2 + (NUM_COLS/2 - i));
            leds[led_index2] = CRGB(avg/2, 0, avg/2 + (NUM_COLS/2 - i));
            */
            leds[led_index] = CHSV(midi_color1_r + i, 255, 50 + avg/2);
            leds[led_index2] = CHSV(midi_color1_r + i, 255, 50 + avg/2);
          //} else {
            //leds[led_index] = CRGB(0, 0, 0);       
            //leds[led_index2] = CRGB(0, 0, 0);  
          //}
        }
      }
      scroll_vals[scroll_ptr] = avg;
      if (scroll_ptr == (NUM_COLS/2 - 1)){
        scroll_ptr = 0;
      } else {
        scroll_ptr++;
      }
    } else if (midi_pgm == 2){
      
      
      // Scrolling color bar from center
      
      for (int i = 0; i < NUM_COLS/2; i++){
        uint8_t scroll_val = scroll_vals[(scroll_ptr + i) % (NUM_COLS/2)];
        for (int j = 0; j < NUM_ROWS; j++){
          uint8_t led_index = i*NUM_ROWS + j;
          uint8_t led_index2 = (NUM_LEDS - 1) - led_index;
          //leds[led_index] = CRGB(scroll_val, 0, 10);
          //leds[led_index2] = CRGB(scroll_val, 0, 10);
          leds[led_index] = CHSV(midi_color1_r + (scroll_val/4), 255, 30 + scroll_val/2);
          leds[led_index2] = CHSV(midi_color1_r + (scroll_val/4), 255, 30 + scroll_val/2);
        }
      }
      scroll_vals[scroll_ptr] = avg;
      if (scroll_ptr == (NUM_COLS/2 - 1)){
        scroll_ptr = 0;
      } else {
        scroll_ptr++;
      }  
    } else if (midi_pgm == 3 || midi_pgm == 6){
      // Display bitmap
      uint8_t* bitmap;
      if (midi_pgm == 3){
        bitmap = dk_symbols;
      } else if (midi_pgm == 6){
        bitmap = coffins;
      }
      for (int i = 0; i < NUM_LEDS; i++){
        CRGB color = CRGB(avg/255.0*128, avg/255.0*0, avg/255.0*128);
        if ((i/NUM_ROWS)%2 == 0){
          if ((bitmap[i/8] & (0x80 >> (i%8))) != 0){
            leds[i] = color;
          } else {
            leds[i] = CRGB(0, 0, 0);
          }
        } else {
          if ((bitmap[i/8] & (0x01 << (i%8))) != 0){
            leds[i] = color;
          } else {
            leds[i] = CRGB(0, 0, 0);
          }        
        }
      }
    } else if (midi_pgm == 4){
      // Random noise
      for (int i = 0; i < NUM_LEDS; i++){
        leds[i] = CHSV(midi_color1_r, midi_color1_g, (random8() >> 2)*(1 + avg/32));
      }
    } else if (midi_pgm == 5) {
      // Bouncing sine wave
      for (int i = 0; i < NUM_COLS; i++){
        for (int j = 0; j < NUM_ROWS; j++){
          uint8_t led_index;
          uint8_t led_index2;
          if (i%2 == 0){
            led_index = i*NUM_ROWS + ((NUM_ROWS-1)-j);
            led_index2 = (NUM_LEDS - 1) - led_index;
          } else {
            led_index = i*NUM_ROWS + j;
            led_index2 = (NUM_LEDS - 1) - led_index;
          }
          if (on_col == i && (((50+avg/4)*sin(i/2.0) + 128) >= j*32) && (((50+avg/4)*sin(i/2.0) + 128)  < (j+1)*32)) {
            
            leds[led_index] = CRGB(avg, 0, 128);
            //leds[led_index2] = CRGB(avg/2, 0, avg/2 + (NUM_COLS/2 - i));
          } else {
            leds[led_index] = CRGB(0, 0, 0);       
            //leds[led_index2] = CRGB(0, 0, 0);  
          }
        }
      }
      if (forward){
        if (on_col == NUM_COLS - 1){
          forward = false;
        } else {
          on_col++;
        }
      } else {
        if (on_col == 0) {
          forward = true;
        } else {
          on_col--;
        }
      }
    }
    FastLED.show();
  }
}
