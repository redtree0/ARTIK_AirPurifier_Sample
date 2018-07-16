#include "basic_function.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <debug.h>
#include <sched.h>
#include <tinyara/progmem.h>
#include <tinyara/fs/smart.h>
#include <tinyara/fs/ioctl.h>
#include <tinyara/spi/spi.h>
#include <sys/stat.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <apps/netutils/netlib.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <cJSON.h>
#include <apps/netutils/wifi/slsi_wifi_api.h>
#include <apps/netutils/wifi/slsi_wifi_utils.h>
#include <tinyara/config.h>
#include <tinyara/fs/fs_utils.h>
#include <shell/tash.h>
#include <artik_error.h>
#include <artik_cloud.h>
#include <artik_wifi.h>
#include "artik_onboarding.h"



#define dustPin 0
#define ledPin 57
#define ArrayLenth 40 //times of collection, 사용자 설정에 따라 변경
#define Dust_target 60 //사용자 설정에 따라 변경
float sensorArray[ArrayLenth]; //Store the average value of the sensor feedback
int sensorArrayIndex = 0;
float dust_voltage;
float dustDensity; //dust sensor value
double avergearray(float* arr, int number);


#define relayPin 55
#define FanCount 300
uint8_t mode_auto = 0; //0 -manual, 1 -auto
int fan_check = 0;
int relay_btn = 1; // 0 -fan off, 1 -fan on
/*
 * TCP
 */
#define BUF_SIZE 256
int g_app_target_port = 5555; //

float dust(void) {
// dust sensor value
	gpio_write(ledPin, LOW);
	float dustVal = analogRead(dustPin);
	sensorArray[sensorArrayIndex++] = dustVal;
	if (sensorArrayIndex == ArrayLenth)
		sensorArrayIndex = 0;
// analog read to voltage -0 ~ 3.3 to 4096
	dust_voltage = avergearray(sensorArray, ArrayLenth) * 3.3 / 4096.0;
	dustDensity = 0.17 * dust_voltage - 0.058;
	gpio_write(ledPin, HIGH);
// dust sensor value end
	return dustDensity;
}

int tcp_server_thread(void) {
	struct sockaddr_in servaddr;
	struct sockaddr_in cliaddr;
	int listenfd = -1;
	int connfd = -1;
	socklen_t clilen;
	int ret = 0;
	int recv_len = 0;
	int nbytes = 0;
	uint32_t sbuf_size = 0;
	char msg[BUF_SIZE];
	listenfd = socket(PF_INET, SOCK_STREAM, 0); //creates a socket
	if (listenfd < 0) {
		printf("\n[TCPSERV] TCP socket failure %d \n", errno);
		exit(1);
	}

	int reuse = 1;
	ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char *) &reuse,
			sizeof(reuse));
// set the socket options
	if (ret < 0) {
		printf("\n[TCPSERV] ERR: setting SO_REUSEADDR \n");
		goto errout_with_socket;
	}
	printf("\n[TCPSERV] set reusable success \n");
	/* Connect the socket to server */
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = PF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = HTONS(g_app_target_port);
	ret = bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr)); //address allocation
	if (ret < 0) {
		perror("\n[TCPSERV] bind fail \n");
		goto errout_with_socket;
	}
	printf("\n[TCPSERV] Listening... port %d \n", g_app_target_port);
	ret = listen(listenfd, 15); //listen for socket connection
	if (ret < 0) {
		perror("\n[TCPSERV] listen fail \n");
		goto errout_with_socket;
	}

	float dust_data = 0;
// set pin
	gpio_write(ledPin, HIGH);
	gpio_write(relayPin, LOW); // relayPin -LOW 1,2 connect
// HIGH 2,3 while (1) {
	clilen = sizeof(cliaddr);
	connfd = accept(listenfd, (struct sockaddr *) &cliaddr, &clilen);
