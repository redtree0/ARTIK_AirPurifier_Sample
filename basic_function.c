#ifndef BASIC_FUNCTION_H_
#define BASIC_FUNCTION_H_
#endif
#include <apps/shell/tash.h>
#include <tinyara/config.h>
#include <stdio.h>
#include <tinyara/gpio.h>
#include <tinyara/analog/adc.h>
#include <tinyara/analog/ioctl.h>
#include <tinyara/pwm.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h> //clock_gettime
//====================================define===================================
#define TRUE 1
#define FALSE 0
//1. ADC
#define S5J_ADC_MAX_CHANNELS 6

#define MAX_PIN_NUM 3 //You Can use ADC pin 0 ~ 3, 4 ~ 5 pin connect XGPIO 23, XGPIO 22
//2. GPIO
#define HIGH 1
#define LOW 0

//=================================Global Variable===============
//1. ADC
int adc_fd;
struct adc_msg_s *sample;
//2. GPIO
//3. PWM
struct pwm_info_s pwm_info[4];
int pwm_fd[6];
//=========================ADC function==========================
int analogInit(void) {
	sample = (struct adc_msg_s *) malloc(
			sizeof(struct adc_msg_s) * S5J_ADC_MAX_CHANNELS);
	adc_fd = open("/dev/adc0", O_RDONLY);
	if (adc_fd < 0) {
		return FALSE;
	}
	return TRUE;
}
void analogFinish(void) {
	close(adc_fd);
	free(sample);
}

int analogRead(int port) {
	int result = -1;
	if (port >= 0 && port <= MAX_PIN_NUM) {
//Check available ADC port
		int ret;
		size_t readsize;
		ssize_t nbytes;
		while (1) {
			ret = ioctl(adc_fd, ANIOC_TRIGGER, 0);
			if (ret < 0) {
				analogFinish();
				analogInit();
				return result;
			}
			readsize = S5J_ADC_MAX_CHANNELS * sizeof(struct adc_msg_s);
			nbytes = read(adc_fd, sample, readsize);
			if (nbytes < 0) {
				if (errno != EINTR) {
					analogFinish();
					analogInit();
					return result;
				}
			} else if (nbytes == 0) {
			} else {
				int nsamples = nbytes / sizeof(struct adc_msg_s);
				if (nsamples * sizeof(struct adc_msg_s) == nbytes) {
					int i;

					for (i = 0; i < nsamples; i++) {
						if (sample[i].am_channel == port) {
							result = sample[i].am_data;
							goto out;
						}
					}
				}
			}
		}
		out: up_mdelay(5);
	}
	return result;
}


void gpio_write(int port, int value){
	char str[4];
	static char devpath[16];
	snprintf(devpath, 16, "/dev/gpio%d", port);
	int fd = open(devpath, O_RDWR);
	ioctl(fd, GPIOIOC_SET_DIRECTION, GPIO_DIRECTION_OUT);
	write(fd, str, snprintf(str, 4, "%d", value != 0) + 1);
	close(fd);
}
