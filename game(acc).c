/*
 * SIMON GAME
 *
 * Copyright (c) 2016 Jad Wahab & Gaelle Feghali
 * Department of Electrical and Computer Engineering
 * American University of Beirut
 *
 */


#include <stdio.h>
#include "xparameters.h"
#include "xgpio.h"
#include "xtmrctr.h"
#include "xintc.h"
#include "ADXL362.h"
#include <stdlib.h>


#define INTC_DEVICE_ID		XPAR_INTC_0_DEVICE_ID
#define INTC_BUTTONS_INT_ID	XPAR_INTC_0_GPIO_1_VEC_ID
#define INTC_TIMER_INT_ID	XPAR_INTC_0_TMRCTR_0_VEC_ID
#define BUTTONS_DEVICE_ID	XPAR_GPIO_1_DEVICE_ID
#define SSD_DEVICE_ID		XPAR_GPIO_0_DEVICE_ID
#define TMR_DEVICE_ID		XPAR_TMRCTR_0_DEVICE_ID
#define ONE_SEC_INTERVAL	0xFA0A1EFF
#define HALF_SEC_INTERVAL	0xFD050F7F
#define QUART_SEC_INTERVAL	0xFE8287BF

// Accelerometer
#define	ADXL362_BASE_ADDR	0x44a00000
#define ACCEL_X_OFFSET		ADXL362_S00_AXI_SLV_REG0_OFFSET
#define ACCEL_Y_OFFSET		ADXL362_S00_AXI_SLV_REG1_OFFSET
#define ACCEL_Z_OFFSET		ADXL362_S00_AXI_SLV_REG2_OFFSET



// Seven-segment display patterns for digits 0-9, blank(10), minus(11), m1(12), m2(13), n(14), l(15), e(16), v(17), p(18), a(19), g(20), d(21), f(22), i(23)
u32 pattern[24] = {0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82, 0xF8, 0x80, 0x98, 0xFF, 0xBF, 0xCC, 0xD8, 0xC8, 0xC7, 0x86, 0xC1, 0x8C, 0x88, 0x90, 0xA1, 0x8E, 0xFB};
//ssd display for segments up(0), right(1), center(2), down(3), left(4)
u32 pattern_seg[5] = {0xFE, 0xF9, 0xBF, 0xF7, 0xCF};
// Anode patterns used to enable seven-segment displays 0-7.
u32 anode[8] = {0xFE, 0xFD, 0xFB, 0xF7, 0xEF, 0xDF, 0xBF, 0x7F};
// Game Level Segment Sequence
u32 sequence[8];

// Prototypes for buttons and timer interrupt handler, and seven-segment display function.
void buttonsHandler(void *CallbackRef);
void TimerCounterHandler(void *CallBackRef, u8 t);
void displayPattern(u32 display, u32 patIndx);
void displayPatternSegment(u32 display, u32 patIndx);
void displaySIMON();
void displayLevel();
void displayGame(u32 lvl);
void displayLose();
void displayPass();
void displayGoScreen();
void displayDiff();
void gameDiffChosen(int diffLvl);
void waitTime();
void checkAccel();



// Global variables
static XGpio buttons;
static XGpio ssd;
static XIntc intc;
static XTmrCtr tmr;

static u32 gameLevel;
static u32 gameScore;
static u32 win_lose;
static u32 currentSeg;
static u32 counter;
static u32 menu; //1 for diff menu, 0 otherwise
static u32 diff;
static int fiveSecCount;
static int t; //for for loops


static int accVar; // 0: middle, 1: not middle



