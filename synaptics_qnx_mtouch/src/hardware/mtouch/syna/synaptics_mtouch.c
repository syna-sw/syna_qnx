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

#include "synaptics_mtouch.h"
#include "synaptics_rmi4_fw_update.h"

extern int synaptics_rmi4_init(syna_dev_t *p_dev);
extern int synaptics_rmi4_deinit(syna_dev_t *p_dev);
extern int synaptics_rmi4_sersor_report(struct synaptics_rmi4_data *rmi4_data);
extern int synaptics_rmi4_fwu_updater(const char *path_fw_image, const unsigned int image_fw_id);


/*
 * perform an i2c read operation
 * the slave address is using the 7-bit format and defined in the pvt_data->i2c_slave.addr
 *
 * syna_dev_t *dev   : mtouch device instance data
 * uint8_t addr      : register address, 8-bit
 * uint16_t len      : length of received data
 * uint8_t *data     : data buffer to store received data
 *
 * return len: success
 * return <0 : operation error
 */
int mtouch_i2c_read(syna_dev_t *dev, uint8_t addr, uint16_t len, uint8_t *data)
{
	int ret;
	struct {
		i2c_sendrecv_t hdr;
		uint8_t buf[I2C_XFER_LIMIT];
	} i2c_rd_data;

	_CHECK_POINTER(dev);

	if ((len > I2C_XFER_LIMIT) || (len < 0)) {
		mtouch_error(MTOUCH_DEV, "%s: invalid input length (input %d; limit %d)",
					__FUNCTION__, len, I2C_XFER_LIMIT);
		return -EINVAL;
	}
	if (dev->pvt_data->i2c_fd == -1) {
		mtouch_error(MTOUCH_DEV, "%s: failure to find %s descriptor",
					__FUNCTION__, dev->pvt_data->i2c);
		return -EINVAL;
	}

	i2c_rd_data.hdr.slave.addr = dev->pvt_data->i2c_slave.addr;
	i2c_rd_data.hdr.slave.fmt = I2C_ADDRFMT_7BIT;
	i2c_rd_data.hdr.send_len = 1;
	i2c_rd_data.hdr.recv_len = len;
	i2c_rd_data.hdr.stop = 1;
	i2c_rd_data.buf[0] = addr & 0xff;

	ret = devctl(dev->pvt_data->i2c_fd,
				DCMD_I2C_SENDRECV,
				&i2c_rd_data,
				sizeof(i2c_rd_data),
				NULL);
	if (EOK != ret) {
		mtouch_error(MTOUCH_DEV, "%s: failure in DCMD_I2C_SENDRECV %s (error: %s)",
					__FUNCTION__, dev->pvt_data->i2c, strerror (errno));
		return -EIO;
	}

	memcpy(&data[0], i2c_rd_data.buf, len);

	return len;
}


/*
 * perform an i2c write operation
 * the slave address is using the 7-bit format and defined in the pvt_data->i2c_slave.addr
 *
 * syna_dev_t *dev   : mtouch device instance data
 * uint8_t addr      : register address, 8-bit
 * uint16_t len      : length of written data
 * uint8_t *data     : written data
 *
 * return len: success
 * return <0 : operation error
 */
int mtouch_i2c_write(syna_dev_t *dev, uint8_t addr, uint16_t len, uint8_t *data)
{
	int ret;
	struct{
		i2c_send_t hdr;
		uint8_t buf[I2C_XFER_LIMIT];
	} i2c_wr_data;

	_CHECK_POINTER(dev);

	if ((len > I2C_XFER_LIMIT) || (len < 0)) {
		mtouch_error(MTOUCH_DEV, "%s: invalid input length (input %d; limit %d)",
					__FUNCTION__, len, I2C_XFER_LIMIT);
		return -EINVAL;
	}
	if (dev->pvt_data->i2c_fd == -1) {
		mtouch_error(MTOUCH_DEV, "%s: failure to find %s descriptor",
					__FUNCTION__, dev->pvt_data->i2c);
		return -EINVAL;
	}

	i2c_wr_data.hdr.slave.addr = dev->pvt_data->i2c_slave.addr;
	i2c_wr_data.hdr.slave.fmt = I2C_ADDRFMT_7BIT;
	i2c_wr_data.hdr.len = len + 1;
	i2c_wr_data.hdr.stop = 1;
	i2c_wr_data.buf[0] = (uint8_t)(addr & 0xff);
	memcpy(&i2c_wr_data.buf[1], &data[0], len);

	ret = devctl(dev->pvt_data->i2c_fd,
				DCMD_I2C_SEND,
				&i2c_wr_data,
				sizeof(i2c_wr_data),
				NULL);
	if (EOK != ret) {
        mtouch_error(MTOUCH_DEV,  "%s: failure in DCMD_I2C_SEND %s (error: %s)",
        			__FUNCTION__, dev->pvt_data->i2c, strerror (errno));
        return -EIO;
    }

    return len;
}


