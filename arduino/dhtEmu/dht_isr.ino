// ============================================================
// ISR INT0 (PD2 = D2) - EMULASI DHT11
// BAGIAN DATA / SAVE-RESTORE = C (aman, mudah debug).
// BAGIAN TIMING KRITIS (RESPONSE + 40 BIT) = INLINE ASM MURNI
//   dengan cycle-counted 16-cycle NOP loop = 1us TEPAT.
// Alasan: Overhead loop C for + compare + branch GCC 7.3 menambah
//   ~20us BAWAH per pulse, sehingga 80us menjadi 100us.
//   Pure inline asm menghilangkan SEMUA overhead itu.
// ============================================================
ISR(INT0_vect) {
    // ===== A. Persiapan data dari global (C, aman dari timing issue) =====
    uint8_t hI = g_hum_int;
    uint8_t hD = g_hum_dec;
    uint8_t tI = g_temp_int;
    uint8_t tD = g_temp_dec;
    uint8_t cks_ok = g_cksum_mode;
    uint8_t cks = (uint8_t)(hI + hD + tI + tD);
    if (!cks_ok) cks ^= 0xFF;

    // Susun 5 byte DHT11 ORDER BENAR:
    // [0]=hum_int, [1]=hum_dec, [2]=temp_int, [3]=temp_dec, [4]=checksum
    uint8_t bytes[5];
    bytes[0] = hI;
    bytes[1] = hD;
    bytes[2] = tI;
    bytes[3] = tD;
    bytes[4] = cks;

    // ===== B. SAVE & DISABLE SEMUA INTERRUPT (TIMING PRESISI) =====
    uint8_t sreg_saved = SREG;
    cli();
    uint8_t eimsk_saved  = EIMSK;
    uint8_t timsk0_saved = TIMSK0;
    uint8_t timsk1_saved = TIMSK1;
    uint8_t timsk2_saved = TIMSK2;
    uint8_t adcsra_saved = ADCSRA;
    uint8_t ucsr0b_saved = UCSR0B;
    EIMSK  = 0;
    TIMSK0 = 0;
    TIMSK1 = 0;
    TIMSK2 = 0;
    ADCSRA &= ~(1 << ADIE);
    UCSR0B &= ~((1 << RXCIE0) | (1 << TXCIE0) | (1 << UDRIE0));
    EIFR = (1 << INTF0);     // clear flag pending start master sebelumnya

    // ===== C+D+E+F: SEMUA TIMING KRITIS DALAM 1 BLOK ASM SAJA (supaya label _d_us ter-resolve) =====
    //   + TAMBAHAN END MARKER LOW 53µs SESUAI MODUL ASLI CAPTURE LA USER.
    uint8_t *bytes_ptr = bytes;
    __asm__ __volatile__(
        /* ==========================================================
           STEP C: TUNGGU MASTER MELEPAS LINE (PD2 -> HIGH)
           ========================================================== */
        "_wait_high_merged:     \n\t"
        "sbic %[_pind], 2       \n\t"  // PIND.2 LOW ? Jika LOW, skip rjmp = lanjut loop.
        "rjmp _master_high_ok   \n\t"  // PIND.2 HIGH -> lanjut step response
        "rjmp _wait_high_merged \n\t"
        "_master_high_ok:       \n\t"
        "ldi  r24, 30           \n\t"  // Spec DHT11: 20-40uS tunggu setelah master release
        "rcall _d_us            \n\t"

        /* ==========================================================
           SETUP Z pointer ke array 5 byte data & aktifkan output driver
           ========================================================== */
        "movw r30, %a[_bp]      \n\t"  // Z = bytes_ptr (r25:r24)
        "sbi  %[_ddrd], 2       \n\t"  // DDRD.2 = OUTPUT aktif (push-pull)

        /* ==========================================================
           STEP D: RESPONSE PULSE SPEC DHT11 ASLI LOW 80µs -> HIGH 80µs
           ========================================================== */
        "cbi  %[_portd], 2      \n\t"  // LOW 80µs
        "ldi  r24, 80           \n\t"
        "rcall _d_us            \n\t"
        "sbi  %[_portd], 2      \n\t"  // HIGH 80µs
        "ldi  r24, 80           \n\t"
        "rcall _d_us            \n\t"

        /* ==========================================================
           STEP E: KIRIM 40 BIT DATA (5 byte × 8 bit, MSB FIRST)
           ========================================================== */
        "ldi  r25, 5            \n\t"  // counter 5 byte
        "_next_byte_merged:     \n\t"
        "ld   r18, Z+           \n\t"  // load byte[i], Z++
        "ldi  r17, 8            \n\t"  // counter 8 bit / byte (IMMEDIATE = tidak korup)
        "_next_bit_merged:      \n\t"
        "lsl  r18               \n\t"  // LSL = logical shift left (carry luar TIDAK MASUK). bit7 -> C
        /* --- AWAL BIT: LOW 50µs SELALU --- */
        "cbi  %[_portd], 2      \n\t"
        "ldi  r24, 50           \n\t"
        "rcall _d_us            \n\t"
        /* --- HIGH 28µs (bit 0) atau 70µs (bit 1) --- */
        "sbi  %[_portd], 2      \n\t"
        "brcc _is_bit0_merged   \n\t"  // carry clear = bit 0
        /* bit 1 HIGH 70µs */
        "ldi  r24, 70           \n\t"
        "rcall _d_us            \n\t"
        "rjmp _bit_done_merged  \n\t"
        "_is_bit0_merged:       \n\t"
        /* bit 0 HIGH 28µs (dinaikkan 2µs dari 26, tidak borderline threshold master) */
        "ldi  r24, 28           \n\t"
        "rcall _d_us            \n\t"
        "_bit_done_merged:      \n\t"
        "dec  r17               \n\t"
        "brne _next_bit_merged  \n\t"  // hingga 8 bit/byte
        "dec  r25               \n\t"
        "brne _next_byte_merged \n\t"  // hingga 5 byte = 40 BIT TEPAT!

        /* ==========================================================
           STEP E.5: === END MARKER LOW 53µs SAMA MODUL ASLI DHT11 ===
           Sesuai permintaan user dari capture LA (cursor 53µs LOW label "End" ungu).
           Setelah 40 bit terakhir HIGH pulse selesai, keluarkan LOW 53µs.
           Master library DHT mengenali ini sebagai tanda END OF FRAME.
           ========================================================== */
        "cbi  %[_portd], 2      \n\t"  // AKHIR: LOW = mulai END MARKER
        "ldi  r24, 53           \n\t"
        "rcall _d_us            \n\t"  // 53µs PERSIS LOW = sesuai capture hardware asli
        "sbi  %[_portd], 2      \n\t"  // Kembali HIGH setelah end marker
        "ldi  r24, 5            \n\t"  // Settling 5µs sebelum Hi-Z (hindari glitch)
        "rcall _d_us            \n\t"

        /* ==========================================================
           STEP F: RELEASE PD2 -> INPUT Hi-Z tanpa internal pull-up
           ========================================================== */
        "cbi  %[_ddrd], 2       \n\t"  // DDRD.2 = INPUT (HIGH-Z)
        "cbi  %[_portd], 2      \n\t"  // PORTD.2 = 0 → NO INTERNAL PULL-UP, pakai external 10K
        "rjmp _end_asm_merged   \n\t"

        /* ==========================================================
           SUBROUTINE _d_us: delay r24 × 1µs @16MHz = 16 CYCLE TEPAT per loop
           ========================================================== */
        "_d_us:                 \n\t"
        "tst  r24               \n\t"
        "breq _d_us_end         \n\t"
        "_d_us_lp:              \n\t"
        "ldi  r16, 3            \n\t"  // 1c
        "_d_sublp:              \n\t"
        "dec  r16               \n\t"
        "brne _d_sublp          \n\t"  // subtotal 8c
        "nop                    \n\t"  // 1c
        "nop                    \n\t"  // 1c
        "nop                    \n\t"  // 1c
        "nop                    \n\t"  // 1c
        "dec  r24               \n\t"  // 1c
        "brne _d_us_lp          \n\t"  // 2c taken => 1+8+4+1+2 = 16C / iterasi TEPAT
        "_d_us_end:             \n\t"
        "ret                    \n\t"

        "_end_asm_merged:       \n\t"
        : /* no output operands */
        : [_bp]    "e" (bytes_ptr),
          [_ddrd]  "I" (_SFR_IO_ADDR(DDRD)),
          [_portd] "I" (_SFR_IO_ADDR(PORTD)),
          [_pind]  "I" (_SFR_IO_ADDR(PIND))
        : "r16","r17","r18","r24","r25","r30","r31","memory"
    );

    // ===== G. CLEAR INT0 FLAG yang di-generate output drive sendiri =====
    EIFR = (1 << INTF0);
    __asm__ __volatile__("nop");
    EIFR = (1 << INTF0);

    // ===== H. RESTORE SEMUA INTERRUPT MASK =====
    EIMSK  = eimsk_saved;
    TIMSK0 = timsk0_saved;
    TIMSK1 = timsk1_saved;
    TIMSK2 = timsk2_saved;
    ADCSRA = adcsra_saved;
    UCSR0B = ucsr0b_saved;
    SREG   = sreg_saved;
    // RETI akhir ISR otomatis enable global interrupt.
}