int main()
{

	// Initialize gameLevel and gameScore and win_lose
	gameLevel = 1;
	gameScore = 0;
	win_lose = 0;
	counter = 0;
	menu = 1;

	// Initialize accelerometer axes
	int x = ADXL362_mReadReg(ADXL362_BASE_ADDR, ACCEL_X_OFFSET);
	int y = ADXL362_mReadReg(ADXL362_BASE_ADDR, ACCEL_Y_OFFSET);
	int z = ADXL362_mReadReg(ADXL362_BASE_ADDR, ACCEL_Z_OFFSET);


	// Initialize GPIOs
	XGpio_Initialize(&buttons, BUTTONS_DEVICE_ID);
	XGpio_SetDataDirection(&buttons, 1, 0x1F);

	XGpio_Initialize(&ssd, SSD_DEVICE_ID);
	XGpio_SetDataDirection(&ssd, 1, 0x0000);
	XGpio_SetDataDirection(&ssd, 2, 0x0000);

	// Initialize Timer
	XTmrCtr_Initialize(&tmr, TMR_DEVICE_ID);
	XTmrCtr_SetHandler(&tmr, TimerCounterHandler, &tmr);
	XTmrCtr_SetOptions(&tmr, 0, XTC_INT_MODE_OPTION | XTC_AUTO_RELOAD_OPTION);


	// Enable interrupts at the processor, INTC, and device levels
	microblaze_enable_interrupts();

	XIntc_Initialize(&intc, INTC_DEVICE_ID);
	XIntc_Connect(&intc, INTC_BUTTONS_INT_ID, (XInterruptHandler)buttonsHandler, (void *)0);
	XIntc_Connect(&intc, INTC_TIMER_INT_ID, (XInterruptHandler)XTmrCtr_InterruptHandler,(void *)&tmr);
	XIntc_Start(&intc, XIN_REAL_MODE);
	XIntc_Enable(&intc, INTC_BUTTONS_INT_ID);
	XIntc_Enable(&intc, INTC_TIMER_INT_ID);

	XGpio_InterruptEnable(&buttons, 1);
	XGpio_InterruptGlobalEnable(&buttons);



	// MAIN CODE
	displaySIMON();

	while (1) {

		if (menu)
			displayDiff();
		else {

			gameDiffChosen(diff);

			int x;
			for (x=0; (x<8)&&(!menu); x++) {
				XGpio_InterruptDisable(&buttons, 1); // Disable buttons

				displayLevel();
				displayGame(gameLevel);

				XGpio_InterruptClear(&buttons, 1);	// Re-enable buttons
				XIntc_Acknowledge(&intc, INTC_BUTTONS_INT_ID);
				XGpio_InterruptEnable(&buttons,1);

				counter=0;
				XTmrCtr_Start(&tmr, 0);

				while((counter<fiveSecCount)&&(gameLevel!=win_lose)&&!menu) {
					displayGoScreen();

				}

				if (gameLevel == win_lose)
					displayPass();
				else
					displayLose();

				win_lose = 0;
				gameLevel++;

			}// end for

			//to repeat the game after for loop ends
			gameLevel = 1;
			gameScore = 0;
			win_lose = 0;
			counter = 0;
			menu = 1;
		}


	}//end while





	return 1;
}

// Buttons interrupt handler.
// Note that the CallbackRef parameter must be declared, but is not actually used in the handler.

void buttonsHandler(void *CallbackRef)
{
	u32 data, options;

	// Disable GPIO interrupts
	XGpio_InterruptDisable(&buttons, 1);

	// Debounce and read button value.
	// Note: We use a software debouncing loop because precise timing in this case is not necessary.
	for (t = 0; t < 10000; t++);
	data = XGpio_DiscreteRead(&buttons, 1);

	// Disable timer interrupts to prevent the timer from interrupting the button handler.
	options = XTmrCtr_GetOptions(&tmr, 0);
	options = options & 0xFFFFFFBF; // clears bit 6 (ENIT0)
	XTmrCtr_SetOptions(&tmr, 0, options);


	if (menu){	// Choose game difficulty
		switch (data) {
			// Left
			case 2:
				diff = 1;
				menu = 0;	// To move to the next stage
				break;
			// Center
			case 8:
				diff = 3;
				menu = 0;
				break;
			// Right
			case 16:
				diff = 2;
				menu = 0;
				break;
			default:
				break;
			}

	}


	// Clear and re-enable timer interrupts
	options = XTmrCtr_GetOptions(&tmr, 0);
	options = options | 0x00000140; // Set bits 8 (T0INT) and 6 (ENIT0)
	XTmrCtr_SetOptions(&tmr, 0, options);
	XIntc_Acknowledge(&intc, INTC_TIMER_INT_ID);

	// Clear and re-enable button interrupts

	XGpio_InterruptClear(&buttons, 1);
	XIntc_Acknowledge(&intc, INTC_BUTTONS_INT_ID);
	XGpio_InterruptEnable(&buttons,1);
}


// Timer interrupt handler.
// Note that the CallbackRef and t parameters must be declared, but are not actually used in the handler.

void TimerCounterHandler(void *CallBackRef, u8 t)
{
	u32 options;

	// Disable timer interrupts
	options = XTmrCtr_GetOptions(&tmr, 0);
	options = options & 0xFFFFFFBF; // clears bit 6 (ENIT0)
	XTmrCtr_SetOptions(&tmr, 0, options);

	// Increment counter
	counter++;

	// Check if the code was inputted correctly to end the wait
	if (gameLevel == win_lose)
		counter=fiveSecCount;

	// Clear and re-enable timer interrupts
	XIntc_Acknowledge(&intc, INTC_TIMER_INT_ID);
	options = XTmrCtr_GetOptions(&tmr, 0);
	options = options | 0x00000140; // Set bits 8 (T0INT) and 6 (ENIT0)
	XTmrCtr_SetOptions(&tmr, 0, options);
}


