/*
 * $QNXLicenseC:
 * Copyright 2010, QNX Software Systems.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You
 * may not reproduce, modify or distribute this software except in
 * compliance with the License. You may obtain a copy of the License
 * at: http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OF ANY KIND, either express or implied.
 *
 * This file may contain contributions from others, either as
 * contributors under the License or as licensors under other terms.
 * Please review this entire file for other proprietary rights or license
 * notices, as well as the QNX Development Suite License Guide at
 * http://licensing.qnx.com/license-guide/ for other information.
 * $
 */

/*
 * Synaptics Touchscreen driver
 *
 * Copyright (C) 2012-2018 Synaptics Incorporated. All rights reserved.
 *
 * The hardware access library is included from QNX software, and the
 * mtouch framework is also included from install/usr/input/. Both of
 * libraries are under Apache-2.0.
 *
 * Synaptics Touchscreen driver is interacted with QNX software system
 * as a touchscreen controller, for its licensing rule and related notices,
 * please scroll down the text to browse all of it.
 *
 * INFORMATION CONTAINED IN THIS DOCUMENT IS PROVIDED "AS-IS," AND SYNAPTICS
 * EXPRESSLY DISCLAIMS ALL EXPRESS AND IMPLIED WARRANTIES, INCLUDING ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE,
 * AND ANY WARRANTIES OF NON-INFRINGEMENT OF ANY INTELLECTUAL PROPERTY RIGHTS.
 * IN NO EVENT SHALL SYNAPTICS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, PUNITIVE, OR CONSEQUENTIAL DAMAGES ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OF THE INFORMATION CONTAINED IN THIS DOCUMENT, HOWEVER CAUSED
 * AND BASED ON ANY THEORY OF LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, AND EVEN IF SYNAPTICS WAS ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE. IF A TRIBUNAL OF COMPETENT JURISDICTION DOES
 * NOT PERMIT THE DISCLAIMER OF DIRECT DAMAGES OR ANY OTHER DAMAGES, SYNAPTICS'
 * TOTAL CUMULATIVE LIABILITY TO ANY PARTY SHALL NOT EXCEED ONE HUNDRED U.S.
 * DOLLARS.
 *
 */


#ifndef _SYNAPTICS_MTOUCH_H_
#define _SYNAPTICS_MTOUCH_H_


#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
#include <malloc.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/dcmd_input.h>
#include <sys/neutrino.h>
#include <sys/time.h>

#include <hw/i2c.h>
#include <hw/inout.h>

#include "input/mtouch_log.h"
#include "input/event_types.h"
#include "input/mtouch_driver.h"
#include "input/parseopts.h"

#include "synaptics_rmi4_core.h"

/* touch screen interrupt, connected to GPIO_39 */
#define GPIO_ATTN				(39)
#define TOUCH_INT				(1039)  /* 1000 + GPIO_NUM */

/* thread priority */
#define THREAD_PRIORITY			(21)

/* the pulse code sent to ISR */
#define PULSE_CODE        1

/* string shown for the mtouch_log */
#define MTOUCH_DEV				"mtouch-synaptics"

/* I2C communication data buffer */
#define I2C_XFER_LIMIT			(1024)

/* driver status */
#define FLAG_UNKNOWN      0x0000  // default
#define FLAG_INIT         0x1000  // Driver state initialized
#define FLAG_RESET        0x2000  // Driver state reset


/*
 * QNX dependent information
 *
 * private_data_t is composed of several system-level structure
 * including isr handling, gpio access, and i2c control.
 *
 */
typedef struct _private_data
{
	// IRQ related stuff
	// irq: the interrupt vector number
	// irq_iid: interrupt function ID
	int             	 irq;
	int             	 irq_iid;

	// Thread related stuff
	// thread_chid: channel ID
	// thread_coid: connection ID
	// thread_attr: the thread attributes
	// thread_param: the scheduling parameters
	// thread_event: an event
	// thread_mutex: mutex for the interrupt handling
	int             	 thread_chid;
	int             	 thread_coid;
	pthread_attr_t     	 thread_attr;
	struct sched_param 	 thread_param;
	struct sigevent    	 thread_event;
	pthread_mutex_t    	 thread_mutex;

	// GPIO related stuff
	// gpio_attn: the gpio pin defined as the attn
	unsigned       		 gpio_attn;

	// I2C related stuff
	// i2c: the full path of i2c device
	// i2c_fd: the function descriptor for i2c device
	// i2c_speed: i2c speed
	// i2c_slave: slave address
	char				*i2c;
	int					 i2c_fd;
	unsigned int		 i2c_speed;
	i2c_addr_t			 i2c_slave;

	// firmware upgrade related stuff
	// fw_update_startup: flag to perform fw update during the startup
	// fw_image: the path of target fw image file
	// fw_image_id: the target firmware id
	unsigned int		 fw_update_startup;
	char				*fw_image_path;
	unsigned int		 fw_image_id;

} private_data_t;

/*
 * a full size touched data report
 * this assumes 10 touched points are necessary
 */
struct touch_position_t {
	unsigned short x;
	unsigned short y;
	unsigned short wx;
	unsigned short wy;
};

struct touch_report_t {
	unsigned char is_touched;
	struct touch_position_t touch_points;
};


/*
 * mtouch device instance data
 *
 * syna_dev_t is the structure which contains information specific to
 * mtouch touch driver and Synaptics RMI4 data
 *
 */
typedef struct {
	// handler to connect to the Input Events library
	struct mtouch_device 	 	*inputevents_hdl;

	// structure contains QNX dependent information
	private_data_t 				*pvt_data;

	// structure contains Synaptics RMI4 data
	struct synaptics_rmi4_data 	*rmi4_data;

	// device driver status
	int							 flag;

	// thread for interrupt handling
	pthread_t 				  	 isr_thread;

	// structure for touch report
	struct touch_report_t 		*touch_report;
	int 						 touch_count;

} syna_dev_t;


/*
 * helper function to perform the i2c read/write operation
 * on the QNX system
 */
int mtouch_i2c_read(syna_dev_t *dev, uint8_t addr, uint16_t len, uint8_t *data);
int mtouch_i2c_write(syna_dev_t *dev, uint8_t addr, uint16_t len, uint8_t *data);


/* to check null pointer  */
#define _CHECK_POINTER(_in_ptr) \
		if (!_in_ptr) { mtouch_error(MTOUCH_DEV, "%s: invalid pointer", __FUNCTION__); return -EINVAL;}


#endif /* _SYNAPTICS_MTOUCH_H_ */

