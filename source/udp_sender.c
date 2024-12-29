#include <3ds.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define SOC_ALIGN 0x1000
#define SOC_BUFFERSIZE 0x100000

#define SIZE_TOUCH_X 320
#define SIZE_TOUCH_Y 240



static u32* SOC_buffer = NULL;

__attribute__((format(printf, 1, 2))) void failExit(const char* fmt, ...);

char ip[] = "192.168.0.255";


// const static char ip[] = "127.0.0.1";
#define SERVERPORT "6901"
#define BUFLEN 3
char buf[BUFLEN];

int sockfd = -1;

void socShutdown() {
	printf("waiting for socExit...\n");
	socExit();
}

void send_buf(int sock, const void* buf, size_t bufsize, const struct addrinfo* addr) {
	if (sendto(sock, buf, bufsize, 0, addr->ai_addr,
		addr->ai_addrlen) == -1) {
		printf("[error] send gefailed");
	}
}

int main(int argc, char** argv) {
	int rv;

	int numbytes;
	struct addrinfo hints, * servinfo, * p;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;  // set to AF_INET to use IPv4
	hints.ai_socktype = SOCK_DGRAM;

	gfxInitDefault();

	// register gfxExit to be run when app quits
	// this can help simplify error handling
	atexit(gfxExit);

	consoleInit(GFX_TOP, NULL);

	// allocate buffer for SOC service
	SOC_buffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);

	if (SOC_buffer == NULL) {
		failExit("memalign: failed to allocate\n");
	}

	// Now intialise soc:u service
	if ((rv = socInit(SOC_buffer, SOC_BUFFERSIZE)) != 0) {
		failExit("socInit: 0x%08X\n", (unsigned int)rv);
	}

	// register socShutdown to run at exit
	// atexit functions execute in reverse order so this runs before gfxExit
	atexit(socShutdown);

	// libctru provides BSD sockets so most code from here is standard



	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		failExit("talker: socket");
	}

	int broadcastEnable = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));

	// if (p == NULL) {
	// 	failExit("talker: failed to create socket\n");
	// }

	// if ((numbytes = sendto(sockfd, buf, strlen(buf), 0,
	//          p->ai_addr, p->ai_addrlen)) == -1) {
	//     printf("[error] send gefailed");
	// }
	unsigned int counter = 0;

	if ((rv = getaddrinfo(ip, SERVERPORT, &hints, &servinfo)) != 0) {
		printf("get_addr_info %s\n", gai_strerror(rv));
	}

	while (aptMainLoop()) {
		gspWaitForVBlank();

		hidScanInput();

		touchPosition touch;

		// Read the touch screen coordinates
		hidTouchRead(&touch);
		// printf("\x1b\n[2;0H%03d; %03d", touch.px, touch.py);

		// counter++;
		// if (counter % 2 != 0) continue;

		int new_x = touch.px;
		int new_y = touch.py;
		if (new_x == buf[0] && new_y == buf[1]) continue;

		buf[0] = new_x;
		buf[1] = new_y;
		// for (size_t i = 0; i < BUFLEN; i++) {
		// 	char val = 0;
		// 	if (i % 3 == 0)
		// 		val = (char);
		// 	else if (i % 3 == 1)
		// 		val = (char)touch.py;
		// 	buf[i] = val;
		// }
	// for (size_t i = 1; i < 2; i++) {
		// ip[11] = (i / 10) + 0x30;
		// ip[12] = i % 10 + 0x30;
		// printf("ip: %s\n", ip);


		p = servinfo;
		if ((numbytes = sendto(sockfd, buf, BUFLEN, 0, p->ai_addr,
			p->ai_addrlen)) == -1) {
			printf("[error] send gefailed\n");
		}
		// }


	}

	freeaddrinfo(servinfo);

	close(sockfd);

	return 0;
}

//---------------------------------------------------------------------------------
void failExit(const char* fmt, ...) {
	//---------------------------------------------------------------------------------

	if (sockfd > 0) close(sockfd);

	va_list ap;

	printf(CONSOLE_RED);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf(CONSOLE_RESET);
	printf("\nPress B to exit\n");

	while (aptMainLoop()) {
		gspWaitForVBlank();
		hidScanInput();

		u32 kDown = hidKeysDown();
		if (kDown & KEY_B) exit(0);
	}
}