/*
 * implement the interrupt handling routine
 * the routine is created by the pthtead_create() in mtouch_driver_init()
 *
 * this is a message-driven thread that block in a receive loop,  MsgReceivev()
 * trigger the channel by using SIGEV_PULSE
 *
 * void* args        : the mtouch device instance data, syna_dev_t
 *
 */
static void* mtouch_interrupt_handling_thread(void* args)
{
	syna_dev_t *dev = (syna_dev_t *)args;
	private_data_t *pvt_data = dev->pvt_data;

	// the input/output vector
	iov_t iov;
    int	rcvid;

	// the _pulse structure describes a pulse, a fixed-size,
	// nonblocking message that carries a small payload
	struct _pulse pulse;

	// structure that describes an event
	struct sigevent ev;

	int retval;

	// prepare for the simple messages passing
	// fill in the fields of an iov_t structure
	SETIOV (&iov, &pulse, sizeof(pulse));

	// enable IO capability
	// lock the process's memory and request I/O privileges
	// let the thread execute the in, ins, out, outs, cli, and sti I/O opcodes
	if (ThreadCtl(_NTO_TCTL_IO, 0) == -1) {
		mtouch_error(MTOUCH_DEV, "%s: failed to config ThreadCtl",
				__FUNCTION__);
		return ( 0 );
	}

	while (true) {

		// the MsgReceivev kernel calls wait for a message to arrive on the channel identified by chid,
		// and place the received data in the array of buffers, iov
		if ((rcvid = MsgReceivev (pvt_data->thread_chid, &iov, 1, NULL)) == -1) {
			if (errno == ESRCH) {  // if the channel indicated by chid doesn't exist.
				mtouch_error(MTOUCH_DEV, "%s: iov error", __FUNCTION__);
				pthread_exit (NULL);
			}
			continue;
		}

		if (PULSE_CODE == pulse.code) {
			pthread_mutex_lock (&pvt_data->thread_mutex);

			// invoke the specified function to handle it
			// and the return value stands for the type of interrupt
			retval = synaptics_rmi4_sersor_report(dev->rmi4_data);
			if (retval < 0) {
				mtouch_error(MTOUCH_DEV, "%s: failed to get RMI sensor report", __FUNCTION__);

				pthread_mutex_unlock (&pvt_data->thread_mutex);
				InterruptUnmask (pvt_data->irq, pvt_data->irq_iid);

				return ( 0 );
			}

			SIGEV_NONE_INIT(&ev);
			SIGEV_MAKE_OVERDRIVE(&ev);
			MsgDeliverEvent(0, &ev);

			switch(retval) {
			case INTERRUPT_STATUS_TOUCH:
				mtouch_info(MTOUCH_DEV, "%s: INTERRUPT_STATUS_TOUCH", __FUNCTION__);

				// process the data packet from the specified driver
				// the relevant information from the driver's data packet will be used to create an mtouch_event_t
				// so that the Input Events framework can continue to process it and pass it to Screen.
				mtouch_driver_process_packet(dev->inputevents_hdl,
											dev->touch_report,
											dev,
											MTOUCH_PARSER_FLAG_NONE);
				break;
			default:
				break;
			}

			pthread_mutex_unlock (&pvt_data->thread_mutex);

			// after the interrupt-handling thread has dealt with the event,
			// this must be called InterruptUnmask to re-enable the interrupt.
			InterruptUnmask (pvt_data->irq, pvt_data->irq_iid);
		}
		else {
			mtouch_error(MTOUCH_DEV, "%s: unknown pulse code %x", __FUNCTION__, pulse.code);
			if (rcvid) {
				MsgReplyv (rcvid, ENOTSUP, &iov, 1); // not supported
			}
		}
	}

	return ( 0 );
}


