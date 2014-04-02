/*
 * ctlmissile.c - Control for the Blue Rocket Launcher
 *
 * Copyright 2014 N8body 
 *
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; version 2.
 */
#include <sys/file.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libusb-1.0/libusb.h>
#include <errno.h>
//#include <usb.h>

int debug = 0;

#define LAUNCHER_POLL           0x40
#define LAUNCHER_FIRE           0x10
#define LAUNCHER_STOP           0x20
#define LAUNCHER_UP             0x02
#define LAUNCHER_DOWN           0x01
#define LAUNCHER_LEFT           0x04
#define LAUNCHER_RIGHT          0x08

static libusb_context *ctx;
 
static int do_init(void)
{
if ( NULL == ctx && libusb_init(&ctx) )
return 0;
return 1;
}
int translateCommand(char* cmd){
	int data;
	if (!strcmp(cmd, "up")) {
		data=LAUNCHER_UP;
	} else if (!strcmp(cmd, "down")) {
		data=LAUNCHER_DOWN;
	} else if (!strcmp(cmd, "left")) {
		data=LAUNCHER_LEFT;
	} else if (!strcmp(cmd, "right")) {
		data=LAUNCHER_RIGHT;
	} else if (!strcmp(cmd, "fire")) {
		data=LAUNCHER_FIRE;
	} else if (!strcmp(cmd, "stop")) {
		data=LAUNCHER_STOP;
	} else if (!strcmp(cmd, "poll")) {
		data=LAUNCHER_POLL;
	} else if (strcmp(cmd, "stop")) {
		fprintf(stderr, "Unknown command: %s", cmd);
		exit(EXIT_FAILURE);
	}
	return data;
}
 
int sendCMD(libusb_device_handle *handle,char cmd){
 
	char data[1];
	data[0] = cmd;
	int ret = libusb_control_transfer 	( 	
		handle,
		0x21,// RequestType //USB_DT_HID 0x21
		LIBUSB_REQUEST_SET_CONFIGURATION,// bRequest
		LIBUSB_RECIPIENT_ENDPOINT , // Value
		0, // wIndex
		data, // Data
		1, // Lenght
		5000  // Timeout
	);	
	return ret;
}
int readInterrupt(struct libusb_device_handle * handle){
unsigned char buffer[1];
int transfered = 0;
	int ret = libusb_interrupt_transfer(
		handle,
		0x81,
		buffer,
		1,
		&transfered,
		2000 
	); 	
	if(ret == LIBUSB_ERROR_PIPE){
		//return readInterrupt(handle);
		return 0;
	}
	if(ret!=0){
		fprintf(stderr, "Error with Interrupt: %s\n",libusb_error_name(ret));	
		exit(0);
	}
	
	//fprintf(stderr, "Read Interrupt Call: %d\n",ret);
	//fprintf(stderr, "Read Interrupt Call: %s\n",libusb_error_name(ret));	
	//fprintf(stderr, "Read Interrupt: %X\n",buffer[0]);
	return buffer[0];
//int ret = usb_interrupt_transfer(launcher, 0x81, buffer, sizeof(buffer), &len, 0);
}
int waituntil(struct libusb_device_handle * handle,int interrupt_value,int delay){
	struct timeval  tStart, tStop, tLen;
	gettimeofday (&tStart, NULL) ;
	int interrupt;
	do{
		sendCMD(handle,LAUNCHER_POLL);
		interrupt = readInterrupt(handle);		
		gettimeofday (&tStop, NULL);
		timersub(&tStop,&tStart,&tLen);		
		if(delay!=0 && (tLen.tv_usec  > delay * 1000) ){
			break;
		}
		if(interrupt & interrupt_value){
			break;
		}
		// 2 Sekunden:
		//usleep(2000000);
		usleep(20*1000); // 20MS = 20k uSec
		
	}while(1);
}
void makeCommand(libusb_device_handle *handle,char* cmd,int delay){
	fprintf(stderr, "Moving %s for %d Ms\n",cmd,delay);
	int command = translateCommand(cmd);
	sendCMD(handle,command);
	// Bei Feuern: Kein Zeitlimit.
	if(command == LAUNCHER_FIRE){
		delay=0;
	}
	// Nur bei Bewegungskommandos pollen:
	if(command!=LAUNCHER_STOP && command!=LAUNCHER_POLL){
		waituntil(handle,command,delay);
		sendCMD(handle,LAUNCHER_STOP);
	}
	
}

// Detach USB Device
static int detach_device(libusb_device *dev)
{
struct libusb_device_descriptor d;
struct libusb_config_descriptor *conf;
libusb_device_handle *handle;
unsigned int i;
 
if ( libusb_get_device_descriptor(dev, &d) )
return 1;
 

 
if ( libusb_get_active_config_descriptor(dev, &conf) )
	return 2;
 
if ( libusb_open(dev, &handle) )
	return 3;
 
for(i = 0; i < conf->bNumInterfaces; i++) {
	int ret = libusb_detach_kernel_driver(handle, i);
	if(ret==LIBUSB_ERROR_NOT_FOUND){
		return 10;
	}
	if (  ret ) {
		libusb_close(handle);
		libusb_free_config_descriptor(conf);
		return 4;
	}
	printf(" - detached interface %u\n", i);
}

 
//libusb_reset_device(handle);
 
libusb_close(handle);
libusb_free_config_descriptor(conf);
return 6;
}
/*
 * Command to control Dream Cheeky USB missile launcher
 */

int main(int argc, char *argv[])
{

// Pid Code,erlaubt nur eine Instanz
int pid_file = open("/var/run/ctlmissile.pid", O_CREAT | O_RDWR, 0666);
int rc = flock(pid_file, LOCK_EX | LOCK_NB);
if(rc) {
    if(EWOULDBLOCK == errno)
		fprintf(stderr, "Another Instance is running!");
        exit(0);
}

		libusb_device **devlist;
		libusb_device *dev;
		ssize_t numdev, i;
		int ret = 0;
		do_init();
		 
		numdev = libusb_get_device_list(ctx, &devlist);
		
		 
		for(ret = 1, i = 0; i < numdev; i++) {
			dev = devlist[i];
			struct libusb_device_descriptor d;
			libusb_get_device_descriptor(dev, &d);			
			int ret=0;
			if (d.idVendor == 0x0a81  &&
				d.idProduct == 0x0701) {
				//fprintf(stderr, "Found Device:\n");
				// Try to detach:
				ret = detach_device(dev);
				//fprintf(stderr, "%d:\n",ret);
				
				libusb_device_handle *handle;
				// get Handle:
				ret = libusb_open(dev, &handle);
				if(ret!=0){
					fprintf(stderr, "Couldnt get Handle: %s\n",libusb_error_name(ret));	
				}				
				// Claim Device:
				ret = libusb_claim_interface(handle,0);
				if(ret!=0){
					fprintf(stderr, "Error with Claiming: %s\n",libusb_error_name(ret));	
				}
				int delay;
				sscanf (argv[2],"%d",&delay);
				// Try Command
				makeCommand(handle,argv[1],delay);				
				// Try to reset
				//libusb_reset_device(handle);
				break;
			}
			
		}
	

	if (!dev) {
		fprintf(stderr, "Unable to find device.\n");
		exit(EXIT_FAILURE);
	}

	return 0;
}

