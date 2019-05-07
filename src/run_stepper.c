//----------------------------------------------------------------------------------
// Multi-stepper motor driving program to use with my cap shooter.
// Busy-waits using timer reigsters and sequences stepper motors
// system call latency too long so just busy wait instead.
//----------------------------------------------------------------------------------
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <memory.h>
#include <fcntl.h>
#include <sys/mman.h>
typedef int BOOL;
#define FALSE 0
#define TRUE 1

int CheckUdp(int * XDeg, int * YDeg, int * IsFire, int * IsDelta);

#define TICK_SIZE 200    // Algorithm tick rate, microsconds.  Take at least two ticks per step.
#define TICK_ERROR 300   // Tick must not exceed this time.

//#define SHOOT_MODE 1

#define SHOT_DRAW_STEPS 875
#define SHOT_DRAW_DELAY 150

//-----------------------------------------------------------------------------------
//  This program based on "How to access GPIO registers from C-code on the Raspberry-Pi
//  Example program
//-----------------------------------------------------------------------------------

// Must run on rasperry pi 2 or 3 (pi 1 is single core only, can't monopolize a core)
#define BCM2708_PERI_BASE        0x3f000000 // For Raspberry pi 2 model B

#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) // GPIO controller
#define TIMER_BASE               (BCM2708_PERI_BASE + 0x3000)   // Interrupt registers (with timer)

 
#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)
 
 
// I/O access
volatile unsigned *gpio;
volatile unsigned *timerreg;
  
// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))
 
#define GPIO_SET *(gpio+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio+10) // clears bits which are 1 ignores bits which are 0

#define GPIO_SET2 *(gpio+8)  // same as GPI_SET macro, but for GPIO 32 and higher 
#define GPIO_CLR2 *(gpio+11) // same as GPI_SET macro, but for GPIO 32 and higher 

#define GET_GPIO(g) (*(gpio+13)&(1<<g)) // 0 if LOW, (1<<g) if HIGH
 
#define GPIO_PULL *(gpio+37) // Pull up/pull down
#define GPIO_PULLCLK0 *(gpio+38) // Pull up/pull down clock
 


//--------------------------------------------------------------------------------
// Set up a memory regions to access GPIO
//--------------------------------------------------------------------------------
volatile unsigned * setup_io(int io_base, int io_range)
{
	int  mem_fd;
	void *gpio_map;

    // open /dev/mem 
	if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
	    printf("can't open /dev/mem \n");
	    exit(-1);
	}

	/* mmap GPIO */
	gpio_map = mmap(
		NULL,             //Any adddress in our space will do
		io_range,       //Map length
		PROT_READ|PROT_WRITE,// Enable reading & writting to mapped memory
		MAP_SHARED,       //Shared with other processes
		mem_fd,           //File to map
		io_base           //Offset to peripheral
	);

	close(mem_fd); //No need to keep mem_fd open after mmap

	if (gpio_map == MAP_FAILED) {
		printf("mmap error %d\n", (int)gpio_map);//errno also set!
		exit(-1);
	}

	// Always use volatile pointer!
	return (volatile unsigned *) gpio_map;
}

// Definitions for bor draw motor (motor 1)
#define STEP_ENA1 (1<<2)  // GPIO2 Enable
#define STEP_DIR1 (1<<3)  // GPIO3 Direction
#define STEP_CLK1 (1<<4)  // GPIO4 Clock

// Definitions for tilt motor (motor 2)
#define STEP_ENA2 (1<<15)  // GPIO15 Enable
#define STEP_DIR2 (1<<17)  // GPIO17 Direction
#define STEP_CLK2 (1<<18)  // GPIO18 Clock

// Definitions for turret rotate motor (motor 3)
#define STEP_ENA3 (1<<22)  // GPIO22 Enable
#define STEP_DIR3 (1<<23)  // GPIO23 Direction
#define STEP_CLK3 (1<<24)  // GPIO24 Clock

int CurrentPos = 0;

#define NUM_RAMP_STEPS 28
static const char RampUp[NUM_RAMP_STEPS]
 = {30,35,40,45,50,55,60,65,70,75,80,85,90,95,100,103,106,108,110,112,114,116,118,120,122,124,126,128}; // Also use for ramp down.

typedef struct {
	int Pos;
	int Target;
	//unsigned char RampUp[20,30,40,50,60,70,70,90,100,105,110,115,120,124,128]; // Also use for ramp down.
	int Speed; // 128 = 1 tick per clock on/off, 64 = 2 ticks per, 1 = 256 ticks per.
	int Dir;  // 1 or -1.
	int CountDown;
	int Wait; // Start/stop delay.
	int RampIndex;
	int RampStretch;
	int MaxSpeed;
	
	// GPIO line definitions
	unsigned int ENABLE;
	unsigned int DIR;
	unsigned int CLOCK;
}stepper_t;

stepper_t motors[3];

#define ABS(a) (a > 0 ? a : -(a))


