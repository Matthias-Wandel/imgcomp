// UDP communications code for run stepper program.
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
	int Motion;
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
void SendUDP(int Pan, int Tilt, int IsDelta)
{
    int datasize;
    int wrote;
    Udp_t Buf;
    
    memset(&Buf, 0, sizeof(Buf));

    Buf.Ident = UDP_MAGIC;
    Buf.Level = 0;
    Buf.xpos = Pan;
    Buf.ypos = Tilt;
    Buf.IsAdjust = IsDelta;
	Buf.Motion = 0;
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

    printf("Sent UDP packet, Pan=%5.1f, Tilt=%5.1f\n",Pan/10.0, Tilt/10.0);
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
int PanRequested, TiltRequested;
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
        
        sscanf(argv[2],"%d",&PanRequested);
		
		if (argc > 3){
			sscanf(argv[3],"%d",&TiltRequested);
		}
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
        SendUDP(PanRequested, TiltRequested, DeltaRequested);
    }else{
        printf("listening on UDP\n");
        RunStepping();
    }
    return 0;
}

//--------------------------------------------------------------------------
// Check if new UDP packet is here
//--------------------------------------------------------------------------
int CheckUdp(int * XDeg, int * YDeg, int * Level, int * Motion, int * IsDelta)
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
                printf("pan=%5.1f  Tilt=%5.1f %s\n", Udp->xpos/10.0, Udp->ypos/10.0, Udp->IsAdjust ? "Adj":"");
                *XDeg = Udp->xpos;
				*YDeg = Udp->ypos;
				*Level = Udp->Level;
				*Motion = Udp->Motion;
                *IsDelta = Udp->IsAdjust;
            }   
        }
        return TRUE;
    }
    return FALSE;
}


#ifdef _WIN32
//--------------------------------------------------------------------------
// Windows stub for receiving commands and showing them.
//--------------------------------------------------------------------------
void RunStepping(void)
{
	int Pan,Tilt,Fire,Motion,IsDelta;
    for (;;){
        if (CheckUdp(&Pan, &Tilt, &Fire, &Motion, &IsDelta)){
            printf("UDP pos request: %d,%d  f=%d  motion=%d isdelta:%d\n",Pan, Tilt, Fire, Motion, IsDelta);
        }
	}
}
#endif