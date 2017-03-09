// C8051F381_ADC_multiple_inputs.c:  Shows how to use the 10-bit ADC and the
// multiplexer.  This program measures the voltages applied to pins P2.0 to P2.3.
//
// (c) 2008-2014, Jesus Calvino-Fraga
//
// ~C51~ 

#include <stdio.h>
#include <stdlib.h>
#include <c8051f38x.h>

#define MHZ 1000000L
#define SYSCLK (12*MHZ)
#define BAUDRATE 115200L


////define LCD PORTS///////////////
#define LCD_RS P2_1
#define LCD_RW P2_0 
#define LCD_E  P1_7
#define LCD_D4 P1_3
#define LCD_D5 P1_2
#define LCD_D6 P1_1
#define LCD_D7 P1_0

#define TEST P2_7
#define REF	 P2_6

//button input
#define PUSH_BUTTON P0_2
#define LED P0_3


#define CHARS_PER_LINE 16

// ANSI colors
#define	COLOR_BLACK		0
#define	COLOR_RED		1
#define	COLOR_GREEN		2
#define	COLOR_YELLOW	3
#define	COLOR_BLUE		4
#define	COLOR_MAGENTA	5
#define	COLOR_CYAN		6
#define	COLOR_WHITE		7

// Some ANSI escape sequences
#define CURSOR_ON "\x1b[?25h"
#define CURSOR_OFF "\x1b[?25l"
#define CLEAR_SCREEN "\x1b[2J"
#define GOTO_YX "\x1B[%d;%dH"
#define CLR_TO_END_LINE "\x1B[K"

/* Black foreground, white background */
#define BKF_WTB "\x1B[0;30;47m"
#define FORE_BACK "\x1B[0;3%d;4%dm"
#define FONT_SELECT "\x1B[%dm"

char _c51_external_startup (void)
{
	PCA0MD&=(~0x40) ;    // DISABLE WDT: clear Watchdog Enable bit
	// CLKSEL&=0b_1111_1000; // Not needed because CLKSEL==0 after reset
	#if (SYSCLK == (12*MHZ))
		//CLKSEL|=0b_0000_0000;  // SYSCLK derived from the Internal High-Frequency Oscillator / 4 
	#elif (SYSCLK == (24*MHZ))
		CLKSEL|=0b_0000_0010; // SYSCLK derived from the Internal High-Frequency Oscillator / 2.
	#elif (SYSCLK == (48*MHZ))
		CLKSEL|=0b_0000_0011; // SYSCLK derived from the Internal High-Frequency Oscillator / 1.
	#else
		#error SYSCLK must be either 12MHZ, 24MHZ, or 48MHZ
	#endif
	OSCICN |= 0x03; // Configure internal oscillator for its maximum frequency
	
	//configure output
	P1MDOUT|=0b_1000_1111; 	
	P2MDOUT|=0b_0000_0011; 
		 
	
	// Configure P2.0 to P2.3 as analog inputs
	P2MDIN &= 0b_1111_0011; // P2.2 to P2.3
	P2SKIP |= 0b_0000_1100; // Skip Crossbar decoding for these pins

	// Init ADC multiplexer to read the voltage between P2.0 and ground.
	// These values will be changed when measuring to get the voltages from
	// other pins.
	// IMPORTANT: check section 6.5 in datasheet.  The constants for
	// each pin are available in "c8051f38x.h" both for the 32 and 48
	// pin packages.
	AMX0P = LQFP32_MUX_P2_2; // Select positive input from P2.2
	AMX0N = LQFP32_MUX_GND;  // GND is negative input (Single-ended Mode)
	
	// Init ADC
	ADC0CF = 0xF8; // SAR clock = 31, Right-justified result
	ADC0CN = 0b_1000_0000; // AD0EN=1, AD0TM=0
  	REF0CN=0b_0000_1000; //Select VDD as the voltage reference for the converter
  	
	VDM0CN=0x80;       // enable VDD monitor
	RSTSRC=0x02|0x04;  // Enable reset on missing clock detector and VDD
	P0MDOUT|=0x10;     // Enable Uart TX as push-pull output
	XBR0=0x01;         // Enable UART on P0.4(TX) and P0.5(RX)
	XBR1=0x40;         // Enable crossbar and weak pull-ups
	
	#if (SYSCLK/BAUDRATE/2L/256L < 1)
		TH1 = 0x10000-((SYSCLK/BAUDRATE)/2L);
		CKCON &= ~0x0B;                  // T1M = 1; SCA1:0 = xx
		CKCON |=  0x08;
	#elif (SYSCLK/BAUDRATE/2L/256L < 4)
		TH1 = 0x10000-(SYSCLK/BAUDRATE/2L/4L);
		CKCON &= ~0x0B; // T1M = 0; SCA1:0 = 01                  
		CKCON |=  0x01;
	#elif (SYSCLK/BAUDRATE/2L/256L < 12)
		TH1 = 0x10000-(SYSCLK/BAUDRATE/2L/12L);
		CKCON &= ~0x0B; // T1M = 0; SCA1:0 = 00
	#else
		TH1 = 0x10000-(SYSCLK/BAUDRATE/2/48);
		CKCON &= ~0x0B; // T1M = 0; SCA1:0 = 10
		CKCON |=  0x02;
	#endif
	
	TL1 = TH1;     // Init timer 1
	TMOD &= 0x0f;  // TMOD: timer 1 in 8-bit autoreload
	TMOD |= 0x20;                       
	TR1 = 1;       // Start timer1
	SCON = 0x52;
	
	return 0;
}


