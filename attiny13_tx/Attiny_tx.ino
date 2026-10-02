#ifndef F_CPU
#define F_CPU 9600000UL // 9.6 MHz Takt
#endif

#include <avr/io.h>
#include <util/delay.h>

#define CAR_ID 2   // Auto-ID (1 bis 63)
#define IR_PIN PB3 // Pin 2 am ATtiny13

static uint16_t lfsr = 0xACE1u;

uint16_t getRandomNumber(void) {
  uint16_t bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1u;
  lfsr = (lfsr >> 1) | (bit << 15);
  return lfsr;
}

// 38 kHz Trägersignal (350 µs Impulse für TSOP4838)
void ir_mark(uint16_t time_us) {
  uint16_t cycles = time_us / 26;
  while (cycles--) {
    PORTB |= (1 << IR_PIN);
    _delay_us(13);
    PORTB &= ~(1 << IR_PIN);
    _delay_us(13);
  }
}

void ir_space(uint16_t time_us) {
  PORTB &= ~(1 << IR_PIN);
  while (time_us >= 100) {
    _delay_us(100);
    time_us -= 100;
  }
  while (time_us >= 10) {
    _delay_us(10);
    time_us -= 10;
  }
}

void delay_ms_variable(uint16_t ms) {
  while (ms--) {
    _delay_ms(1);
  }
}

// Ultra-kurzes 12-Bit Frame (~16 ms Dauer)
void send_micro_frame(uint16_t frame) {
  // Leader: 2.0 ms Puls + 1.0 ms Pause
  ir_mark(2000);
  ir_space(1000);

  // 12 Bits senden (MSB zuerst)
  for (int8_t i = 11; i >= 0; i--) {
    ir_mark(350);
    if (frame & (1U << i)) {
      ir_space(1050); // Bit '1' (~1.4 ms)
    } else {
      ir_space(350);  // Bit '0' (~0.7 ms)
    }
  }

  // Stop-Bit
  ir_mark(350);
  ir_space(350);
}

int main(void) {
  // Ungenutzte Pins auf Pull-Up (Strom sparen)
  DDRB &= ~((1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB4));
  PORTB |= (1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB4);

  DDRB |= (1 << IR_PIN);
  PORTB &= ~(1 << IR_PIN);

  lfsr ^= ((uint16_t)CAR_ID << 8) | ((uint16_t)CAR_ID * 0x45u);

  // 6 Bit ID + 6 Bit invertierte ID
  uint8_t id = CAR_ID & 0x3F;
  uint8_t id_inv = (~id) & 0x3F;
  uint16_t frame_12bit = ((uint16_t)id << 6) | id_inv;

  while (1) {
    send_micro_frame(frame_12bit);

    // Entzerrungs-Pause: 15 bis 30 ms
    uint16_t randomDelay = (getRandomNumber() % 16) + 15;
    delay_ms_variable(randomDelay);
  }

  return 0;
}