// Displays a seven-segment pattern on a specific display.

void displayPattern(u32 display, u32 patIndx)
{
	// Read the anode activation and display patterns
	u32 d = anode[display];
	u32 p = pattern[patIndx];
	// Use the patterns to drive the SSD
	XGpio_DiscreteWrite(&ssd, 1, d);
	XGpio_DiscreteWrite(&ssd, 2, p);
}


// Display SIMON on screen

void displaySIMON() {
	int i=0;
	while(i<300){//approx 2s
		displayPattern(6,5);
		for (t = 0; t < 10000; t++);
		displayPattern(5,1);
		for (t = 0; t < 10000; t++);
		displayPattern(4,12);
		for (t = 0; t < 10000; t++);
		displayPattern(3,13);
		for (t = 0; t < 10000; t++);
		displayPattern(2,0);
		for (t = 0; t < 10000; t++);
		displayPattern(1,14);
		for (t = 0; t < 10000; t++);
		i++;
	}
}


// Displays a game-segment on the ssd

void displayPatternSegment(u32 display, u32 patIndx)
{
	// Read the anode activation and display patterns
	u32 d = anode[display];
	u32 p = pattern_seg[patIndx];
	// Use the patterns to drive the SSD
	XGpio_DiscreteWrite(&ssd, 1, d);
	XGpio_DiscreteWrite(&ssd, 2, p);

	currentSeg = patIndx;	//set the current segment to the segment displayed
}


// Display level number

void displayLevel() {
	int i=0;
	while(i<150){//approx 1s
		displayPattern(7,15);
		for (t = 0; t < 10000; t++);
		displayPattern(6,16);
		for (t = 0; t < 10000; t++);
		displayPattern(5,17);
		for (t = 0; t < 10000; t++);
		displayPattern(4,16);
		for (t = 0; t < 10000; t++);
		displayPattern(3,15);
		for (t = 0; t < 10000; t++);

		displayPattern(1, gameLevel);
		for (t = 0; t < 10000; t++);
		i++;
	}
}


// Display Game Patterns
void displayGame(u32 lvl) {

	switch(lvl){
	case 1:
		displayPatternSegment(0, sequence[0]);
		waitTime();
		break;
	case 2:
		displayGame(1);
		displayPatternSegment(0, sequence[1]);
		waitTime();
		break;
	case 3:
		displayGame(2);
		displayPatternSegment(0, sequence[2]);
		waitTime();
		break;
	case 4:
		displayGame(3);
		displayPatternSegment(0, sequence[3]);
		waitTime();
		break;
	case 5:
		displayGame(4);
		displayPatternSegment(0, sequence[4]);
		waitTime();
		break;
	case 6:
		displayGame(5);
		displayPatternSegment(0, sequence[5]);
		waitTime();
		break;
	case 7:
		displayGame(6);
		displayPatternSegment(0, sequence[6]);
		waitTime();
		break;
	case 8:
		displayGame(7);
		displayPatternSegment(0, sequence[7]);
		waitTime();
		break;
	}
}

void displayPass(){

	gameScore = gameScore + gameLevel*diff;

	int i=0;
	while(i<150){//approx 1s
		displayPattern(5,18);
		for (t = 0; t < 10000; t++);
		displayPattern(4,19);
		for (t = 0; t < 10000; t++);
		displayPattern(3,5);
		for (t = 0; t < 10000; t++);
		displayPattern(2,5);
		for (t = 0; t < 10000; t++);

		i++;
	}


}


void displayLose() {
	int i=0;
	u32 c0,c1;

	c0 = gameScore%10;	// units digit
	c1 = gameScore/10;	// tens digit

	while(i<150){//approx 1s
		displayPattern(7,15);
		for (t = 0; t < 10000; t++);
		displayPattern(6,0);
		for (t = 0; t < 10000; t++);
		displayPattern(5,5);
		for (t = 0; t < 10000; t++);
		displayPattern(4,16);
		for (t = 0; t < 10000; t++);

		// Score
		displayPattern(1,c1);
		for (t = 0; t < 10000; t++);
		displayPattern(0,c0);
		for (t = 0; t < 10000; t++);
		i++;
	}

	menu = 1;
}