void TIMER0_Init(void)
{
	TMOD&=0b_1111_0000; // Set the bits of Timer/Counter 0 to zero
	TMOD|=0b_0000_0001; // Timer/Counter 0 used as a 16-bit timer
	TR0=0; // Stop Timer/Counter 0
}


// Uses Timer3 to delay <us> micro-seconds. 
void Timer3us(unsigned char us)
{
	unsigned char i;               // usec counter
	
	// The input for Timer 3 is selected as SYSCLK by setting T3ML (bit 6) of CKCON:
	CKCON|=0b_0100_0000;
	
	TMR3RL = (-(SYSCLK)/1000000L); // Set Timer3 to overflow in 1us.
	TMR3 = TMR3RL;                 // Initialize Timer3 for first overflow
	
	TMR3CN = 0x04;                 // Sart Timer3 and clear overflow flag
	for (i = 0; i < us; i++)       // Count <us> overflows
	{
		while (!(TMR3CN & 0x80));  // Wait for overflow
		TMR3CN &= ~(0x80);         // Clear overflow indicator
	}
	TMR3CN = 0 ;                   // Stop Timer3 and clear overflow flag
}

void waitms (unsigned int ms)
{
	unsigned int j;
	unsigned char k;
	for(j=0; j<ms; j++)
		for (k=0; k<4; k++) Timer3us(250);
}

void LCD_pulse (void)
{
	LCD_E=1;
	Timer3us(40);
	LCD_E=0;
}

void LCD_byte (unsigned char x)
{
	// The accumulator in the C8051Fxxx is bit addressable!
	ACC=x; //Send high nible
	LCD_D7=ACC_7;
	LCD_D6=ACC_6;
	LCD_D5=ACC_5;
	LCD_D4=ACC_4;
	LCD_pulse();
	Timer3us(40);
	ACC=x; //Send low nible
	LCD_D7=ACC_3;
	LCD_D6=ACC_2;
	LCD_D5=ACC_1;
	LCD_D4=ACC_0;
	LCD_pulse();
}

void WriteData (unsigned char x)
{
	LCD_RS=1;
	LCD_byte(x);
	waitms(2);
}

void WriteCommand (unsigned char x)
{
	LCD_RS=0;
	LCD_byte(x);
	waitms(5);
}

void LCD_4BIT (void)
{
	LCD_E=0; // Resting state of LCD's enable is zero
	LCD_RW=0; // We are only writing to the LCD in this program
	waitms(20);
	// First make sure the LCD is in 8-bit mode and then change to 4-bit mode
	WriteCommand(0x33);
	WriteCommand(0x33);
	WriteCommand(0x32); // Change to 4-bit mode

	// Configure the LCD
	WriteCommand(0x28);
	WriteCommand(0x0c);
	WriteCommand(0x01); // Clear screen command (takes some time)
	waitms(20); // Wait for clear screen command to finish.
}