/*
 * to enable/disable the hardware interrupt
 */
static void mtouch_irq_enable(syna_dev_t *dev, bool enable)
{
	if (enable)
		InterruptUnmask (dev->pvt_data->irq, dev->pvt_data->irq_iid);
	else
		InterruptMask (dev->pvt_data->irq, dev->pvt_data->irq_iid);


	mtouch_info(MTOUCH_DEV, "%s: interrupt is %s ", __FUNCTION__, (enable==true)?"enabled":"disabled");
}


/*
 * control the gpio to power-on the device
 * issue a hardware reset
 */
static void mtouch_power_on()
{
	// do nothing
}

/*
 * control the gpio to power-off the device
 * reset pin should stay in low
 */
static void mtouch_power_off()
{
	// do nothing
}

/*
 * retrieves the touch status for the specified digit of a touch-related event
 * this function is called for each of the touchpoints.
 * the maximum number of touchpoints is max_touchpoints, as specified in mtouch_driver_params_t
 *
 * void *packet      : data packet that contains information on the touch-related event
 * uint8_t digit_idx : digit (finger) index that the Input Events library is requesting, 0 ~ (max_touchpoints-1)
 * 					   this function is called in (max_touchpoints-1) times
 * int *valid        : valid pointer to touch status (e.g., 1 = Down and 0 = Up) of the touch-related event
 * void* arg         : user information
 */
static int mtouch_is_contact_down(void *packet, uint8_t digit_idx, int *valid, void *arg)
{
	struct touch_report_t *touch_report = (struct touch_report_t *)packet;

	*valid = touch_report[digit_idx].is_touched;

	printf("touch_point_%d : %s\n", digit_idx, (*valid)? "touching (1)":"lifting (0)");

	return EOK;
}

/*
 * retrieves the contact ID for the specified digit of a touch-related event
 * this function is called for each of the touchpoints.
 * the maximum number of touchpoints is max_touchpoints, as specified in mtouch_driver_params_t
 *
 * void *packet      : data packet that contains information on the touch-related event
 * uint8_t digit_idx : digit (finger) index that the Input Events library is requesting, 0 ~ (max_touchpoints-1)
 * 					   this function is called in (max_touchpoints-1) times
 * uint32_t *contact_id: pointer to contact ID of the touch-related event
 * void* arg         : user information
 */
static int mtouch_get_contact_id(void *packet, uint8_t digit_idx, uint32_t *contact_id, void *arg)
{
	*contact_id = digit_idx;

	printf("touch_point_%d : id = %-2d\n", digit_idx, *contact_id);

	return EOK;
}

/*
 * retrieves the coordinates for the specified digit of a touch-related event
 * this function is called for each of the touchpoints.
 * the maximum number of touchpoints is max_touchpoints, as specified in mtouch_driver_params_t
 *
 * void *packet      : data packet that contains information on the touch-related event
 * uint8_t digit_idx : digit (finger) index that the Input Events library is requesting, 0 ~ (max_touchpoints-1)
 * 					   this function is called in (max_touchpoints-1) times
 * int32_t *x        : x coordinate of the touch-related event for the specified digit_idx
 * int32_t *y        : y coordinate of the touch-related event for the specified digit_idx
 * void* arg         : user information
 */
static int mtouch_get_coords(void *packet, uint8_t digit_idx, int32_t *x, int32_t *y, void *arg)
{
	struct touch_report_t *touch_report = (struct touch_report_t *)packet;

	*x = touch_report[digit_idx].touch_points.x;
	*y = touch_report[digit_idx].touch_points.y;

	printf("touch_point_%d : (x , y) = (%-4d, %-4d)\n",digit_idx, *x, *y);

	return EOK;
}

