// UDP / PING program
// For testing UDP and ICMP behaviours on GPRS.
//----------------------------------------------------------------------------------
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN // To keep windows.h bloat down.    
    #ifdef OLD_WINSOCK
        #include <winsock.h>
    #else
        #include <winsock2.h>
    #endif
    #include <conio.h>
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <linux/net.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <sys/time.h>
    #include <memory.h>
    typedef unsigned char BYTE;
    typedef unsigned char UCHAR;
    typedef unsigned short USHORT;
    typedef unsigned long ULONG;
    typedef int SOCKET;
    typedef int BOOL;
    #define FALSE 0
    #define TRUE 1
    #define SOCKET_ERROR -1
    #define INVALID_SOCKET -1
    typedef fd_set FD_SET;
    typedef struct timeval TIMEVAL;
#endif


#define ICMP_ECHO 8
#define ICMP_ECHOREPLY 0

#define ICMP_MIN 8 // minimum 8 byte icmp packet (just header)

#define MAGIC_ID 0xf581     // Magic value to identify pings from this program.

#define MAGIC_PORTNUM 7777   // Magic port number for UDP pings (this is the port Joe picked for his app)

#define UDP_MAGIC 0x46c1

//-------------------------------------------------------------------------------------
// Structure for a test UDP packet.
typedef struct {
    int Ident;
    int Level;
    int xpos;
    int ypos;
    int IsAdjust;
}Udp_t;

//-------------------------------------------------------------------------------------

#define STATUS_FAILED 0xFFFF
#define DEF_PACKET_SIZE 32
#define MAX_PACKET 1024

struct sockaddr_in dest;

static SOCKET sockUDP;

static int UdpPayloadLength = -1;

//--------------------------------------------------------------------------
// Form and send a UDP packet
//--------------------------------------------------------------------------
void SendUDP(int Pos, int IsDelta)
{
    int datasize;
    int wrote;
    Udp_t Buf;
    
    memset(&Buf, 0, sizeof(Buf));

    Buf.Ident = UDP_MAGIC;
    Buf.Level = 1000;
    Buf.xpos = Pos;
    Buf.ypos = 0;
    Buf.IsAdjust = IsDelta;
    datasize = sizeof(Udp_t);

    wrote = sendto(sockUDP,(char *)&Buf, datasize, 0,(struct sockaddr*)&dest, sizeof(struct sockaddr_in));

    if (wrote == SOCKET_ERROR){
        perror("UDP sendto failed");

        #ifndef _WIN32
            printf("On Linux, This eventually happens if the destination port is unreacable and Uping is not root\n");
        #endif
        exit(-1);
    }
    if (wrote < datasize ) {
        fprintf(stdout,"Wrote %d bytes of %d\n",wrote, datasize);
    }

    printf("Sent UDP packet, x=%d\n",Pos);
}

//----------------------------------------------------------------------------------
// Command line help...
//----------------------------------------------------------------------------------
void Usage(char *progname)
{
    
    fprintf(stderr,"Usage:\n");
    fprintf(stderr,"%s [host] [pos]\n",progname);
    fprintf(stderr,"Options:\n");
    
    exit(-1);
}

void RunStepping(void);
int PosRequested;
int DeltaRequested;

