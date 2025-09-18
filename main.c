#include <msp430.h>
#include <stdio.h>
#include <string.h>

#define MCLK_FREQ_MHZ 4 // MCLK = 4MHz
#define BUFFER_SIZE 5

unsigned char response[BUFFER_SIZE];
char rez[128];
unsigned int i = 0;
volatile unsigned int showTemperature = 0;
volatile unsigned int showHumidity = 0;
volatile unsigned int delayTime = 5000; // Default delay time in milliseconds

void initClock();
void initGPIO();
void initUART();
void initTB0();
void startDHT11();
void sendUART(const char *data);
void Software_Trim();
void us_sleep(unsigned int us_delay);
void ms_sleep(unsigned int ms_delay);
unsigned int dht_read(unsigned char *response);

void main(void) {
    WDTCTL = WDTPW | WDTHOLD; // Stop watchdog timer
    initClock();
    initGPIO();
    initUART();
    initTB0();
    PM5CTL0 &= ~LOCKLPM5;

    __bis_SR_register(GIE); // Enable global interrupts

    while (1) {
        sendUART("Starting DHT11 read...\r\n");
        const unsigned int error = dht_read(response);
        if (!error) {
            if (showTemperature) {
                snprintf(rez, sizeof rez, "Temperature: %d.%dC\r\n", response[3], response[4]);
                showTemperature = 0; // Reset the flag
            } else if (showHumidity) {
                snprintf(rez, sizeof rez, "Humidity: %d.%d%%\r\n", response[1], response[2]);
                showHumidity = 0; // Reset the flag
            }
        } else {
            snprintf(rez, sizeof rez, "Couldn't read data from DHT11! Error: %d\r\n", error);
        }
        sendUART(rez);
        ms_sleep(delayTime); // Delay between reads
    }
}

void initClock() {
    __bis_SR_register(SCG0); // Disable FLL
    CSCTL3 = SELREF__REFOCLK; // Set REFO as FLL reference source
    CSCTL1 = DCOFTRIMEN_1 | DCOFTRIM0 | DCOFTRIM1 | DCORSEL_2;
    CSCTL2 = FLLD_0 + 121; // 121 = 4.000.000 / 32768
    __delay_cycles(3);
    __bic_SR_register(SCG0); // Enable FLL
    Software_Trim(); // Software Trim to get the best DCOFTRIM value
    CSCTL4 = SELMS__DCOCLKDIV | SELA__REFOCLK;
    CSCTL5 = DIVM_0 | DIVS_0;

    // SMCLK - 4 MHz
    P3DIR |= BIT4;
    P3SEL0 |= BIT4;
    P3SEL1 &= ~BIT4;

    // MCLK - 4 MHz
    P3DIR |= BIT0;
    P3SEL0 |= BIT0;
    P3SEL1 &= ~BIT0;
}

void initGPIO() {
    // Configure UART pins
    P4SEL0 |= BIT2 | BIT3; // Select UART functionality for P4.2 (Rx) and P4.3 (Tx)
    P1DIR |= BIT0; // Set P1.0 as output for LED
    P1OUT &= ~BIT0; // Set P1.0 to low

    P1DIR |= BIT3; // Set P1.3 as output for DHT11 data line
    P1OUT |= BIT3; // Ensure the line is high initially

    P1REN |= BIT3; // Enable pull-up resistor on P1.3
    P1OUT |= BIT3; // Set pull-up resistor to active
    P4DIR &= ~BIT1; // Set P4.1 as input
    P4REN |= BIT1;  // Enable pull-up resistor on P4.1
    P4OUT |= BIT1;  // Set pull-up resistor to active
    P4IES |= BIT1;  // Interrupt on falling edge
    P4IE |= BIT1;   // Enable interrupt on P4.1

    P2DIR &= ~BIT3; // Set P2.3 as input
    P2REN |= BIT3;  // Enable pull-up resistor on P2.3
    P2OUT |= BIT3;  // Set pull-up resistor to active
    P2IES |= BIT3;  // Interrupt on falling edge
    P2IE |= BIT3;   // Enable interrupt on P2.3

    PM5CTL0 &= ~LOCKLPM5; // Disable the GPIO power-on default high-impedance mode
}