void displayGoScreen() {	// With 5s countdown
	displayPattern(7,20);
	for (t = 0; t < 10000; t++);
	displayPattern(6,0);
	for (t = 0; t < 10000; t++);

	switch(diff) {
	case 1:
		displayPattern(4, 5 - counter);
		for (t = 0; t < 10000; t++);
		break;
	case 2:
		displayPattern(4, 5-(counter/2));
		for (t = 0; t < 10000; t++);
		break;
	case 3:
		displayPattern(4, 5-(counter/4));
		for (t = 0; t < 10000; t++);
		break;
	default:
		break;
	}

	// Display Accelerometer actual value
	int x = ADXL362_mReadReg(ADXL362_BASE_ADDR, ACCEL_X_OFFSET);
	int y = ADXL362_mReadReg(ADXL362_BASE_ADDR, ACCEL_Y_OFFSET);
	int z = ADXL362_mReadReg(ADXL362_BASE_ADDR, ACCEL_Z_OFFSET);

	if (x<-100) { //up
		displayPatternSegment(0, 0);
		for (t = 0; t < 10000; t++);
	}
	else if (x>100) { //down
		displayPatternSegment(0, 3);
		for (t = 0; t < 10000; t++);
	}
	else if (y>100) { //left
		displayPatternSegment(0, 4);
		for (t = 0; t < 10000; t++);
	}
	else if (y<-100) { //right
		displayPatternSegment(0, 1);
		for (t = 0; t < 10000; t++);
	}

	if( (x>-150&&x<150) && (y>-210&&y<110)){ //set back to middle
		accVar = 0;
	}

	if (!accVar) {///middle
		checkAccel();
	}


}


void checkAccel() {

	int x = ADXL362_mReadReg(ADXL362_BASE_ADDR, ACCEL_X_OFFSET);
	int y = ADXL362_mReadReg(ADXL362_BASE_ADDR, ACCEL_Y_OFFSET);
	int z = ADXL362_mReadReg(ADXL362_BASE_ADDR, ACCEL_Z_OFFSET);
//	xil_printf("X = %d; Y = %d; Z = %d;\r\n",x,y,z);

	if (x<-250) { //up
		if (sequence[win_lose] == 0)
			win_lose++;
		else
			displayLose();
		accVar = 1;
	}
	else if (x>300) { //down
		if (sequence[win_lose] == 3)
			win_lose++;
		else
			displayLose();
		accVar = 1;
	}
	else if (y>300) { //left
		if (sequence[win_lose] == 4)
			win_lose++;
		else
			displayLose();
		accVar = 1;
	}
	else if (y<-300) { //right
		if (sequence[win_lose] == 1)
			win_lose++;
		else
			displayLose();
		accVar = 1;
	}


}


void displayDiff() {	// Difficulty screen
	displayPattern(7,21);
	for (t = 0; t < 10000; t++);
	displayPattern(6,23);
	for (t = 0; t < 10000; t++);
	displayPattern(5,22);
	for (t = 0; t < 10000; t++);
	displayPattern(4,22);
	for (t = 0; t < 10000; t++);

	displayPattern(2,1);
	for (t = 0; t < 10000; t++);
	displayPattern(1,2);
	for (t = 0; t < 10000; t++);
	displayPattern(0,3);
	for (t = 0; t < 10000; t++);
}


void gameDiffChosen(int diffLvl) {	// Set the timer value according to diff chosen
	switch (diffLvl) {
		case 1:
			XTmrCtr_SetResetValue(&tmr, 0, ONE_SEC_INTERVAL);	// One sec
			fiveSecCount = 5;
			break;
		case 2:
			XTmrCtr_SetResetValue(&tmr, 0, HALF_SEC_INTERVAL);	// 0.5 sec
			fiveSecCount = 10;
			break;
		case 3:
			XTmrCtr_SetResetValue(&tmr, 0, QUART_SEC_INTERVAL);	// 0.25 sec
			fiveSecCount = 20;
			break;
		default:
			break;
	}

	// Generate random sequence
	int x = abs(ADXL362_mReadReg(ADXL362_BASE_ADDR, ACCEL_X_OFFSET));
	int y = abs(ADXL362_mReadReg(ADXL362_BASE_ADDR, ACCEL_Y_OFFSET));
	int z = abs(ADXL362_mReadReg(ADXL362_BASE_ADDR, ACCEL_Z_OFFSET));

	srand(x*y*z);

	for (t=0; t<8; t=t+3)
		sequence[t] = rand()%5;

}


// Wait specific amount of time

void waitTime() {
	counter=0;
	XTmrCtr_Start(&tmr, 0);
	while(counter==0);
	displayPattern(0,10);	// Display blank after each segment
	for (t = 0; t < 1000000; t++);	// For case of 2 consecutive same segments
}