/*
 * attach the driver to the Input Event framework, libinputevents
 *
 * assign the callback functions appropriately by using the mtouch_driver_funcs_t,
 * specify the capabilities that are supported in mtouch_driver_params_t
 *
 * syna_dev_t *dev   : mtouch device instance data
 */
static int mtouch_attach_dev(syna_dev_t *p_dev)
{
	_CHECK_POINTER(p_dev);
	_CHECK_POINTER(p_dev->rmi4_data);

	// assign the driver's callback functions
	mtouch_driver_funcs_t funcs = {
		.get_contact_id = mtouch_get_contact_id,
		.is_contact_down = mtouch_is_contact_down,
		.get_coords = mtouch_get_coords,
		.get_down_count = NULL,
		.get_touch_width = NULL,
		.get_touch_height = NULL,
		.get_touch_orientation = NULL,
		.get_touch_pressure = NULL,
		.get_seq_id = NULL,
		.set_event_rate = NULL,
		.get_contact_type = NULL,
		.get_select = NULL
	};

	// specify the capabilities
	mtouch_driver_params_t params = {
		.capabilities = MTOUCH_CAPABILITIES_CONTACT_ID |
						MTOUCH_CAPABILITIES_COORDS,
		.flags = 0,
		.max_touchpoints = p_dev->rmi4_data->num_of_fingers,
		.width = p_dev->rmi4_data->sensor_max_x,
		.height = p_dev->rmi4_data->sensor_max_y
	};

	mtouch_info(MTOUCH_DEV, "%s: maximum touch points = %-2d",
				__FUNCTION__, params.max_touchpoints);
	mtouch_info(MTOUCH_DEV, "%s: sensor maximum X = %-4d, maximum Y = %-4d",
				__FUNCTION__, params.width, params.height);

	// connect the driver with the specified parameters and callback functions
	// to the Input Events framework. It must be called while initializing the touch screen driver.
	p_dev->inputevents_hdl = mtouch_driver_attach(&params, &funcs);

	if (NULL == p_dev->inputevents_hdl) {
		mtouch_error(MTOUCH_DEV, "%s: failed to connect to libinputeventsy",
					__FUNCTION__);
		return -EIO;
	}

	return EOK;
}

/*
 * parse the argument options from the graphics.conf.
 * this is a callback function called by the input_parseopts()
 *
 * The format of options is: option=value1,option2=value2
 *
 * const char *option: passed from mtouch section of graphics.conf
 * const char *value : value for each arguments
 * void *arg         : user information
 */
static int mtouch_options(const char *option, const char *value, void *arg)
{
	syna_dev_t *dev = (syna_dev_t *)arg;

	if (0 == strcmp("i2c_devname", option)) {
		return input_parse_string(option, value, &dev->pvt_data->i2c);
	}
	else if (0 == strcmp("i2c_slave", option)) {
		return input_parse_unsigned(option, value, &dev->pvt_data->i2c_slave.addr);
	}
	else if (0 == strcmp("i2c_speed", option)) {
		return input_parse_unsigned(option, value, &dev->pvt_data->i2c_speed);
	}
	else if (0 == strcmp("fw_update_startup", option)) {
		return input_parse_unsigned(option, value, &dev->pvt_data->fw_update_startup);
	}
	else if (0 == strcmp("fw_img", option)) {
		return input_parse_string(option, value, &dev->pvt_data->fw_image_path);
	}
	else if (0 == strcmp("fw_img_id", option)) {
		return input_parse_unsigned(option, value, &dev->pvt_data->fw_image_id);
	}

	return EOK;
}

/*
 * initialization callback function
 *
 * screen calls dlopen() on this specified driver.
 * upon a successful dlopen(), Screen will use dlsym() to look for mtouch_driver_init()
 * for driver initialization.
 * performs the followings:
 *    - set the default values of the driver
 *    - parse any options specified in graphics.conf
 *    - hardware initialization
 *    - connect to input events library
 *    - create a separate thread to communicate directly with the hardware
 *      and trigger the input events library API function mtouch_driver_process_packet()
 *      to start processing the touch-related event data.
 */
