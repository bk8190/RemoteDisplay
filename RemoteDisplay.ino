/*
 Copyright (C) 2011 J. Coliz <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 */

/**
 * Example RF Radio Ping Pair
 *
 * This is an example of how to use the RF24 class.  Write this sketch to two different nodes,
 * connect the role_pin to ground on one.  The ping node sends the current time to the pong node,
 * which responds by sending the value back.  The ping node can then see how long the whole cycle
 * took.
 */

#include <SPI.h>
#include <cstring>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"

//
// Hardware configuration
//

// Set up nRF24L01 radio on SPI bus plus pins 9 & 10
RF24 radio(9,10);

// sets the role of this unit in hardware.  Connect to GND to be the 'pong' receiver
// Leave open to be the 'ping' transmitter
const int role_pin = 7;

// Radio pipe addresses for the 2 nodes to communicate.
const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL };

//
// Role management
//
// Set up role.  This sketch uses the same software for all the nodes
// in this system.  Doing so greatly simplifies testing.  The hardware itself specifies
// which node it is.
//
// This is done through the role_pin
//

// The various roles supported by this sketch
typedef enum { role_base = 1, role_mobile = 2} role_e;

// The debug-friendly names of those roles
const char* role_friendly_name[] = { "invalid", "Base station", "Mobile"};

// The role of the current running sketch
role_e role;



#define PAYLOAD_SIZE 32
uint8_t tx_buf[PAYLOAD_SIZE+1];
uint8_t rx_buf[PAYLOAD_SIZE+1];

typedef struct
{
	uint8_t command_id;
	uint8_t command_data[31];
} tx_payload_t;

#define BEGIN_DELIM '<'
#define END_DELIM   '>'

typedef enum 
{

	CMD_TEXT_LINE_1      = 't',// data is a 
	CMD_TEXT_LINE_2      = 'u',
	CMD_BACKLIGHT        = 'l',// data is a uint8
	CMD_DISPLAY          = 'd', 
	CMD_SCROLL           = 's',
	CMD_BUTTON           = 'b',
} command_t;

typedef struct
{
	char data[16];
} text_payload_t;

void setup(void)
{
	memset(tx_buf, 0, sizeof(tx_buf));
	memset(rx_buf, 0, sizeof(tx_buf));

	// set up the role pin
	pinMode(role_pin, INPUT);
	digitalWrite(role_pin,HIGH);
	delay(20); // Just to get a solid reading on the role pin

	// read the address pin, establish our role
	if ( ! digitalRead(role_pin) )
	role = role_base;
	else
	role = role_mobile;

	//
	// Print preamble
	//

	Serial.begin(57600);
	printf_begin();
	printf("\n\rRemoteDisplay\n\r");
	printf("ROLE: %s\n\r",role_friendly_name[role]);

	//
	// Setup and configure rf radio
	//

	radio.begin();

	// optionally, increase the delay between retries & # of retries
	radio.setRetries(15,15);

	// optionally, reduce the payload size.  seems to
	// improve reliability
	radio.setPayloadSize(32);

	//
	// Open pipes to other nodes for communication
	//

	// This simple sketch opens two pipes for these two nodes to communicate
	// back and forth.
	// Open 'our' pipe for writing
	// Open the 'other' pipe for reading, in position #1 (we can have up to 5 pipes open for reading)

	if ( role == role_base )
	{
		//radio.setPALevel(RF24_PA_HIGH);
		radio.openWritingPipe(pipes[0]);
		radio.openReadingPipe(1,pipes[1]);
	}
	else
	{
		radio.setPALevel(RF24_PA_LOW);
		radio.openWritingPipe(pipes[1]);
		radio.openReadingPipe(1,pipes[0]);
	}

	//
	// Start listening
	radio.startListening();

	//
	// Dump the configuration of the rf unit for debugging
	//

	radio.printDetails();
}

void loop(void)
{
	if (role == role_base)
	{
		loop_base();
	}

	if ( role == role_mobile )
	{
		loop_mobile();
	}
}


#define MOBILE_SLEEP_INTERVAL 3000
#define MOBILE_WAKE_INTERVAL 200

#define BASE_RETRY_INTERVAL 50
#define BASE_MAX_WAIT 5000
#define BASE_MAX_RETRIES (BASE_MAX_WAIT/BASE_RETRY_INTERVAL)

typedef enum
{
	MOBILE_LISTENING,
	MOBILE_SLEEPING
} mobile_state_t;