//-------------------------------------------------------------------------------------
// Test real-time performance of busywaiting using the microsecond hw timer register
//-------------------------------------------------------------------------------------
void TestTimer(void)
{
    int time1,time2,a, cycles;
    printf("timer is: \n%d\n%d\n",*(timerreg+1),*(timerreg+1));
    printf("timer is: \n%d\n%d\n",*(timerreg+1),*(timerreg+1));
    
    for (a=0;a<=50;a++){
        time1 = *(timerreg+1);
        usleep(a);
        //sleep(1);
        if (0){
			int x,y,a,b;
            CheckUdp(&x,&y,&a,&b);
        }
        time2 = *(timerreg+1);
        printf("usleep %d: ticked %d\n",a,time2-time1);
        if (a >=10) a += 4;
        if (a >=100) a += 5;
    }
    
    printf("looking for delays...\n");
    for (cycles=0;;cycles++){
        int DelayBins[15];
        int StartTime;
        int t1,t2,diff;
        int missing;
        int longest ;
        memset(DelayBins, 0, sizeof(DelayBins));;
        missing=longest=0;
        t2 = StartTime = *(timerreg+1);
        for (;;){
            t1 = *(timerreg+1);
            diff = t1-t2;
            if (diff > longest) longest = diff;
            if (diff >= 1){
                missing += diff-1;
                for (a=0;a<15;a++){
                    diff >>= 1;
                    if (diff == 0){
                        DelayBins[a] += 1;
                        break;
                    }
                }
            }
            t2 = t1;
            if (t2-StartTime > 1000000){
                break;
            }
        }
        diff = 1;
        printf("%5d  ",missing);
        for (a=0;a<15;a++){
            printf("%4d",DelayBins[a]);
            diff <<= 1;
        }
        printf(" l=%d\n",longest);
    }
}

// Notes:
// Calling CheckUdp adds about 650 microseconds!
// Calling uSleep adds 60 microseconds + specified time.

//-------------------------------------------------------------------------------------
// Motor routine - do steps if necessary.
//-------------------------------------------------------------------------------------
void DoMotor(stepper_t * motor)
{
	int ToGo;
		
	if (motor->Wait){
		motor->Wait -= 1;
	}else{
		ToGo = motor->Target - motor->Pos;
		//printf("to go:%d Target:%d pos:%d\n",ToGo,motor->Target,motor->Pos);
		
		if (ToGo){
			int ToGoAbs;
			ToGoAbs = ToGo;
			if(ToGo < 0) ToGoAbs = -ToGo;
			if (motor->Speed == 0){
				// Motor was not running.  Enable and set direction.
				GPIO_CLR = motor->ENABLE; // Enable the motor.
				if (ToGo > 0){ // Set direction.
					GPIO_SET = motor->DIR;
				}else{
					GPIO_CLR = motor->DIR;
				}
				motor->RampIndex = 0;
				motor->Speed = RampUp[motor->RampIndex];
				motor->Dir = ToGo > 0 ? 1 : -1;
				motor->CountDown = 127;
				motor->Wait = 1;
			}else{
				motor->CountDown -= motor->Speed;
				if (motor->CountDown < 0){
					motor->Pos += motor->Dir; // This completes this clock cycle.
					
					// Compute the new speed.
					motor->RampIndex += 1;
					if (motor->RampIndex < NUM_RAMP_STEPS*motor->RampStretch) motor->Speed = RampUp[motor->RampIndex/motor->RampStretch];
					if (ToGoAbs < NUM_RAMP_STEPS*motor->RampStretch){
						// Ramp down if getting close.
						if (RampUp[ToGo] < motor->Speed) motor->Speed = RampUp[ToGoAbs/motor->RampStretch];
					}
					if (motor->Speed > motor->MaxSpeed) motor->Speed = motor->MaxSpeed;
					
					if (motor->Pos != motor->Target){
						// Start the next step.
						motor->CountDown += 256;
						// 128 and above means clock high.
						GPIO_SET = motor->CLOCK;
					}else{
						motor->Wait = 20;
					}
				}else if (motor->CountDown < 128){
					GPIO_CLR = motor->CLOCK;
					// 127 or below means clock low.
				}
			}
		}else{
			if (motor->Speed){
				//GPIO_SET = motor->ENABLE; // turn off the motor.
				motor->Speed = 0;
			}
		}
	}
	//printf("Cl:%d Wait %d  ToGo:%3d  CntDwn:%3d Speed%3d\n",motor->CountDown > 128, motor->Wait, motor->Target - motor->Pos, motor->CountDown, motor->Speed);
}

