// Code to read four DHT22 temperature sensors in the house.
// connected to GPIO 14,15,18 and 23
//
// Adjustments made because wirtingpi delay is inaccurate on Pi4.
//
// Compile with : gcc dht22.c -o dht22 -lwiringPi
//
// Matthias Wandel Nov 22 2019
//
#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
 
#define MAX_TIMINGS	85
//#define DHT_PIN	15 // GPIO 14, header pin 8.  Basement DHT22.  Works.
//#define DHT_PIN	16 // GPIO 15, header pin 10.  Master bedroom
//#define DHT_PIN	 1 // GPIO 18, header pin 12.  Furnace room
//#define DHT_PIN	 4 // GPIO 23, header pin 16.  Living room

//-----------------------------------------------------------------------------
// read DHT sensor
//-----------------------------------------------------------------------------
void read_dht_data(int dht_pin, float * temp, float * humid)
{
	uint8_t laststate	= HIGH;
	uint8_t counter		= 0;
	uint8_t j			= 0, i;

    static int data[5] = { 0, 0, 0, 0, 0 };    

    int counts[MAX_TIMINGS]; // For debugging.
 
	data[0] = data[1] = data[2] = data[3] = data[4] = 0;
 
	/* pull pin down for 18 milliseconds */
	pinMode( dht_pin, OUTPUT );
	digitalWrite( dht_pin, LOW );
	usleep( 18000 );
 
	/* prepare to read the pin */
	pinMode( dht_pin, INPUT );
    //delayMicroseconds( 8 );
    usleep(10);
 
	/* detect change and read data */
	for ( i = 0; i < MAX_TIMINGS; i++ )
	{
		counter = 0;
		while ( digitalRead(dht_pin) == laststate )
		{
			counter++;
			delayMicroseconds( 1 );
			if ( counter == 255 ){
				break;
			}
            counts[i] = counter;
		}
		laststate = digitalRead(dht_pin);
 
		if ( counter == 255 )
			break;
 
		/* ignore first 3 transitions */
		if ( (i >= 4) && (i % 2 == 0) )
		{
			/* shove each bit into the storage bytes */
			data[j / 8] <<= 1;
			if ( counter > 30 )
				data[j / 8] |= 1;
			j++;
		}
	}
    //printf("i=%d \n",i);
    //for (j=0;j<i;j+=2){
    //    printf(" %d %d\n",counts[j]-20, counts[j+1]);
    //}
    //printf("\n");
 
	// check we read 40 bits (8bit x 5 ) + verify checksum in the last byte
	// print it out if data is good
	if ( (j >= 40) && (data[4] == ( (data[0] + data[1] + data[2] + data[3]) & 0xFF))) {
		float h = (float)((data[0] << 8) + data[1]) / 10;

		if ( h > 100 ) h = data[0];	// for DHT11

		float c = (float)(((data[2] & 0x7F) << 8) + data[3]) / 10;

		if ( c > 125 ) c = data[2];	// for DHT11
        
		if ( data[2] & 0x80 )c = -c;
		printf( "H: %.1f  T: %.1f \n", h, c);
        if (temp) *temp = c;
        if (humid) *humid = h;
	}else  {
		printf( "Data not good, skip\n" );
        if (temp) *temp = 0;
        if (humid) *humid = 0;
        
        printf("i=%d \n",i);
        for (j=0;j<i;j+=2){
            printf(" %d %d\n",counts[j]-20, counts[j+1]);
        }
        printf("\n");
        
	}
}

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------
int main( void )
{
    float temps[4];
    float humid[4];
    time_t now;
    int a;
 
	if ( wiringPiSetup() == -1 ) exit( 1 );
    
    printf("Basement:    ");
    read_dht_data(15, &temps[0], &humid[0]); // Basement
    printf("Master bedr: ");
    read_dht_data(16, &temps[1], &humid[1]); // Master bedroom
    printf("Furnace rm:  ");    
    read_dht_data(1,  &temps[2], &humid[2]); // Furnace room
    printf("Living room: ");    
    read_dht_data(4,  &temps[3], &humid[3]); // Living room
    
    char TimeString[20];
    now = time(NULL);
    strftime(TimeString, 20, "%d-%b-%y %H:%M:%S", localtime(&now));

    printf(TimeString);
    for (a=0;a<4;a++){
        if (temps[a] || humid[a]){
            printf(", %4.1f,%4.1f",temps[a],humid[a]);
        }else{
            printf(",     ,    ");
        }
    }
    printf("\n");


    FILE * logf = fopen("4temps.txt","a");
    if (logf != NULL){
        fprintf(logf,TimeString);
        for (a=0;a<4;a++){
            if (temps[a] || humid[a]){
                fprintf(logf,", %4.1f,%4.1f",temps[a],humid[a]);
            }else{
                fprintf(logf,",     ,    ");
            }
        }
        fprintf(logf,"\n");
        fclose(logf);
    }
 
	return(0);
}