//--------------------------------------------------------------------------
// Main
//--------------------------------------------------------------------------
int main(int argc, char **argv)
{
    #ifdef _WIN32
        WSADATA wsaData;
    #endif

    struct hostent * hp;
    unsigned int addr=0;
    unsigned short PortNum = MAGIC_PORTNUM;
    char * HostName = NULL;
    char String[500];

    memset(String, 0, sizeof(String));

    if (argc > 1){
        HostName = argv[1];
    }
    if (argc > 2){
        DeltaRequested = 0;
        if (argv[2][0] == '-') DeltaRequested = 1;
        if (argv[2][0] == '+'){
            DeltaRequested = 1;
            argv[2]++;
        }
        
        sscanf(argv[2],"%d",&PosRequested);
    }

    #ifdef _WIN32
        if (WSAStartup(MAKEWORD(1,1),&wsaData) != 0){
            fprintf(stderr,"WSAStartup failed: %d\n",GetLastError());
                ExitProcess(STATUS_FAILED);
        }
    #endif

    //-------------------------------------------------------------------
    // Resolve the remote address

    memset(&dest,0,sizeof(struct sockaddr_in));

    if (HostName){
        hp = gethostbyname(HostName);

        if (hp == NULL){
            // If address is not a host name, assume its a x.x.x.x format address.
            addr = inet_addr(HostName);
            if (addr == INADDR_NONE) {
                // Not a number format address either.  Give up.
                fprintf(stderr,"Unable to resolve %s\n",HostName);
                exit(-1);
            }
            dest.sin_addr.s_addr = addr;
            dest.sin_family = AF_INET;

        }else{
            // Resolved to a host name.
            memcpy(&(dest.sin_addr),hp->h_addr,hp->h_length);
            dest.sin_family = hp->h_addrtype;
        }
    }else{
        // No host name specified.  Use loopback address by default.
        printf("No host name specified.  Using 'loopback' as target\n");
        addr = inet_addr("127.0.0.1");
        dest.sin_addr.s_addr = addr;
        dest.sin_family = AF_INET;
    }

    dest.sin_port = htons(PortNum); 

    //-------------------------------------------------------------------
    // Open socket for reception of regular UDP packets.
    {
        struct sockaddr_in local;

    	local.sin_family = AF_INET;
	    local.sin_addr.s_addr = INADDR_ANY;

      	local.sin_port = htons(PortNum);
        if (HostName){
            // Bind to a different UDP port if just running it for sending a UDP
            // packet because we need the port free for the listening side running
            // on the same computer.
            local.sin_port = htons(PortNum+1);
        }
        sockUDP = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

        if (sockUDP == INVALID_SOCKET){
            perror("couldn't create UDP socket");
            exit(-1);
        }

    	if (bind(sockUDP,(struct sockaddr*)&local,sizeof(local) ) == SOCKET_ERROR) {
    	    #ifdef _WIN32
                int err;
                err = WSAGetLastError();
                if (err != WSAEADDRINUSE){
    		        perror("bind() failed with error");
            		WSACleanup();
                    exit(-1);
                }else{
                    printf("Listen port already in use\n");
                }
            #else
                perror("bind failed");
                exit(-1);
            #endif
    	}
    }

    //-------------------------------------------------------------------

    if (HostName){
        // Just send a UDP packet.
        SendUDP(PosRequested, DeltaRequested);
    }else{
        printf("listening on UDP\n");
        RunStepping();
    }
    return 0;
}

//--------------------------------------------------------------------------
// Check if new UDP packet is here
//--------------------------------------------------------------------------
int CheckUdp(int * NewPos, int * IsDelta)
{
    char recvbuf[MAX_PACKET];
    FD_SET rws;
    int bread;
    struct sockaddr_in from;
    int fromlen = sizeof(from);
    int a;

    {
        // Check for stuff that might come back.
        TIMEVAL timeout;

        FD_ZERO(&rws);
        FD_SET(sockUDP, &rws);
        #ifndef _WIN32
            FD_SET(STDIN_FILENO, &rws);
        #endif
        timeout.tv_sec = 0;
        timeout.tv_usec = 500;

        a = select(10, &rws, NULL, NULL, &timeout);
        if (a == 0){
            // There is no received data.
            return FALSE;
        }
    }

    // Check the UDP socket.
    if (FD_ISSET(sockUDP, &rws)){
        memset(recvbuf, 0, sizeof(recvbuf));
        bread = recvfrom(sockUDP,recvbuf,MAX_PACKET,0,(struct sockaddr*)&from, &fromlen);
        if (bread == SOCKET_ERROR){
            // This happens after we sent something out to another machine and the port is 
            // unavailable.  Under Linux, but not under windows, we subsequently get a socket
            // error - probably on account of the ICMP port unreachable packet that comes back.
            perror("UDP socket error");
            #ifndef _WIN32
                printf("(could be caused by destination port unreachable)\n");
            #endif
        }else{
            {    
                Udp_t * Udp;
                Udp = (Udp_t *) recvbuf;
                
                printf("UDP from %s ", inet_ntoa(from.sin_addr));
                printf("x=%d  y=%d  %s\n", Udp->xpos, Udp->ypos, Udp->IsAdjust ? "Adj":"");
                *NewPos = Udp->xpos;
                *IsDelta = Udp->IsAdjust;
            }   
        }
        return TRUE;
    }
    return FALSE;
}


#ifndef _WIN32
//-----------------------------------------------------------------------------------
//  This program based on "How to access GPIO registers from C-code on the Raspberry-Pi
//  Example program
//  Blinks camerae LED on raspberry Pi model B+ and Raspberry Pi 2 model B
//-----------------------------------------------------------------------------------

#define RASPBERRY_PI 2
#if RASPBERRY_PI == 2 
    // it's a rapsberry pi 2.  Peripherals are mapped in a different location.
    #define BCM2708_PERI_BASE        0x3f000000 // For Raspberry pi 2 model B
#endif
#if RASPBERRY_PI == 1
    #define BCM2708_PERI_BASE        0x20000000 // For raspberry pi model B+
#endif

#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) // GPIO controller
#define TIMER_BASE               (BCM2708_PERI_BASE + 0x3000)   // Interrupt registers (with timer)
 
//#include <stdio.h>
//#include <stdlib.h>
//#include <fcntl.h>
//#include <sys/mman.h>
//#include <unistd.h>
 
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
 
