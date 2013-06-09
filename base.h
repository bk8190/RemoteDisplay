
typedef struct
{
	bool tx_buf_full;
	int  tx_buf_offset;

	bool got_start_delim;
} 
base_t;
base_t base;


void base_serial_input();
void base_reset_parsing();
void base_tx();
void base_rx();

void setup_base()
{
	Serial.println("base_setup");
}

void loop_base()
{
	// If we don't have a complete message, try reading from the serial port
	if (!base.tx_buf_full)
	{
		base_serial_input();
	}

	// If we have a complete message in our buffer, try to send it.
	if (base.tx_buf_full)
	{
		base_tx();
	}

	// See if the mobile sent us a response
	base_rx();
}


void base_reset_parsing()
{
	base.got_start_delim = false;
	base.tx_buf_full = false;
	base.tx_buf_offset = 0;
}

void base_serial_input()
{
	char b;
	int i;

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

		if (b < ' ' || b > '~')
		{
			printf("Bad chr %u\r\n", b);
			base_reset_parsing();
			break;
		}

		if (b == BEGIN_DELIM)
		{
			Serial.println("Repeat open delim");
			base_reset_parsing();
			break;
		}

		tx_buf[base.tx_buf_offset++] = b;
		if (b == END_DELIM)
		{
			base.tx_buf_full = true;
			break;
		}

		if (base.tx_buf_offset == PAYLOAD_SIZE)
		{
			Serial.println("ERROR: parsing buffer overflow");
			base_reset_parsing();
			break;
		}
	}
}


void base_tx()
{
	char b;
	int i;
	int try_num;


	Serial.print("Sending \"");
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
	for (try_num=1; try_num<BASE_MAX_TX_RETRIES; try_num++) 
	{
		radio.stopListening();
		ok = radio.write( tx_buf, PAYLOAD_SIZE );
		radio.startListening();

		if (ok) break;
		
		delay(BASE_TX_RETRY_INTERVAL);
	}

	if (ok)
	{
		base_reset_parsing();
		memset(tx_buf, 0, sizeof(tx_buf));
	}
	else
	{
		Serial.println("Send failed. <t>");
	}
}

void base_rx()
{
	radio.startListening();

	unsigned long started_waiting_at = millis();
	bool timeout = false;

	while (!timeout)
	{
		// Wait here until we get a response, or timeout
		while ( ! radio.available() && ! timeout )
		{
			if (millis() - started_waiting_at > BASE_RX_TIMEOUT )
				timeout = true;
		}

		// Grab the response
		if(!timeout )
		{
			radio.read( rx_buf, PAYLOAD_SIZE );
			printf("Got: \"%s\"\r\n", rx_buf);
		}
	}
}