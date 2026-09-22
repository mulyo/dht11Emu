// ============================================================
// Emulator DHT11 - FILE UTAMA (MAIN)
// Board : Arduino Nano ATmega328P 16MHz, TANPA LIBRARY APAPUN
// ============================================================
// File Modularisasi (arsitektur file sketch Arduino IDE:
// Semua file .ino dalam folder sketch DIGABUNGKAN (concatenate)
// OTOMATIS sebelum kompilasi, jadi tidak perlu header .h.
// ============================================================
// File tab lain di sketch folder ini:
//   ✅ emu2_oled.ino      -> Driver OLED + I2C bit-bang
//   ✅ emu2_dht_isr.ino  -> ISR INT0 emulator DHT11 ASM
// ============================================================
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

// ---------- PIN DEFINISI ----------
#define POT_PIN      0    // A0 - Potensiometer
#define SW_MODE_PIN  1    // A1 - Mode (LOW = suhu / kelembapan
#define SW_CKSUM_PIN 2    // A2 - Checksum (LOW = benar, HIGH = XOR 0xFF)
#define SDA_PIN      4    // A4 - SDA OLED SSD1306
#define SCL_PIN      5    // A5 - SCL OLED SSD1306
#define OLED_ADDR    0x3C
#define DHT_PIN      2    // D2 / PD2 = INT0 = Output emulator DHT11

// ---------- GLOBAL VARIABLES (bisa diakses semua file .ino)
volatile uint8_t g_hum_int     = 50;   // 0-100 %RH
volatile uint8_t g_hum_dec     = 0;    // Selalu 0
volatile uint8_t g_temp_int    = 25;   // 0-50 °C
volatile uint8_t g_temp_dec    = 0;    // Selalu 0
volatile uint8_t g_cksum_mode  = 1;    // 1 = OK, 0 = ERR (XOR 0xFF)
volatile uint8_t g_mode        = 0;    // 0 = Potensio -> Kelembapan; 1 -> Suhu
volatile uint32_t g_last_isr_time = 0;  // Debounce 100ms antar INT0
uint32_t g_last_oled_update      = 0;   // Update OLED tiap 200ms
#define OLED_UPDATE_MS 200

// ============================================================
// FORWARD DECLARATIONS (didefinisikan di file tab lain)
// ============================================================
static void     adc_init(void);
static uint16_t adc_read(uint8_t ch);
static void     oled_init(void);
static void     oled_clear(void);
static void     oled_set_pos(uint8_t page, uint8_t col);
static void     oled_putc(char ch);
static void     oled_puts(const char *s);
static void     oled_put2(uint8_t v);
static uint32_t get_millis(void);

// ============================================================
// SETUP
// ============================================================
void setup() {
    // Pin switch INPUT_PULLUP
    DDRC  &= ~((1 << SW_MODE_PIN) | (1 << SW_CKSUM_PIN));
    PORTC |=  ((1 << SW_MODE_PIN) | (1 << SW_CKSUM_PIN));

    // D2 (PD2) = INPUT, NO PULL-UP (eksternal 10K yang handle)
    DDRD  &= ~(1 << DHT_PIN);
    PORTD &= ~(1 << DHT_PIN);

    // Timer0 untuk millis() sudah di-init oleh Arduino core.

    // ADC
    adc_init();

    // I2C pin state awal HIGH
    DDRC  &= ~((1 << SDA_PIN) | (1 << SCL_PIN));
    PORTC |=  ((1 << SDA_PIN) | (1 << SCL_PIN));
    _delay_ms(50);
    oled_init();

    // INT0 = falling edge
    EICRA = (1 << ISC01);  // ISC01=1, ISC00=0 => falling edge
    EIMSK = (1 << INT0);

    sei();
}

// ============================================================
// LOOP UTAMA
// ============================================================
// Helper: debounce sederhana untuk switch
static uint8_t sw_debounce(uint8_t pin) {
    // Return 1 = LOW (tertekan), 0 = HIGH (lepas)
    // sample 3 kali dengan jeda 2ms
    uint8_t cnt = 0;
    for (uint8_t i = 0; i < 3; i++) {
        if (!(PINC & (1 << pin))) cnt++;
        _delay_ms(2);
    }
    return (cnt >= 2) ? 1 : 0;
}

