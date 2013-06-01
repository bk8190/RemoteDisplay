

typedef struct
{
	Adafruit_RGBLCDShield lcd;
	bool needs_tx;
} 
mobile_t;
mobile_t mobile;

void mobile_rx();
void mobile_process_command();
void mobile_delay();

void setup_mobile()
{
	mobile.lcd = Adafruit_RGBLCDShield();
	mobile.lcd.begin(16,2);
	mobile.lcd.print("Hello");

	mobile.needs_tx = false;
}

void loop_mobile()
{    
	int i;
	bool done = false;

	// if there is data ready
	if ( radio.available() )
	{
		mobile_rx();
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

void mobile_rx()
{
	int try_num, i; 
	bool ok, done = false;

	// Process the payloads until we've gotten everything
	while (!done)
	{
		// Fetch the payload, and see if this was the last one.
		done = radio.read( &rx_buf, PAYLOAD_SIZE);

		mobile_process_command();

		if (mobile.needs_tx)
		{
			// Send the response back.
			for (try_num=1; try_num<MOBILE_MAX_TX_RETRIES; try_num++)
			{
				radio.stopListening();
				ok = radio.write( tx_buf, PAYLOAD_SIZE );
				radio.startListening();

				//printf("send attempt %u: failed\n\r", try_num);
				delay(MOBILE_TX_RETRY_INTERVAL);
				if (ok)
					break;

			}
			mobile.needs_tx = false;
			printf("Sent response, try %u.\n\r", try_num);
		}
	}

}

void mobile_cmd_backlight();
void mobile_cmd_text();
void mobile_cmd_clear();
void mobile_cmd_scroll();

void mobile_process_command()
{
	int i;
	printf("Got payload \"%s\"...\r\n", rx_buf);

	switch (rx_buf[1])
	{
		case CMD_BACKLIGHT:
			mobile_cmd_backlight();
			break;
		case CMD_TEXT:
			mobile_cmd_text();
			break;
		case CMD_CLEAR:
			mobile_cmd_clear();
			break;
		//case CMD_SCROLL:
		//	mobile_cmd_scroll();
		//	break;

		default:
			for (i=0; i<PAYLOAD_SIZE; i++) {
				tx_buf[i] = rx_buf[i];
			}
			mobile.needs_tx = true;

	}
}

void mobile_cmd_backlight()
{
	printf("Setting backlight '%c'\n", rx_buf[2]);
	switch (rx_buf[2])
	{
		case 'r': //red
			mobile.lcd.setBacklight(0x1);
			break;
		case 'y': //yellow
			mobile.lcd.setBacklight(0x3);
			break;
		case 'g': //green
			mobile.lcd.setBacklight(0x2);
			break;
		case 't': //teal
			mobile.lcd.setBacklight(0x6);
			break;
		case 'b': //blue
			mobile.lcd.setBacklight(0x4);
			break;
		case 'v': //violet
			mobile.lcd.setBacklight(0x5);
			break;
		case 'w': //white
			mobile.lcd.setBacklight(0x7);
			break;

		case 'o': //off
		default:
			mobile.lcd.setBacklight(0x0);
	}
}

void mobile_cmd_text()
{
	char line = rx_buf[2] - 'a';
	char col  = rx_buf[3] - 'a';

	char text[PAYLOAD_SIZE+1];
	memset(text, 0, PAYLOAD_SIZE+1);

	int i;
	for (i=4; i<PAYLOAD_SIZE; i++)
	{
		if (rx_buf[i] == '>')
			break;
		text[i-4] = rx_buf[i];
	}

	printf("Text line %u, col %u: <%s>\n", line, col, text);
	mobile.lcd.setCursor(col, line);
	mobile.lcd.print(text);
}

void mobile_cmd_clear()
{
	char arg = rx_buf[2];
	switch (arg)
	{
		case 'a':
			printf("Clear line 1\n");
			mobile.lcd.setCursor(0,0);
			mobile.lcd.print("                ");
			break;
		case 'b':
			printf("Clear line 2\n");
			mobile.lcd.setCursor(0,1);
			mobile.lcd.print("                ");
			break;
		default:
			printf("Clear all\n");
			mobile.lcd.clear();
	}
}

void mobile_cmd_scroll()
{
	char arg = rx_buf[2];
	switch (arg)
	{
		case 'a':
			printf("Autoscroll on\n");
			mobile.lcd.autoscroll();
			break;
		case 'b':
		default:
			printf("Autoscroll off\n");
			mobile.lcd.noAutoscroll();
			break;
	}
}