#include <fcntl.h>
#include <sys/mman.h>

void TestTimer(void);
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
#endif
int CurrentPos = 0;

#define TICK_SIZE 200    // Algorithm tick rate, microsconds.  Take at least two ticks per step.
#define TICK_ERROR 250   // Tick must not exceed this time.

#define NUM_RAMP_STEPS 29
static const char RampUp[NUM_RAMP_STEPS]
 = {25,30,35,40,45,50,55,60,65,70,75,80,85,90,95,100,103,106,108,110,112,114,116,118,120,122,124,126,128}; // Also use for ramp down.

typedef struct {
	int Pos;
	int Target;
	//unsigned char RampUp[20,30,40,50,60,70,70,90,100,105,110,115,120,124,128]; // Also use for ramp down.
	int Speed; // 128 = 1 tick per clock on/off, 64 = 2 ticks per, 1 = 256 ticks per.
	int Dir;  // 1 or -1.
	int CountDown;
	int Wait; // Start/stop delay.
	int RampIndex;
	int MaxSpeed;
	
	// GPIO line definitions
	unsigned int ENABLE;
	unsigned int DIR;
	unsigned int CLOCK;
}stepper_t;

stepper_t motors[3];

#define ABS(a) (a > 0 ? a : -(a))


void DoMotor(stepper_t * motor)
{
	int ToGo;
		
	if (motor->Wait){
		motor->Wait -= 1;
	}else{
		ToGo = motor->Target - motor->Pos;
		
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
					if (motor->RampIndex < NUM_RAMP_STEPS) motor->Speed = RampUp[motor->RampIndex];
					if (ToGoAbs < NUM_RAMP_STEPS){
						// Ramp down if getting close.
						if (RampUp[ToGo] < motor->Speed) motor->Speed = RampUp[ToGoAbs];
					}
					if (motor->Speed > motor->MaxSpeed) motor->Speed = motor->MaxSpeed;
					
					if (motor->Pos != motor->Target){
						// Start the next step.
						motor->CountDown += 256;
						// 128 and above means clock high.
						GPIO_SET = motor->CLOCK;
					}else{
						motor->Wait = 1;
					}
				}else if (motor->CountDown < 128){
					GPIO_CLR = motor->CLOCK;
					// 127 or below means clock low.
				}
			}
		}else{
			if (motor->Speed){
				GPIO_SET = motor->ENABLE; // turn off the motor.
				motor->Speed = 0;
			}
		}
	}
	//printf("Cl:%d Wait %d  ToGo:%3d  CntDwn:%3d Speed%3d\n",motor->CountDown > 128, motor->Wait, motor->Target - motor->Pos, motor->CountDown, motor->Speed);
}


void RunStepping(void)
{
	int time1, time2;
	int numticks;
	int flag;
	stepper_t * motor;
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
	motors[0].Target = 850;
	motors[0].ENABLE = STEP_ENA1;
	motors[0].DIR = STEP_DIR1;
	motors[0].CLOCK = STEP_CLK1;
	motors[0].MaxSpeed = 128;
	

	motors[1].Pos = 0;
	//motors[1].Target = 300;
	motors[1].ENABLE = STEP_ENA2;
	motors[1].DIR = STEP_DIR2;
	motors[1].CLOCK = STEP_CLK2;
	motors[1].MaxSpeed = 128;
	
	motors[2].Pos = 0;
	//motors[2].Target = -500;
	motors[2].ENABLE = STEP_ENA3;
	motors[2].DIR = STEP_DIR3;
	motors[2].CLOCK = STEP_CLK3;
	motors[2].MaxSpeed = 40;
	
	flag = 1;
	
	numticks = 0;
	time1 = *(timerreg+1);
	for(;;){
		for (;;){ // Busywait for next tick interval (usleep system call has too much jitter)
			int delta;
			time2 = *(timerreg+1);
			delta = time2-time1;
			if (delta >= TICK_SIZE){
				if (delta > TICK_ERROR){
					printf("tick too long!");
					time2 = *(timerreg+1);
				}
				time1 = time2;
				break;
			}
		}
		numticks ++;
		
		DoMotor(&motors[0]);
		DoMotor(&motors[1]);
		DoMotor(&motors[2]);
		
		if (motors[0].Speed == 0 && motors[1].Speed == 0 && motors[2].Speed == 0){
			if (flag){
				printf("return home\n");
				flag = 0;
				motors[0].Target = 0;
				motors[1].Target = 0;
				motors[2].Target = 0;
			}else{
				break;
			}
		}			
	}
	return;

}


char OutString[100];
double input, output;
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
           int PosRecvd;
           int IsDelta;
           CheckUdp(&PosRecvd, &IsDelta);
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
