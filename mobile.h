
const int MOBILE_CELL_VOLTAGE_PIN = 5;

const int MOBILE_WAKE_PIN = 2;

const int MOBILE_LOW_VOLTAGE = 3.4;

const int isr = 3;


#define col_off    (0x0)
#define col_red    (0x1)
#define col_green  (0x2)
#define col_yellow (0x3)
#define col_teal   (0x6)
#define col_blue   (0x4)
#define col_violet (0x5)
#define col_white  (0x7)

const char* status_msgs[] =
{
	"                ", 
	"Busy            ", // Left
	"Lunch           ", // Right
	"Meeting         ", // Up
	"Inside          ", // Down
	"Out             ", // Select
};

const int status_colors[6] =
{
	col_off,
	col_red,
	col_green,
	col_yellow,
	col_blue,
	col_teal
};

typedef struct
{
	Adafruit_RGBLCDShield lcd;
	bool needs_tx;
	bool interrupted;
	float last_voltage;
	int needs_send_status;
	bool override_light;
} 
mobile_t;
mobile_t mobile;


void mobile_wake()
{
	Serial.println("ISR!!!!!");
	detachInterrupt(isr);
	mobile.interrupted = true;
}

void mobile_rx();
void mobile_process_command();
void mobile_delay();
void mobile_read_voltage();
void mobile_cmd_heartbeat();
void mobile_read_buttons();
void mobile_tx_buffer();

void setup_mobile()
{
	mobile.lcd = Adafruit_RGBLCDShield();
	mobile.lcd.begin(16,2);
	mobile.lcd.print("Hello!   " __DATE__);
	mobile.lcd.setCursor(0,1);
	mobile.lcd.print( __TIME__);
	mobile.lcd.enableButtonInterrupt();

	delay(500);
	mobile.lcd.setBacklight(0);
	mobile.needs_send_status = false;
	mobile.needs_tx          = false;
	mobile.override_light    = false;
	mobile.interrupted       = false;
	pinMode(led_pin, OUTPUT);
	delay(2000);
}

void loop_mobile()
{    
	int i;
	bool done = false;

	digitalWrite(led_pin, 1);

	mobile_read_buttons();
	mobile_read_voltage();


	// if there is data ready
	if ( radio.available()  || mobile.needs_send_status)
	{
		mobile_rx();
	}
	else
	{
		radio.powerDown();
		digitalWrite(led_pin, 0);
		attachInterrupt(isr, mobile_wake, LOW);

		LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);

		detachInterrupt(isr);
		digitalWrite(led_pin, 1);
	}

	// Try to read a packet
	radio.startListening();

	if (!mobile.interrupted)
		delay(MOBILE_WAKE_INTERVAL); // TODO: make this sleep?
	mobile.interrupted = false;
}

void mobile_rx()
{
	bool done = false;

	// Process the payloads until we've gotten everything
	while (!done)
	{
		memset(tx_buf, 0, PAYLOAD_SIZE+1);
		if (mobile.needs_send_status)
		{
			tx_buf[0] = '<';
			tx_buf[1] = 's';
			strncpy(&tx_buf[2], status_msgs[mobile.needs_send_status], 16);
			tx_buf[17] = '>';
			mobile.needs_tx = true;
			mobile.needs_send_status = 0;
			done = true;
		}
		else
		{
			// Fetch the payload, and see if this was the last one.
			done = radio.read(&rx_buf, PAYLOAD_SIZE);
			mobile_process_command();
		}


		if (!mobile.needs_tx)
		{
		//	mobile_cmd_heartbeat();
		}

		if (mobile.needs_tx) {
			mobile_tx_buffer();
		}

		delay(200);
	}

}

void mobile_tx_buffer()
{
	int try_num;
	bool ok;
	delay(100);

	// Send the response back.
	for (try_num=1; try_num<MOBILE_MAX_TX_RETRIES; try_num++)
	{
		radio.stopListening();
		ok = radio.write( tx_buf, PAYLOAD_SIZE );
		radio.startListening();

		//printf("send attempt %u: failed\n\r", try_num);
		delay(MOBILE_TX_RETRY_INTERVAL); // TODO: make this sleep?
		if (ok)
			break;
	}
	mobile.needs_tx = false;
	printf("Sent response, try %u.\n\r", try_num);
}