void loop() {
    // 1. Baca potensiometer (0..1023)
    uint16_t pot = adc_read(POT_PIN);

    // 2. Baca switch mode A1
    uint8_t sw_mode = sw_debounce(SW_MODE_PIN); // 1=ditekan(LOW) => suhu
    g_mode = sw_mode; // 0=kelembapan (HIGH/pull-up), 1=suhu (LOW/tekan)

    if (g_mode == 1) {
        // Mode suhu: 0-50 °C
        uint8_t val = (uint8_t)((uint32_t)pot * 50 / 1023);
        if (val > 50) val = 50;
        g_temp_int = val;
        // g_hum_int tetap (disimpan global)
    } else {
        // Mode kelembapan: 0-100 %RH
        uint8_t val = (uint8_t)((uint32_t)pot * 100 / 1023);
        if (val > 100) val = 100;
        g_hum_int = val;
        // g_temp_int tetap
    }

    // 3. Baca switch checksum A2
    uint8_t sw_cksum = sw_debounce(SW_CKSUM_PIN); // 1=ditekan(LOW) => benar
    g_cksum_mode = sw_cksum; // 1=OK, 0=ERR

    // 4. Debounce ISR INT0: cek & clear jika terlalu dekat (<100 ms)
    // Karena flag INT0 di-set meskipun masked, kita lakukan clear manual
    // jika last ISR masih baru.
    static uint8_t s_isr_enabled = 1;
    uint32_t now = get_millis();
    if (s_isr_enabled) {
        // Tidak perlu aksi khusus. Setelah ISR fired, kita disable sementara.
        // Tapi karena ISR_NAKED tidak otomatis, kita lakukan polling flag.
    }
    // Alternatif: setelah INT0 ter-trigger, software-disable 100ms
    if (EIFR & (1 << INTF0)) {
        // Fall edge baru saja terdeteksi (ISR akan / baru jalan)
        // Disable INT0 100 ms untuk debounce antar-request
        EIMSK &= ~(1 << INT0);
        g_last_isr_time = now;
        s_isr_enabled = 0;
        // Clear flag
        EIFR = (1 << INTF0);
    }
    if (!s_isr_enabled && (now - g_last_isr_time) >= 100) {
        EIMSK |= (1 << INT0);
        s_isr_enabled = 1;
    }

    // 5. Update OLED setiap 200 ms. === PARTIAL UPDATE HANYA BARIS BERUBAH SAJA ===
    // Cache state 4 baris (nilai sebelumnya disimpan di static.
    // Init = 0xFF = invalid (mustahil nilai asli DHT (suhu<51, hum<101)
    static uint8_t prev_mode = 0xFF;
    static uint8_t prev_temp = 0xFF;
    static uint8_t prev_hum  = 0xFF;
    static uint8_t prev_cks  = 0xFF;

    if (now - g_last_oled_update >= OLED_UPDATE_MS) {
        g_last_oled_update = now;

        // BACA global volatiles SEKALI SAJA di awal (atomic, hindari perubahan tengah compare)
        uint8_t cur_mode = g_mode;
        uint8_t cur_temp = g_temp_int;
        uint8_t cur_hum  = g_hum_int;
        uint8_t cur_cks  = g_cksum_mode;

        // -------------------------------------------------
        // BARIS 1 (page 0) → MODE : Cek perubahan mode
        // -------------------------------------------------
        oled_set_pos(0, 0);
        if (cur_mode == 1) oled_puts("Mode: Suhu       ");
        else               oled_puts("Mode: Kelembapan");

        // -------------------------------------------------
        // BARIS 2 (page 2) → SUHU : Cek perubahan g_temp_int
        // Catatan: perubahan SUHU juga terjadi jika potensio diubah di MODE SUHU.
        //          Nilai lama tertinggal jika tidak di update, harus selalu compare.
        // -------------------------------------------------
        if (cur_temp != prev_temp) {
            prev_temp = cur_temp;
            oled_set_pos(2, 0);
            oled_puts("Suhu: ");
            oled_put2(cur_temp);
            oled_puts(" C");
        }

        // -------------------------------------------------
        // BARIS 3 (page 4) → HUM : Cek perubahan g_hum_int
        // -------------------------------------------------
        if (cur_hum != prev_hum) {
            prev_hum = cur_hum;
            oled_set_pos(4, 0);
            oled_puts("Hum : ");
            oled_put2(cur_hum);
            oled_puts(" %");
        }

        // -------------------------------------------------
        // BARIS 4 (page 6) → CHECKSUM : CKSUM OK/ERROR PRESS SW2
        // -------------------------------------------------
        if (cur_cks != prev_cks) {
            prev_cks = cur_cks;
            oled_set_pos(6, 0);
            oled_puts("Cksum: ");
            if (cur_cks) oled_puts("OK ");
            else          oled_puts("ERR");
        }
    }
}
