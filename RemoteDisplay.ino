/*
Copyright (C) 2013 W. Kulp
Based on Example RF Radio Ping Pair by J. Coliz <maniacbug@ymail.com>

 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 */

#include <stdlib.h>
#include <SPI.h>
#include <cstring>
#include "nRF24L01.h"
#include "LowPower.h"
#include "RF24.h"
#include "printf.h"

#include <Wire.h>
#include <Adafruit_MCP23017.h>
#include <Adafruit_RGBLCDShield.h>

// Set up nRF24L01 radio on SPI bus plus pins 9 & 10
RF24 radio(9,10);

// sets the role of this unit in hardware.  Connect to GND to be the 'pong' receiver
// Leave open to be the 'ping' transmitter
const int role_pin = 6;
const int led_pin = 13;
int led = false;

// Radio pipe addresses for the 2 nodes to communicate.
const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL };

// The various roles supported by this sketch
typedef enum { 
  role_base = 1, role_mobile = 2} 
role_t;

// The debug-friendly names of those roles
const char* role_friendly_name[] = { 
  "invalid", "Base station", "Mobile"};

// The role of the current running sketch
role_t role;


#define PAYLOAD_SIZE 32
char tx_buf[PAYLOAD_SIZE+1];
char rx_buf[PAYLOAD_SIZE+1];

typedef struct
{
  char command_id;
  char command_data[31];
} 
tx_payload_t;

#define BEGIN_DELIM '<'
#define END_DELIM   '>'

typedef enum 
{
  CMD_HEARTBEAT    = 'h',
  CMD_TEXT         = 't', //argc 1: row [a,b]  argc2: col [a,p]  argstring: text (max 16)
  CMD_QUICKTEXT    = 'u', //argstring: text
  CMD_CLEAR        = 'c', //argc: row [a,b] (default clear all)
  CMD_BACKLIGHT    = 'l',

  CMD_TIMEOUT      = 't',
} 
command_t;



void setup_base();
void loop_base();
void setup_mobile();
void loop_mobile();

void setup(void)
{
  memset(tx_buf, 0, sizeof(tx_buf));
  memset(rx_buf, 0, sizeof(tx_buf));

  // set up the role pin
  pinMode(role_pin, INPUT);
  digitalWrite(role_pin,HIGH);
  delay(20); // Just to get a solid reading on the role pin

  // read the address pin, establish our role
  if ( digitalRead(role_pin) )
    role = role_mobile;
  else
    role = role_base;

  Serial.begin(57600);
  printf_begin();
  printf("\n\rRemoteDisplay\n\r");
  printf("ROLE: %s\n\r",role_friendly_name[role]);

  // Setup and configure rf radio
  radio.begin();

  // optionally, increase the delay between retries & # of retries
  radio.setRetries(3,15);
  //radio.setRetries(15,15);
  radio.setPayloadSize(32);

  // Open pipes to other nodes for communication
  if ( role == role_base )
  {
    //radio.setPALevel(RF24_PA_HIGH);
    radio.openWritingPipe(pipes[0]);
    radio.openReadingPipe(1,pipes[1]);
    setup_base();
  }
  else
  {
    radio.setPALevel(RF24_PA_LOW);
    radio.openWritingPipe(pipes[1]);
    radio.openReadingPipe(1,pipes[0]);
    setup_mobile();
  }

  // Start listening
  radio.startListening();

  // Dump the configuration of the rf unit for debugging
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
#define MOBILE_WAKE_INTERVAL 500

#define BASE_RX_TIMEOUT 750
#define BASE_TX_TIMEOUT 12000
#define BASE_TX_RETRY_INTERVAL 50
#define BASE_MAX_TX_RETRIES (BASE_TX_TIMEOUT/BASE_TX_RETRY_INTERVAL)

#define MOBILE_TX_TIMEOUT 5000
#define MOBILE_TX_RETRY_INTERVAL 50
#define MOBILE_MAX_TX_RETRIES (MOBILE_TX_TIMEOUT/MOBILE_TX_RETRY_INTERVAL)


#include "base.h"
#include "mobile.h"