void mobile_read_buttons()
{
	uint8_t b = mobile.lcd.readButtons();

	if (!mobile.override_light)
		mobile.lcd.setBacklight(col_off);

	//mobile.lcd.setBacklight(b%5);
	if (b)
	{
		if (b & BUTTON_LEFT) {
			mobile.needs_send_status = 1;
		}
		else if (b & BUTTON_RIGHT) {
			mobile.needs_send_status = 2;
		}
		else if (b & BUTTON_UP) {
			mobile.needs_send_status = 3;
		}
		else if (b & BUTTON_DOWN) {
			mobile.needs_send_status = 4;
		}
		else if (b & BUTTON_SELECT) {
			mobile.needs_send_status = 5;
		}

		mobile.lcd.clear();
		mobile.lcd.setCursor(0,0);
		mobile.lcd.print(status_msgs[mobile.needs_send_status]);
		mobile.lcd.setBacklight(status_colors[mobile.needs_send_status]);
		mobile.override_light = false;
	}
}

void mobile_read_voltage()
{
	char tmp[10];

	int val = analogRead(MOBILE_CELL_VOLTAGE_PIN);
	mobile.last_voltage = ((float) val) * (5.0 / 1024);

	dtostrf(mobile.last_voltage, 1, 2, tmp);
	tmp[4] = 'v';
	tmp[5] = '\0';

	printf("v = %s\n", tmp);
	mobile.lcd.setCursor(12,1);
	mobile.lcd.print(tmp);

	if (mobile.last_voltage < MOBILE_LOW_VOLTAGE && mobile.last_voltage > 3.0)
	{
		mobile.lcd.setBacklight(col_off);
		
		mobile.lcd.setCursor(0, 0);
		mobile.lcd.print("BATTERY LOW,");
		mobile.lcd.setCursor(0, 1);
		mobile.lcd.print("UNPLUG NOW!");

		radio.powerDown();

		delay(1000);
		detachInterrupt(isr);
		//LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
	}
}

void mobile_cmd_backlight();
void mobile_cmd_text();
void mobile_cmd_clear();
void mobile_cmd_quicktext();
void mobile_cmd_heartbeat();

void mobile_process_command()
{
	int i;
	printf("Got payload \"%s\"\r\n", rx_buf);

	switch (rx_buf[1])
	{
		case CMD_HEARTBEAT:
			mobile_cmd_heartbeat();
			break;
		case CMD_TEXT:
			mobile_cmd_text();
			break;
		case CMD_QUICKTEXT:
			mobile_cmd_quicktext();
			break;
		case CMD_BACKLIGHT:
			mobile_cmd_backlight();
			break;
		case CMD_CLEAR:
			mobile_cmd_clear();
			break;

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
		case 'r':
			mobile.lcd.setBacklight(col_red); 
			break;
		case 'y':
			mobile.lcd.setBacklight(col_yellow);
			break;
		case 'g':
			mobile.lcd.setBacklight(col_green);
			break;
		case 't':
			mobile.lcd.setBacklight(col_teal);
			break;
		case 'b':
			mobile.lcd.setBacklight(col_blue);
			break;
		case 'v':
			mobile.lcd.setBacklight(col_violet);
			break;
		case 'w':
			mobile.lcd.setBacklight(col_white);
			break;

		case 'o':
		default:
			mobile.lcd.setBacklight(col_off);
	}
	mobile.override_light = true;
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

void mobile_cmd_quicktext()
{
	char text[PAYLOAD_SIZE+1];
	memset(text, 0, PAYLOAD_SIZE+1);

	int i;
	for (i=2; i<PAYLOAD_SIZE; i++)
	{
		if (rx_buf[i] == '>')
			break;
		text[i-2] = rx_buf[i];
	}

	printf("Text: <%s>\n", text);
	mobile.lcd.clear();
	mobile.lcd.setCursor(0, 0);
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

void mobile_cmd_heartbeat()
{
	printf("Heartbeat\n");
	int pos=0;

	memset(rx_buf, 0, PAYLOAD_SIZE+1);
	tx_buf[pos++] = '<';
	tx_buf[pos++] = 'h';

	dtostrf(mobile.last_voltage, 1, 2, &tx_buf[pos]);
	pos += 4;

	tx_buf[pos++] = '>';
	mobile.needs_tx = true;
}