typedef struct
{
	bool tx_buf_full;
	int  tx_buf_offset;

	bool got_start_delim;
} base_t;
base_t base;

void base_reset_parsing()
{
	base.got_start_delim = false;
	base.tx_buf_full = false;
	base.tx_buf_offset = 0;
}

void loop_base()

{
	int try_num;
	char b;
	int i;

	if (!base.tx_buf_full)
	{
		// Discard up to the opening delimiter
		while (!base.got_start_delim && Serial.available())
		{
			b = Serial.read();
			if (b == BEGIN_DELIM)
			{
				base.got_start_delim = true;
				tx_buf[0] = b;
				base.tx_buf_offset = 1;
			}
		}

		// Read up to the closing delimiter
		while (base.got_start_delim && Serial.available())
		{
			b = Serial.read();
			tx_buf[base.tx_buf_offset++] = b;
			if (b == END_DELIM)
			{
				base.tx_buf_full = true;
				break;
			}

			if (base.tx_buf_offset == PAYLOAD_SIZE)
			{
				printf("ERROR: parsing buffer overflow");
				base_reset_parsing();
				break;
			}
		}
	}

	// If we have a complete message in our buffer
	if (base.tx_buf_full)
	{
		Serial.print("Now sending \"");
		for (i=0; i<PAYLOAD_SIZE; i++)
		{
			b = tx_buf[i];
			if (b == '<' || b == '>')
				Serial.print('_');
			else if(b != '\0')
				Serial.print(b);
		}
		Serial.println("\"");

		bool ok;
		for (try_num=1; try_num<BASE_MAX_RETRIES; try_num++)
		{
			radio.stopListening();
			ok = radio.write( tx_buf, PAYLOAD_SIZE );
			radio.startListening();

			if (ok)
				break;
			
			//printf("send attempt %u: failed\n\r", try_num);
			delay(BASE_RETRY_INTERVAL);
		}

		if (ok)
		{
			printf("send attempt %u: YOLO\n\r", try_num);
			base_reset_parsing();
			memset(tx_buf, 0, sizeof(tx_buf));
		}
		else
		{
			printf("overall failed.\n\r");
		}
	}

	// See if the mobile sent us anything
	radio.startListening();

	// Wait here until we get a response, or timeout (250ms)
	unsigned long started_waiting_at = millis();
	bool timeout = false;
	while ( ! radio.available() && ! timeout )
		if (millis() - started_waiting_at > 500 )
			timeout = true;

	// Describe the results
	if(!timeout )
	{
		// Grab the response, compare, and send to debugging spew
		//unsigned long got_time;
		radio.read( rx_buf, PAYLOAD_SIZE );

		// Spew it
		printf("Got: \"%s\"\r\n", rx_buf);
	}
}

void loop_mobile()
{    
	int i;
	bool done = false;

	// if there is data ready
	if ( radio.available() )
	{
		// Process the payloads until we've gotten everything
		while (!done)
		{
			// Fetch the payload, and see if this was the last one.
			done = radio.read( &rx_buf, PAYLOAD_SIZE);
			
			// Spew it
			printf("Got payload \"%s\"...\r\n", rx_buf);
			for (i=0; i<PAYLOAD_SIZE; i++) {
				printf("%-2u ",i);
				tx_buf[i] = rx_buf[i];
			}
			printf("\r\n");
			for (i=0; i<PAYLOAD_SIZE; i++) {
				printf("%02hhx ", rx_buf[i]);
				tx_buf[i] = rx_buf[i];
			}
			printf("\r\n");
			for (i=0; i<PAYLOAD_SIZE; i++) {
				printf("%c   ", rx_buf[i]);
			}
			printf("\r\n");


			int try_num; bool ok;
			// Send the response back.
			for (try_num=1; try_num<BASE_MAX_RETRIES; try_num++)
			{
				radio.stopListening();
				ok = radio.write( tx_buf, PAYLOAD_SIZE );
				radio.startListening();

				//printf("send attempt %u: failed\n\r", try_num);
				delay(BASE_RETRY_INTERVAL);
				if (ok)
					break;
				
			}
			printf("Sent response, try %u.\n\r", try_num);
		}

		// Now, resume listening so we catch the next packets.
		delay(MOBILE_WAKE_INTERVAL);
	}
	else
	{
		printf("Sleeping\r\n");
		radio.stopListening();
		delay(MOBILE_SLEEP_INTERVAL);

		printf("Listening\r\n");
		radio.startListening();
		delay(MOBILE_WAKE_INTERVAL);
	}
}