void *mtouch_driver_init(const char *options)
{
	int retval;
	syna_dev_t *p_dev;
	private_data_t *pvt_data;

	// create a syna_dev_t instance
	p_dev = calloc(1, sizeof(syna_dev_t));
	if (!p_dev) {
		mtouch_error(MTOUCH_DEV, "%s: failed to allocate syna_dev_t device ",
					__FUNCTION__);
		return NULL;
	}

	// initialize defaults
	p_dev->flag = FLAG_UNKNOWN;
	p_dev->inputevents_hdl = NULL;
	p_dev->pvt_data = NULL;
	p_dev->rmi4_data = NULL;
	p_dev->isr_thread = NULL;

	// create private_data_t
	p_dev->pvt_data = calloc(1, sizeof(private_data_t));
	if (!p_dev->pvt_data) {
		mtouch_error(MTOUCH_DEV, "%s: failed to create the private_data_t",
					__FUNCTION__);
		goto exit;
	}
	pvt_data = p_dev->pvt_data;

	// enable IO capability
	// lock the process's memory and request I/O privileges
	// let the thread execute the in, ins, out, outs, cli, and sti I/O opcodes
	if (ThreadCtl(_NTO_TCTL_IO, 0) == -1) {
		mtouch_error(MTOUCH_DEV, "%s: failed to config ThreadCtl",
					__FUNCTION__);
		goto exit;
	}

	// create string buffers
	pvt_data->i2c = calloc(20, sizeof(char));
	if (!pvt_data->i2c) {
		mtouch_error(MTOUCH_DEV, "%s: failed to create the i2c string buffer",
					__FUNCTION__);
		goto exit;
	}
	pvt_data->fw_image_path = calloc(256, sizeof(char));
	if (!pvt_data->fw_image_path) {
		mtouch_error(MTOUCH_DEV, "%s: failed to create the fw_image string buffer",
					__FUNCTION__);
		goto exit;
	}

	// initialize defaults at private_data_t
	pvt_data->irq = TOUCH_INT;

	pvt_data->gpio_attn = GPIO_ATTN; // gpio 39

	pvt_data->thread_chid = -1;
	pvt_data->thread_coid = -1;
	pvt_data->thread_param.sched_priority = THREAD_PRIORITY;
	pvt_data->thread_event.sigev_priority = THREAD_PRIORITY;

	pvt_data->i2c_fd = -1;
	pvt_data->i2c_speed = 100000;  // default i2c speed, 100k
	pvt_data->i2c_slave.addr = 0x20;  // default slave address
	pvt_data->i2c_slave.fmt = I2C_ADDRFMT_7BIT;

	// parses settings specified in graphics.conf
	input_parseopts(options, mtouch_options, p_dev);

	// power-on device, hardware reset
	mtouch_power_on();

    // initialize I2C interface
	pvt_data->i2c_fd = open(pvt_data->i2c, O_RDWR);
	if (pvt_data->i2c_fd == -1) {
		mtouch_error(MTOUCH_DEV, "%s: failure in opening I2C device %s",
					__FUNCTION__, pvt_data->i2c);
		goto exit;
	}
	retval = devctl(pvt_data->i2c_fd,
				 	 DCMD_I2C_SET_BUS_SPEED,
				 	 &pvt_data->i2c_speed,
				 	 sizeof(pvt_data->i2c_speed),
				 	 NULL);
	if (EOK != retval) {
		mtouch_error(MTOUCH_DEV, "%s: failed to set i2c speed",
					__FUNCTION__);
		goto exit;
	}

	// perform the RMI4 device initialization
	retval = synaptics_rmi4_init(p_dev);
	if (EOK != retval) {
		mtouch_error(MTOUCH_DEV, "%s: failed to initialize synaptics rmi device",
					__FUNCTION__);
		goto exit;
	}

	p_dev->flag = FLAG_INIT;  // initialize the driver status

	// attach to input events framework, libinputevents
	// to configure mtouch_driver_funcs_t and mtouch_driver_params_t as well
	retval = mtouch_attach_dev(p_dev);
	if (EOK !=retval) {
		mtouch_error(MTOUCH_DEV, "%s: failure in attaching the driver to input event library",
					__FUNCTION__);
		goto exit;
	}

    // setup the interrupt handler thread
	// and a communication channel
	pvt_data->thread_chid = ChannelCreate (_NTO_CHF_DISCONNECT | _NTO_CHF_UNBLOCK);
	if (pvt_data->thread_chid == -1)    {
		mtouch_error(MTOUCH_DEV, "%s: failure ro create interrupt handler (error: %s)",
					__FUNCTION__, strerror (errno));
		goto exit;
	}
	pvt_data->thread_coid = ConnectAttach (0, 0, pvt_data->thread_chid, _NTO_SIDE_CHANNEL, 0);
	if (pvt_data->thread_coid == -1) {
		mtouch_error(MTOUCH_DEV, "%s: failure ro attach interrupt handler (error: %s)",
					__FUNCTION__, strerror (errno));
		goto exit;
	}

	// initializes the thread attributes in the default values
	// defaults: PTHREAD_CANCEL_DEFERRED, PTHREAD_CANCEL_ENABLE, PTHREAD_CREATE_JOINABLE
	// PTHREAD_INHERIT_SCHED, PTHREAD_SCOPE_SYSTEM
	pthread_attr_init(&pvt_data->thread_attr);
	// set the thread scheduling policy, round-robin scheduling.
	pthread_attr_setschedpolicy(&pvt_data->thread_attr, SCHED_RR);
	// set a thread's scheduling parameters
	pvt_data->thread_param.sched_priority = THREAD_PRIORITY;
	pthread_attr_setschedparam(&pvt_data->thread_attr, &pvt_data->thread_param);
	// set the thread's inherit-scheduling attribute
	// use the scheduling policy specified in pthread_attr_t for the thread.
	pthread_attr_setinheritsched(&pvt_data->thread_attr, PTHREAD_EXPLICIT_SCHED);
	// create the thread in a detached state.
	pthread_attr_setdetachstate(&pvt_data->thread_attr, PTHREAD_CREATE_DETACHED);
	// set the thread stack-size to 4K
	pthread_attr_setstacksize(&pvt_data->thread_attr, 4096);
	// describe the signal event, send a pulse
	// sigev_coid: the connection ID. this should be attached to the channel with
	//             which the pulse will be received.
	// sigev_code: a code to be interpreted by the pulse handler
	pvt_data->thread_event.sigev_notify = SIGEV_PULSE;
	pvt_data->thread_event.sigev_coid = pvt_data->thread_coid;
	pvt_data->thread_event.sigev_code = PULSE_CODE;

	// initialize a mutex for isr thread
    pthread_mutex_init (&pvt_data->thread_mutex, NULL);

    // create interrupt handler thread
	retval = pthread_create (&p_dev->isr_thread,
							&pvt_data->thread_attr,
							(void *)mtouch_interrupt_handling_thread,
							p_dev);
    if (EOK != retval) {
    	mtouch_error(MTOUCH_DEV, "%s: failure in pthread_create (error: %s)",
    				__FUNCTION__, strerror (errno));
        goto exit;
    }
    // name a thread
    // if a thread is setting its own name, uses ThreadCtl()
	pthread_setname_np(p_dev->isr_thread, "mtouch-synaptics-isr");

    // attach the given event to an interrupt source
	// assigining the interrupt vector number and the pointer of sigvent structure
	// that want to be delivered when this interrupt occurs
	//
	// before calling InterruptAttachEvent, it must request I/O privileges by calling ThreadCtl()
	pvt_data->irq_iid = InterruptAttachEvent (pvt_data->irq,
											&pvt_data->thread_event,
											_NTO_INTR_FLAGS_TRK_MSK);
	if (pvt_data->irq_iid == -1) {
		mtouch_error(MTOUCH_DEV, "%s: failure in attaching interrupt event (error: %s)",
    				__FUNCTION__, strerror (errno));
        goto exit;
    }

	mtouch_info(MTOUCH_DEV, "%s: finished", __FUNCTION__);


	// call firmware updating if it is necessary
	// the control switch is defined in option "fw_update_startup" in graphics.conf
	// 1: enabled; 0: disabled
	if (p_dev->pvt_data->fw_update_startup) {

		// disable the hardware to skip all interrupt event during the process
		mtouch_irq_enable(p_dev, false);

		retval = synaptics_rmi4_fwu_updater(p_dev->pvt_data->fw_image_path,
											p_dev->pvt_data->fw_image_id);
		if (retval < 0) {
	    	mtouch_error(MTOUCH_DEV, "%s: fail to do fw update",
	    				__FUNCTION__);
	    	goto exit;
	    }

		// enable the hardware interrupt
		mtouch_irq_enable(p_dev, true);

		// re-connect to the input event framework
		// to update the maximum finger supported
		mtouch_driver_detach(p_dev->inputevents_hdl);
		p_dev->inputevents_hdl = NULL;
		retval = mtouch_attach_dev(p_dev);
		if (retval < 0) {
			mtouch_error(MTOUCH_DEV, "%s: fail to re-connect to the input event framework", __FUNCTION__);
	    	goto exit;
		}

	}


	return p_dev;

exit:
	// break the connection
	if (p_dev->pvt_data->thread_coid != -1) {
		ConnectDetach(p_dev->pvt_data->thread_coid);
	}
	// destroy a communications channel
	if (p_dev->pvt_data->thread_chid != -1) {
		ChannelDestroy(p_dev->pvt_data->thread_chid);
	}
	// detach from the input event framework
	if (p_dev->inputevents_hdl) {
		mtouch_driver_detach(p_dev->inputevents_hdl);
		p_dev->inputevents_hdl = NULL;
	}
	// release the RMI4 device
	if (p_dev->flag != FLAG_UNKNOWN) {
		synaptics_rmi4_deinit(p_dev);
		p_dev->rmi4_data = NULL;

		p_dev->flag = FLAG_UNKNOWN;
	}
	// close i2c interface
	if (p_dev->pvt_data->i2c_fd != -1) {
		close(p_dev->pvt_data->i2c_fd);
	}
	if (p_dev->pvt_data->i2c) {
		free(p_dev->pvt_data->i2c);
		p_dev->pvt_data->i2c = NULL;
	}
	// release path of image
	if (p_dev->pvt_data->fw_image_path) {
		free(p_dev->pvt_data->fw_image_path);
		p_dev->pvt_data->fw_image_path = NULL;
	}
	// release private data
	if (p_dev->pvt_data) {
		free(p_dev->pvt_data);
		p_dev->pvt_data = NULL;
	}
	// release mtouch device
	if (p_dev) {
		free(p_dev);
		p_dev = NULL;
	}

	return NULL;
}

