

void mobile_rx();

void setup_mobile()
{

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

		printf("Got payload \"%s\"...\r\n", rx_buf);
		for (i=0; i<PAYLOAD_SIZE; i++) {
			tx_buf[i] = rx_buf[i];
		}


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
		printf("Sent response, try %u.\n\r", try_num);
	}

}