//socket requests a connection
	if (connfd < 0) {
		perror("\n[TCPSERV] accept fail \n");
		goto errout_with_socket;
	}
	printf("\n[TCPSERV] Accepted \n");
	recv_len = sizeof(msg);

	while (1) {
		dust_data = dust() * 1000; // ㎍/㎥로 단위변환
		char *string = NULL;
		cJSON *value = NULL;
		cJSON *DUST = cJSON_CreateObject();
		if (DUST == NULL) {
			goto end;
		}
		value = cJSON_CreateNumber(dust_data);
		if (value == NULL) {
			goto end;
		}
		cJSON_AddItemToObject(DUST, "value", value);
		string=cJSON_Print(DUST);
		if (string == NULL) {
			fprintf(stderr, "Failed to print root. \n");
		}
		sbuf_size = strlen(string);
		send(connfd, string, sbuf_size, 0); //send a message on socket
		printf("Sensor Value : %s \n", string);
		end: cJSON_Delete(DUST);

		up_mdelay(1500);
		nbytes =	recv(connfd, msg,recv_len,MSG_DONTWAIT);

		if ((nbytes == 1 | nbytes == 2) && msg[0] == 'q') {
			/* connection closed */
			printf("\n[TCPSERV] selectserver : socket hung up err \n");
			break;
		} else if (nbytes > 0 && (strstr(msg, "fan_state ")) != NULL) {
			printf("fanstate : %s \n", msg);
			cJSON *root = cJSON_Parse(msg); //parse a msg (fan_state )
			relay_btn = cJSON_GetObjectItem(root, "fan_state ")->valueint;
			cJSON_Delete(root);
		} else if (nbytes > 0 && (strstr(msg, "mode_auto ")) != NULL) {
			printf("mode_auto : %s \n", msg);
			cJSON *root2 = cJSON_Parse(msg); //parse a msg (mode_auto )
			mode_auto = cJSON_GetObjectItem(root2, "mode_auto ")->valueint;
			cJSON_Delete(root2);
		}

		// 받은 데이터에 따른 mode,mode,mode,mode,mode,fan 동작
		if (mode_auto) { // mode auto
			relay_btn = 1;
			if (fan_check) {
				fan_check++;
				if (fan_check > FanCount)
					fan_check = 0;
			} else if (dustDensity > 0.06) {
				gpio_write(relayPin, LOW);
				fan_check++;
			} else {
				gpio_write(relayPin, HIGH);
			}
		} else { // mode manual
			if (relay_btn) {
				gpio_write(relayPin, LOW);
			} else {
				gpio_write(relayPin, HIGH);
			}
		}
		printf("\t mode -%d, relay_btn -%d, fan_chk -%d \n", mode_auto,
				relay_btn, fan_check);
	}

	if (connfd > 0) {
		close(connfd);
		printf("\n[TCPSERV] Closed connfd successfully \n");
	}
	printf("====== Waiting new client ====== \n");

	close( listenfd);
	printf ("\n[TCPSERV] Closed listenfd successfully \n" );
	return ;

	errout_with_socket :
	close (listenfd );
	exit (1 );
}


int main(int argc, char *argv[]) {
	StartSoftAP(true); //AP mode Start
	tcp_server_thread();
	return 0;
}



double avergearray(float* arr, int number) { //먼지 센서의 평균값 계산
	int i;
	int max, min;
	double avg;
	long amount = 0;
	if (number <= 0) {
		return 0;
	}
	if (arr[0] < arr[1]) {
		min = arr[0];
		max = arr[1];
	} else {
		min = arr[1];
		max = arr[0];
	}
	for (i = 2; i < number; i++) {
		if (arr[i] < min) {
			amount += min; //arr<min
			min = arr[i];
		} else {
			if (arr[i] > max) {
				amount += max; //arr>max
				max = arr[i];
			} else {
				amount += arr[i]; //min<=arr<=max
			}
		}
	}
	avg = (double) amount / (number - 2);
	return avg;
}