/*
 * cleanup callback function
 *
 * mtouch_driver_fini() performs any necessary device cleanup
 * and disconnects from the Input Events library.
 */
void mtouch_driver_fini(void *dev)
{
	syna_dev_t *p_dev = (syna_dev_t *)dev;

    // power-off
	mtouch_power_off();

	// destroy the isr thread
    pthread_cancel(p_dev->isr_thread);
    pthread_join(p_dev->isr_thread, NULL);

	// release the RMI4 device
	synaptics_rmi4_deinit(p_dev);
	p_dev->rmi4_data = NULL;
	p_dev->flag = FLAG_UNKNOWN;

	// detach from the input event framework
	if (p_dev->inputevents_hdl) {
		mtouch_driver_detach(p_dev->inputevents_hdl);
		p_dev->inputevents_hdl = NULL;
	}
	// close i2c interface
	close(p_dev->pvt_data->i2c_fd);

	if (p_dev->pvt_data->i2c) {
		free(p_dev->pvt_data->i2c);
		p_dev->pvt_data->i2c = NULL;
	}
	// release path of fw image
	if (p_dev->pvt_data->fw_image_path) {
		free(p_dev->pvt_data->fw_image_path);
		p_dev->pvt_data->fw_image_path = NULL;
	}
	// release private data
	if (p_dev->pvt_data) {
		free(p_dev->pvt_data);
		p_dev->pvt_data = NULL;
	}
	// release mtouch device
	if (p_dev) {
		free(p_dev);
		p_dev = NULL;
	}
}
