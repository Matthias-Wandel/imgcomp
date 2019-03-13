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

#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) /* GPIO controller */
 
//#include <stdio.h>
//#include <stdlib.h>
//#include <fcntl.h>
//#include <sys/mman.h>
//#include <unistd.h>
 
#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)
 
int  mem_fd;
void *gpio_map;
 
// I/O access
volatile unsigned *gpio;
  
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
 
void setup_io();
 
#include <fcntl.h>
#include <sys/mman.h>

//--------------------------------------------------------------------------------
// Set up a memory regions to access GPIO
//--------------------------------------------------------------------------------
void setup_io()
{
    // open /dev/mem 
	if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
	    printf("can't open /dev/mem \n");
	    exit(-1);
	}

	/* mmap GPIO */
	gpio_map = mmap(
		NULL,             //Any adddress in our space will do
		BLOCK_SIZE,       //Map length
		PROT_READ|PROT_WRITE,// Enable reading & writting to mapped memory
		MAP_SHARED,       //Shared with other processes
		mem_fd,           //File to map
		GPIO_BASE         //Offset to GPIO peripheral
	);

	close(mem_fd); //No need to keep mem_fd open after mmap

	if (gpio_map == MAP_FAILED) {
		printf("mmap error %d\n", (int)gpio_map);//errno also set!
		exit(-1);
	}

	// Always use volatile pointer!
	gpio = (volatile unsigned *)gpio_map;
}

#define STEP_ENA (1<<21)  // GPIO21: Enable
#define STEP_DIR (1<<26)  // GPIO26: Direction
#define STEP_CLK (1<<20)  // GPIO20: Clock
#endif
int CurrentPos = 0;
int IsEnabled = FALSE;
int Direction = 0;
int StepDelay = 5000;
int CurrentSpeed = 0;
int SpeedTarget;

#define ABS(a) (a > 0 ? a : -(a))
void RunStepping(void)
{
    int CurrentSpeed, LastSpeed;

    // Set up gpi pointer for direct register access
	setup_io();

    INP_GPIO(21); // must use INP_GPIO before we can use OUT_GPIO
    OUT_GPIO(21);
    INP_GPIO(26);
    OUT_GPIO(26);
    INP_GPIO(20);
    OUT_GPIO(20);

    GPIO_SET = STEP_ENA | STEP_DIR;
    GPIO_CLR = STEP_CLK | STEP_ENA;
    GPIO_SET = STEP_ENA;
    GPIO_CLR = STEP_CLK;

    PosRequested = 0;
    CurrentSpeed = LastSpeed = 0;
    CurrentPos = 600; // Just to have some initial work.
    for (;;){
        int ToTarget;
        int PosRecvd;
        int IsDelta;
               
        if (CheckUdp(&PosRecvd, &IsDelta)){
            //printf("UDP pos request: %d\n",PosRecvd);
            //PosRecvd = PosRecvd * 9;
            PosRecvd = PosRecvd * 32*4;
            if (!IsDelta){
                // New motion position to aim for.
                PosRequested = PosRecvd;
            }else{
                // Adjust the reference position (fix offsets)
                CurrentPos -= PosRecvd;
            }
        }
        GPIO_CLR = STEP_CLK;
        
        ToTarget = PosRequested - CurrentPos;
        
        // Compute speed to try to get to rapm up to, also causing
        // slowdown ad approaching target.
        if (ToTarget){
            SpeedTarget = (int)(sqrt(ABS(ToTarget))*4000);
            if (SpeedTarget > 400000) SpeedTarget = 400000;
            if (ToTarget < 0) SpeedTarget = - SpeedTarget;
        }else{
            SpeedTarget = 0;
        }
        
        {
            // Limit acceleration / deceleration
            int SpDiff, SpAdjust;
            SpDiff = SpeedTarget-CurrentSpeed;
            SpAdjust = SpDiff;
            if (StepDelay > 2000) StepDelay = 2000;
            //printf("sdely=%d\n",StepDelay);
            if (SpAdjust > StepDelay) SpAdjust = StepDelay*2;
            if (SpAdjust < -StepDelay) SpAdjust = -StepDelay*2;
            CurrentSpeed += SpAdjust;
        }
        
        // Come to a halt.
        if (ToTarget == 0 && ABS(CurrentSpeed) < 10000){
            CurrentSpeed = 0;
        }
        
       
        // Potentially reverse direction output.
        if (CurrentSpeed > 0 && LastSpeed <= 0) GPIO_CLR = STEP_DIR;
        if (CurrentSpeed < 0 && LastSpeed >= 0) GPIO_SET = STEP_DIR;        
        
        
        if (CurrentSpeed == 0){
            if (ToTarget == 0){
                // Speed 0.  Came to a stop on target
                GPIO_CLR = STEP_ENA;
            }
            usleep(2000);
            StepDelay = 2000;
        }else{
            if (LastSpeed == 0){
                // Was stopped.  Restart.
                GPIO_SET = STEP_ENA;
                usleep(1000);
            }
            
            StepDelay = 10000000/ABS(CurrentSpeed);
            if (StepDelay > 2000) StepDelay = 2000;
                 
            // Send out one pulse.
            
            GPIO_SET = STEP_CLK;
            usleep(StepDelay);
           
            CurrentPos += CurrentSpeed > 0 ? 1 : -1;
        }
        
        //printf("ToTarget = %d  Speed=%d st=%d\n",ToTarget, CurrentSpeed,SpeedTarget);
        //if (ToTarget == 0 && CurrentSpeed == 0) exit(0);
        
        LastSpeed = CurrentSpeed;
    }
}