void LCDprint(char * string, unsigned char line, bit clear)
{
	int j;

	WriteCommand(line==2?0xc0:0x80);
	waitms(5);
	for(j=0; string[j]!=0; j++)	WriteData(string[j]);// Write the message
	if(clear) for(; j<CHARS_PER_LINE; j++) WriteData(' '); // Clear the rest of the line
}




#define VDD      3.325 // The measured value of VDD in volts
#define NUM_INS  2

void main (void)
{
	float Period=0;
	float Phasor=0;
	float frequency=0;
	int flag=0;
	int flag1=0;
  	int no_signal_flag = 0;
	float v;
	char Period_str[14];
	char V1_str[9];
	char V2_str[9];
	char Phasor_str[13];
	char Freq_str[9];
	unsigned char j;
	int i = 1;
	
	TIMER0_Init();
	LCD_4BIT();
	
	//LCDprint("testing", 1, 1);
	
	printf(CLEAR_SCREEN); // Clear screen using ANSI escape sequence.
	printf(CURSOR_OFF);
	printf( FORE_BACK, COLOR_BLACK, COLOR_WHITE );
	printf(GOTO_YX , 1, 1);
	printf ("Phasor Voltmeter\n"
	        "Apply peak voltages to P2.2 P2.3\n"
	        "Apply zero cross to P2.6 P2.7\n"
	        "File: %s\n"
	        "Compiled: %s, %s\n\n",
	        __FILE__, __DATE__, __TIME__);
	printf (FORE_BACK, COLOR_BLACK, COLOR_WHITE);
	// Start the ADC in order to select the first channel.
	// Since we don't know how the input multiplexer was set up,
	// this initial conversion needs to be discarded.
	AD0BUSY=1;
	while (AD0BUSY); // Wait for conversion to complete

	while(1)
	{
		waitms(100);
		LED=1;
    	no_signal_flag = 0;
		TR0=0; //Stop timer 0
		TH0=0; TL0=0; TF0=0;// Reset the timer
    	TR0 = 1; //start timer
    	while (REF == 1)
    	{
      		if (TF0 == 1) {LED=0;printf("\x1B[7;1H");LCDprint("No REF Signal",1,1);printf( FORE_BACK, COLOR_BLACK, COLOR_WHITE );printf("---No REF signal---"); no_signal_flag = 1;	break;}
    	}
    	waitms(400);
 
    	TR0=0; //Stop timer 0
		TH0=0; TL0=0; TF0=0;// Reset the timer
    	TR0 = 1; //start timer
    	while (TEST == 1)
    	{
      		if (TF0 == 1) {LED=0;LCDprint("No TEST Signal",1,1); printf("\x1B[7;1H");printf( FORE_BACK, COLOR_WHITE, COLOR_BLACK );printf("---No TEST signal--"); no_signal_flag = 1;	break;}
    	}
  
    	if (no_signal_flag) continue;
		no_signal_flag = 0;
    
		for(j=0; j<NUM_INS; j++)
		{
			AD0BUSY = 1; // Start ADC 0 conversion to measure previously selected input
			
			// Select next channel while ADC0 is busy
			switch(j)
			{
				case 0:
					AMX0P=LQFP32_MUX_P2_3;
				break;
				case 1:
					AMX0P=LQFP32_MUX_P2_2;
				break;
			}
			
			while (AD0BUSY); // Wait for conversion to complete
			AD0BUSY=1;
			while (AD0BUSY); // Wait for conversion to complete
			v = ((ADC0L+(ADC0H*0x100))*VDD)/1023.0; // Read 0-1023 value in ADC0 and convert to volts
			//v += 0.6;
			//v = 2*v*(1000*1.02)/(1020-1);
			v = v/1.414; //get RMS
			
			// Display measured values
			switch(j)
			{
				case 0:
					sprintf(V1_str, "Vt=%3.2fV", v);
					printf( GOTO_YX , 9, 1);
					printf( FORE_BACK, i, COLOR_BLACK );
					printf("Vtest=%5.3fV   ", v);
				break;
				case 1:
					sprintf(V2_str, "Vf=%3.2fV", v);
					printf( FORE_BACK, i, COLOR_BLACK );
					printf("Vref=%5.3fV ", v);
				break;
			}

		}
		
		// Measure half period using timer 0
		TR0=0; //Stop timer 0
		TH0=0; TL0=0; TF0=0;// Reset the timer
    	TR0 = 1; //start timer
    	while (REF == 1)
    	{
      		if (TF0 == 1)
      		{
      			LED=0;
      			LCDprint("No REF Signal",1,1);
      			printf( GOTO_YX , 7, 1);
      			printf( FORE_BACK, COLOR_BLACK, COLOR_WHITE );
      			printf("---No REF signal---"); 
      			no_signal_flag = 1;	
      			break;
      		}
    	}
    	
    	TR0=0; //Stop timer 0
		TH0=0; TL0=0; TF0=0;// Reset the timer
		if (no_signal_flag) continue;
		
		
		while (REF==0); // Wait for the signal to be one
		TR0=1; // Start timing
		while (REF==1); // Wait for the signal to be zero
		TR0=0; // Stop timer 0
		// [TH0,TL0] is half the period in multiples of 12/CLK, so:
		Period=(TH0*0x100+TL0)*2; // Assume Period is unsigned int
		
		printf( GOTO_YX , 11, 1);
		printf( FORE_BACK, i+1, COLOR_BLACK );
		printf("Period = %fus   ", Period);
		sprintf(Period_str, "Period=%3.0fus", Period);
		frequency = 1000000.0/Period;
		printf( FORE_BACK, i+1, COLOR_BLACK );
		printf("Frequency = %fHz ", frequency);
		printf("\x1B[K");
		sprintf(Freq_str, "f=%3.0fHz", frequency);
		
		
		// Measure time difference
		no_signal_flag = 0;
		TR0=0; //Stop timer 0
		TH0=0; TL0=0; TF0=0;// Reset the timer
    	TR0 = 1; //start timer
    	while (REF == 1)
    	{
      		if (TF0 == 1) {LED=0;LCDprint("No REF Signal",1,1);printf( GOTO_YX , 7, 1);printf("---No REF signal---"); no_signal_flag = 1;	break;}
    	}
    	TR0=0; //Stop timer 0
		TH0=0; TL0=0; TF0=0;// Reset the timer
		if (no_signal_flag) continue;
		while (REF==0); // Wait for the ref signal to be one
		if(TEST==1) //check if test=1
		{
			while (TEST==1); //wait for test signal to be zero
			TR0=1;
			while(REF==1); //Wait for ref signal to be zero
			TR0=0;
			flag=0; //test leading ref
		}
		else
		{
			TR0=1; // Start timing
			while (TEST==0); // Wait for the signal to be one
			TR0=0; // Stop timer 0
			flag=1;
		}
		
		// [TH0,TL0] is half the period in multiples of 12/CLK, so:
		Phasor=(TH0*0x100+TL0); // Assume Period is unsigned int
		printf( GOTO_YX , 13, 1);
		printf( FORE_BACK, i+2, COLOR_BLACK );
		printf("Time Difference=%fus   ", Phasor);
		
		Phasor=(Phasor/Period)*360;
		if (flag==1){Phasor = -Phasor;}
		printf( FORE_BACK, i+2, COLOR_BLACK );
		printf("Phasor=%fdegree ", Phasor);
		printf("\x1B[K");
		
		sprintf(Phasor_str, "Phase=%4.1f", Phasor);
		
		
		if(PUSH_BUTTON==0&flag1==0)
    	{
      		waitms(25);
      		if(PUSH_BUTTON==0) flag1=1;
    	}
    	
  		else if(PUSH_BUTTON==0&flag1==1)
  		{
  			waitms(25);
      		if(PUSH_BUTTON==0) flag1=2;
  		}
  		else if(PUSH_BUTTON==0&flag1==2)
  		{
  			waitms(25);
      		if(PUSH_BUTTON==0) flag1=0;
  		}
    		
		if(flag1==0)
		{
			LCDprint(V1_str,1,1);
    		LCDprint(V2_str,2,1);
		}   
		
		if(flag1==1)
		{
			LCDprint(Phasor_str,1,1);
          	LCDprint(Period_str,2,1);
		}
		
		if(flag1==2)	
   		{
			LCDprint(Freq_str,2,1);
		}
    	printf( GOTO_YX , 15, 1);
    	i = (i+1)%6;
    	if (i == 0) i=1;
	}  
}	

