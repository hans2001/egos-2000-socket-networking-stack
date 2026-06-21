/*
 * Description: a simple raw Ethernet transmit smoke test
 */

#include "app.h"
#include <string.h>

int main(int argc, char** argv) {
	if (argc != 1) {
		INFO("usage: net_raw_test");
		return -1;
	}

	unsigned char dummy_frame[60] = {
		// Destination MAC (Broadcast)
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff,

		// Source MAC (your device's MAC)
		0x02, 0x00, 0x00, 0x00, 0x00, 0x01,

		// Ethertype (0x88B5 = Dummy/Experimental)
		0x88, 0xB5,

		// Payload (46 bytes of arbitrary data)
		'H', 'E', 'L', 'L', 'O', '-', 'F', 'R', 'O', 'M',
		'-', 'N', 'E', 'T', '-', 'R', 'A', 'W', '-', 'T',
		'Y', '-', 'P', 'A', 'C', 'K', 'E', 'T', '-', 'T', 
		'E', 'S', 'T', '-', 'Y', 'A', 'Y', '!', ' ', 'R', 
		'/', 'W', '@', '$', 0x00, 0x00
	};
	net_send(60, (char*)(&dummy_frame));
	SUCCESS("Raw Ethernet transmit path completed");

	return 0;
}