void initUART() {
    // Configure UART
    UCA1CTLW0 |= UCSWRST; // Put eUSCI in reset
    UCA1CTLW0 |= UCSSEL__SMCLK; // Set SMCLK as BRCLK

    // Baud Rate calculation for 38400bps with 4MHz SMCLK
    UCA1BR0 = 6; // 4000000 / 38400 ≈ 104.17 -> UCBR = 6
    UCA1BR1 = 0x00;
    UCA1MCTLW = 0x2081; // UCBRSx = 0x20, UCBRFx = 8, UCOS16 = 1

    UCA1CTLW0 &= ~UCSWRST; // Initialize eUSCI
    UCA1IE |= UCRXIE; // Enable USCI_A1 RX interrupt
}

void initTB0() {
    // Set Timer_B0 to continuous mode
    TB0CTL = TBSSEL__SMCLK | MC__CONTINUOUS | TBCLR | ID_2; // SMCLK, continuous mode, clear TBR
}

void startDHT11() {
    // Start signal for DHT11
    P1REN &= ~BIT3;
    P1DIR |= BIT3; // Set P1.3 as output
    P1OUT &= ~BIT3; // Pull the line low
    ms_sleep(18); // Wait for at least 18ms
    P1REN |= BIT3;
    P1OUT |= BIT3; // Pull the line high
    P1DIR &= ~BIT3; // Set P1.3 as input
}

unsigned int dht_read(unsigned char *response) {
    const unsigned char *end = response + BUFFER_SIZE;
    unsigned int bit_pos = 1;
    unsigned int start, stop;

    for (i = 0; i < BUFFER_SIZE; i++) response[i] = 0;

    startDHT11();

    start = TB0R;
    // Response
    while (P1IN & BIT3) if (TB0R - start > 100) return 1;
    stop = TB0R;
    do {
        start = stop;
        // Time low
        while (!(P1IN & BIT3)) if (TB0R - start > 100) return 2;
        // Time high
        while (P1IN & BIT3) if (TB0R - start > 200) return 3;
        stop = TB0R;
        if (stop - start > 110) *response |= bit_pos;
        if (!(bit_pos >>= 1)) {
            bit_pos = 0x80;
            response++;
        }
    } while (response < end);

    response -= BUFFER_SIZE;
    if (response[0] != 1) return 4;
    return 0;
}

void sendUART(const char *data) {
    while (*data) {
        while (!(UCA1IFG & UCTXIFG)); // Wait until the buffer is empty
        UCA1TXBUF = *data++; // Send the current character and move to the next one
    }
}

void us_sleep(const unsigned int us_delay) {
    const unsigned int start = TB0R;
    while (TB0R - start < us_delay);
}

void ms_sleep(const unsigned int ms_delay) {
    for (i = 0; i < ms_delay; i++) us_sleep(1000);
}