//-------------------------------------------------------------------------------------
// Init GPIO and motor states.
//-------------------------------------------------------------------------------------
void Init(void)
{
	// Set up gpi pointer for direct register access
	gpio = setup_io(GPIO_BASE, BLOCK_SIZE);
	timerreg = setup_io(TIMER_BASE, BLOCK_SIZE);

	// Motor 1:
    INP_GPIO(2); OUT_GPIO(2);
    INP_GPIO(3); OUT_GPIO(3);
    INP_GPIO(4); OUT_GPIO(4);

	// Motor 2:
    INP_GPIO(15); OUT_GPIO(15);
    INP_GPIO(17); OUT_GPIO(17);
    INP_GPIO(18); OUT_GPIO(18);

	// Motor 3:
    INP_GPIO(22); OUT_GPIO(22);
    INP_GPIO(23); OUT_GPIO(23);
    INP_GPIO(24); OUT_GPIO(24);

	motors[0].Pos = 0;
	motors[0].Target = 0;
	#ifdef SHOOT_MODE
	motors[0].Target = SHOT_DRAW_STEPS;
	#endif
	motors[0].ENABLE = STEP_ENA1;
	motors[0].DIR = STEP_DIR1;
	motors[0].CLOCK = STEP_CLK1;
	motors[0].MaxSpeed = 128;
	motors[0].RampStretch=1;

	motors[1].Pos = 0;
	motors[1].Target = 0; //Tilt is 31.1 steps per degree.
	motors[1].ENABLE = STEP_ENA2;
	motors[1].DIR = STEP_DIR2;
	motors[1].CLOCK = STEP_CLK2;
	motors[1].MaxSpeed = 60;
	motors[1].RampStretch=10;
	
	motors[2].Pos = 0;
	motors[2].Target = 0; // turret is 9.72 steps per degree.
	motors[2].ENABLE = STEP_ENA3;
	motors[2].DIR = STEP_DIR3;
	motors[2].CLOCK = STEP_CLK3;
	motors[2].MaxSpeed = 128;
	motors[2].RampStretch=12;

}

//-------------------------------------------------------------------------------------
// Steper busywait main timer loop.
//-------------------------------------------------------------------------------------
void RunStepping(void)
{
	int time1, time2;
	int numticks;
	int flag;
	int IsIdle;
	int taking_shot, last_fired;
	int last_seen;
	int GoHome;

	Init();
	
	flag = 0;
	taking_shot = 0;
	IsIdle = 0;
	
	numticks = 0;
	last_fired = 0;
	time1 = *(timerreg+1);
	GoHome = 0;
	
	for(;;){
		for (;;){ // Busywait for next tick interval (usleep system call has too much jitter)
			int delta;
			time2 = *(timerreg+1);
			delta = time2-time1;
			if (delta >= TICK_SIZE){
				if (delta > TICK_ERROR && IsIdle == 0){
					printf("tick too long!\n");
					time2 = *(timerreg+1);
				}
				time1 = time2;
				break;
			}
		}
		if (numticks == 0) last_fired = time1;
		numticks ++;
		
		DoMotor(&motors[0]);
		DoMotor(&motors[1]);
		DoMotor(&motors[2]);
		
		if (motors[0].Speed == 0 && motors[0].Wait == 0 
		 && motors[1].Speed == 0 && motors[1].Wait == 0 
		 && motors[2].Speed == 0){
			// All motions complete.  Look for new instructions
			int xDeg,yDeg,z,fire,isdelta;
			if (!IsIdle) printf("Motion complete.\n");
			IsIdle = 1;
			
			if (CheckUdp(&xDeg,&yDeg,&fire,&isdelta)){
				// Use coordinates as inputs for the motors.
				motors[2].Target = -xDeg * 972 * 4 / 1000;
				motors[1].Target = yDeg * 3110 / 1000;
				printf("Target steps %d,%d\n",motors[2].Target, motors[1].Target);
				
				if ((unsigned)(time1-last_fired) > 1000000*4){ // If it's been 4 seconds, fire again.
					if (time1-last_seen < 800000){ // And this isn't the first report in a long time.
						motors[0].Target = SHOT_DRAW_STEPS;
						taking_shot = SHOT_DRAW_DELAY;
						last_fired = time1;
					}
				}
				last_seen = time1;
				GoHome = 0;
			}
			if (time1-last_seen > 6000000){
				if (!GoHome){
					// Nothing seen for a while.  Return home.
					printf("No commands.  Return home\n");
					motors[1].Target = motors[2].Target = 0;
					GoHome = 1;
				}else{
					if (time1-last_seen > 10000000){
						// Disable the motors a bit later.
						GPIO_SET = motors[0].ENABLE | motors[1].ENABLE | motors[2].ENABLE;
					}
				}
			}
			/*
			if ((numticks & 32767) == 0){
				// test fire every few seconds.
				printf("fire");
				motors[0].Target = SHOT_DRAW_STEPS;
				taking_shot = SHOT_DRAW_DELAY;
			}
			*/
			
			
			if (taking_shot){
				if (motors[0].Speed == 0 && motors[0].Wait == 0){
					// Draw motion is complete, now wait a bit to let cap drop.
					taking_shot--;
					if (taking_shot == 0){
						// Drawn back dwell period over, time to return.
						motors[0].Target = 0;
					}
				}
			}
			
			
			#ifdef SHOOT_MODE // Shoot cap mode.
			if (flag == 0){
				usleep(70000); // Allow time for cap to drop.
				motors[0].Target = 0; // Then return draw.
				flag += 1;
			}else if (flag == 1){
				usleep(10000); // Make sure things are settled.
				// Turn off motor 0 driver.
				GPIO_SET = motors[0].ENABLE;
				exit(0);
			}
			#endif
		 }else{
			 IsIdle = 0;
		 }
	}
	return;
}
