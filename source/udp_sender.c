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
#include <math.h>

#define SOC_ALIGN 0x1000
#define SOC_BUFFERSIZE 0x100000

#define SIZE_TOUCH_X 320
#define SIZE_TOUCH_Y 240



static u32* SOC_buffer = NULL;

__attribute__((format(printf, 1, 2))) void failExit(const char* fmt, ...);

// const static char ip[] = "192.168.0.111";
const static char ip[] = "192.168.0.255";


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

void HSVToRGB(float hue, float sat, float val) {
	float r = 0, g = 0, b = 0;

	if (sat == 0) {
		r = val;
		g = val;
		b = val;
	}
	else {
		int i;
		float f, p, q, t;

		if (hue == 360)
			hue = 0;
		else
			hue = hue / 60;

		i = (int)trunc(hue);
		f = hue - i;

		p = val * (1.0f - sat);
		q = val * (1.0f - (sat * f));
		t = val * (1.0f - (sat * (1.0f - f)));

		switch (i) {
		case 0:
			r = val;
			g = t;
			b = p;
			break;

		case 1:
			r = q;
			g = val;
			b = p;
			break;

		case 2:
			r = p;
			g = val;
			b = t;
			break;

		case 3:
			r = p;
			g = q;
			b = val;
			break;

		case 4:
			r = t;
			g = p;
			b = val;
			break;

		default:
			r = val;
			g = p;
			b = q;
			break;
		}

	}
	buf[0] = r * 255;
	buf[1] = g * 255;
	buf[2] = b * 255;
}

int old_x = 0;
int old_y = 0;

int update_color() {
	hidScanInput();

	touchPosition touch;

	// Read the touch screen coordinates
	hidTouchRead(&touch);


	u32 keys = hidKeysDownRepeat();
	if (keys & KEY_START) return -1;
	if (!(keys & KEY_TOUCH)) return 0;

	int new_x = touch.px;
	int new_y = touch.py;

	if (new_x == old_x && new_y == old_y) return 0;
	HSVToRGB(1.125f * (float)new_x, 1.0f, ((float)new_y) / SIZE_TOUCH_Y);
	printf("%f, %f\n", 1.125f * (float)new_x, ((float)new_y));
	// buf[0] = old_x;
	// buf[1] = old_y;
	old_x = new_x;
	old_y = new_y;

	return 1;
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


	if ((rv = getaddrinfo(ip, SERVERPORT, &hints, &servinfo)) != 0) {
		printf("get_addr_info %s\n", gai_strerror(rv));
	}

	while (aptMainLoop()) {
		rv = update_color();
		if (rv == -1) break;
		else if (rv == 0) continue;
		else {

			p = servinfo;
			if ((numbytes = sendto(sockfd, buf, BUFLEN, 0, p->ai_addr,
				p->ai_addrlen)) == -1) {
				printf("[error] send gefailed\n");
			}
		}
		gspWaitForVBlank();
	}

	freeaddrinfo(servinfo);

	close(sockfd);

	return 0;
}

void failExit(const char* fmt, ...) {
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
