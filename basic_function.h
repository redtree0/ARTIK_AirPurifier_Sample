#ifndef BASIC_FUNCTION_H_
#define BASIC_FUNCTION_H_
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
//==================================define=====================================
#define TRUE 1
#define FALSE 0
//1. ADC
#define S5J_ADC_MAX_CHANNELS6
#define MAX_PIN_NUM 3 //You Can use ADC pin 0 ~ 3, 4 ~ 5 pin connect XGPIO 23, XGPIO 22
//2. GPIO
#define HIGH 1
#define LOW 0


//===========================Global Variable==================================
//1. ADC
int adc_fd;
struct adc_msg_s *sample;
//2. GPIO
//3. PWM
struct pwm_info_s pwm_info[4];
int pwm_fd[6];
//===============================function=====================================
//1. ADC
int analogInit(void);
int analogRead(int port);
void analogFinish(void);
//2. GPIO
void gpio_write(int port, int value); //Available GPIO Pin Number : 39, 41, 52, 53, 54, 55
int gpio_read(int port);
//3. PWM
int pwmInit(int port, int frequency, int duty);
void pwmSet(int port, int duty);
void pwmSetpin(int pin, int *fd);
void pwmFinish(int port);
#endif /* BASIC_FUNCTION_H_ */