void Software_Trim() {
    unsigned int oldDcoTap = 0xffff;
    unsigned int newDcoTap = 0xffff;
    unsigned int newDcoDelta = 0xffff;
    unsigned int bestDcoDelta = 0xffff;
    unsigned int csCtl0Copy = 0;
    unsigned int csCtl1Copy = 0;
    unsigned int csCtl0Read = 0;
    unsigned int csCtl1Read = 0;
    unsigned int dcoFreqTrim = 3;
    unsigned char endLoop = 0;

    do {
        CSCTL0 = 0x100; // DCO Tap = 256
        do {
            CSCTL7 &= ~DCOFFG; // Clear DCO fault flag
        } while (CSCTL7 & DCOFFG); // Test DCO fault flag

        __delay_cycles((unsigned int)3000 * MCLK_FREQ_MHZ); // Wait FLL lock status (FLLUNLOCK) to be stable
                                                           // Suggest to wait 24 cycles of divided FLL reference clock
        while ((CSCTL7 & (FLLUNLOCK0 | FLLUNLOCK1)) && ((CSCTL7 & DCOFFG) == 0));

        csCtl0Read = CSCTL0; // Read CSCTL0
        csCtl1Read = CSCTL1; // Read CSCTL1

        oldDcoTap = newDcoTap; // Record DCOTAP value of last time
        newDcoTap = csCtl0Read & 0x01ff; // Get DCOTAP value of this time
        dcoFreqTrim = (csCtl1Read & 0x0070) >> 4; // Get DCOFTRIM value

        if (newDcoTap < 256) { // DCOTAP < 256
            newDcoDelta = 256 - newDcoTap; // Delta value between DCPTAP and 256
            if ((oldDcoTap != 0xffff) && (oldDcoTap >= 256)) // DCOTAP cross 256
                endLoop = 1; // Stop while loop
            else {
                dcoFreqTrim--;
                CSCTL1 = (csCtl1Read & (~DCOFTRIM)) | (dcoFreqTrim << 4);
            }
        } else { // DCOTAP >= 256
            newDcoDelta = newDcoTap - 256; // Delta value between DCPTAP and 256
            if (oldDcoTap < 256) // DCOTAP cross 256
                endLoop = 1; // Stop while loop
            else {
                dcoFreqTrim++;
                CSCTL1 = (csCtl1Read & (~DCOFTRIM)) | (dcoFreqTrim << 4);
            }
        }

        if (newDcoDelta < bestDcoDelta) { // Record DCOTAP closest to 256
            csCtl0Copy = csCtl0Read;
            csCtl1Copy = csCtl1Read;
            bestDcoDelta = newDcoDelta;
        }

    } while (endLoop == 0); // Poll until endLoop == 1

    CSCTL0 = csCtl0Copy; // Reload locked DCOTAP
    CSCTL1 = csCtl1Copy; // Reload locked DCOFTRIM
    while (CSCTL7 & (FLLUNLOCK0 | FLLUNLOCK1)); // Poll until FLL is locked
}

// UART ISR
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=USCI_A1_VECTOR
__interrupt void USCI_A1_ISR(void) {
#elif defined(__GNUC__)
void __attribute__ ((interrupt(USCI_A1_VECTOR))) USCI_A1_ISR(void) {
#else
#error Compiler not supported!
#endif
    static char cmd[3] = {0}; // Buffer to store received command
    static unsigned int cmdIndex = 0;

    switch (__even_in_range(UCA1IV, USCI_UART_UCTXCPTIFG)) {
        case USCI_NONE: break;
        case USCI_UART_UCRXIFG:
            while (!(UCA1IFG & UCTXIFG));
            char receivedChar = UCA1RXBUF;
            UCA1TXBUF = receivedChar;


            if (cmdIndex < sizeof(cmd) - 1) {
                cmd[cmdIndex++] = receivedChar;
                cmd[cmdIndex] = '\0'; // Null-terminate the string

                if (cmdIndex == 2) {
                    if (strcmp(cmd, "t1") == 0) {
                        delayTime = 1000;
                        sendUART("Delay set to 1 second\r\n");
                    } else if (strcmp(cmd, "t2") == 0) {
                        delayTime = 2000;
                        sendUART("Delay set to 2 seconds\r\n");
                    }
                    cmdIndex = 0;
                }
            }
            break;
        case USCI_UART_UCTXIFG: break;
        case USCI_UART_UCSTTIFG: break;
        case USCI_UART_UCTXCPTIFG: break;
    }
}

// Port 2 ISR
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=PORT2_VECTOR
__interrupt void Port_2(void) {
#elif defined(__GNUC__)
void __attribute__ ((interrupt(PORT2_VECTOR))) Port_2(void) {
#else
#error Compiler not supported!
#endif
    if (P2IFG & BIT3) {
        showHumidity = 1;
        P2IFG &= ~BIT3; // Clear interrupt flag
    }
}

// Port 4 ISR
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=PORT4_VECTOR
__interrupt void Port_4(void) {
#elif defined(__GNUC__)
void __attribute__ ((interrupt(PORT4_VECTOR))) Port_4(void) {
#else
#error Compiler not supported!
#endif
    if (P4IFG & BIT1) {
        showTemperature = 1;
        P4IFG &= ~BIT1; // Clear interrupt flag
    }
}
