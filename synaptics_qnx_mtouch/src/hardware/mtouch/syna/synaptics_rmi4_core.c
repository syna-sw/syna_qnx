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

#define DLEAY_AFTER_RESET 200000 // 200 ms

extern int synaptics_rmi4_fwu_init(struct synaptics_rmi4_data *rmi4_data, const unsigned char fn_version);
extern void synaptics_rmi4_fwu_deinit(void);

static syna_dev_t *g_syna_dev;

/*
 * helper function to configure the page selected register
 * and update the rmi4_data->current_page to skip redundant i2c operation
 *
 * struct synaptics_rmi4_data *rmi4_data: RMI4 device instance data
 * unsigned short address : RMI address, 16-bit
 *
 * return EOK: success
 * return <0 : error
 */
static int synaptics_rmi4_set_page(struct synaptics_rmi4_data *rmi4_data, unsigned short address)
{
	int retval = -EIO;
	unsigned char retry;
	unsigned char page;

	_CHECK_POINTER(rmi4_data);
	_CHECK_POINTER(g_syna_dev);

	page = ((address >> 8) & MASK_8BIT);
	if (page != rmi4_data->current_page) {
		for (retry = 0; retry < SYNA_I2C_RETRY_TIMES; retry++) {
			if (mtouch_i2c_write(g_syna_dev, 0xFF, 1, &page) == 1) {
				rmi4_data->current_page = page;
				retval = EOK;
				break;
			}
			mtouch_warn(MTOUCH_DEV,  "%s: rmi retry %d",
        			__FUNCTION__, retry + 1);
			usleep(20000);
		}
	}
	else {
		retval = EOK;
	}

	return retval;
}
/*
 * perform the RMI read operation
 *
 * struct synaptics_rmi4_data *rmi4_data: RMI4 device instance data
 * unsigned short address : RMI address, 16-bit
 * unsigned char* rd_data : read data
 * int r_length           : number of bytes read
 *
 * return r_length: success
 * otherwise, operation error
 */
int synaptics_rmi4_reg_read(struct synaptics_rmi4_data *rmi4_data,
							unsigned short address, unsigned char* rd_data, int r_length)
{
	int retval;
	unsigned char retry;
	unsigned char reg;

	_CHECK_POINTER(rmi4_data);
	_CHECK_POINTER(g_syna_dev);

	pthread_mutex_lock(&rmi4_data->rmi4_io_ctrl_mutex);

	retval = synaptics_rmi4_set_page(rmi4_data, address);
	if (EOK != retval) {
		retval = -EIO;
		goto exit;
	}

	reg = (unsigned char) address & MASK_8BIT;
	for (retry = 0; retry < SYNA_I2C_RETRY_TIMES; retry++) {
		retval = mtouch_i2c_read(g_syna_dev, reg, r_length, rd_data);
		if (retval == r_length)
			break;

		mtouch_warn(MTOUCH_DEV,  "%s: rmi retry %d",
    			__FUNCTION__, retry + 1);
		usleep(20000);
	}

	if (retry == SYNA_I2C_RETRY_TIMES) {
		mtouch_error(MTOUCH_DEV,  "%s: rmi read over retry limit",
    			__FUNCTION__);
		retval = -EIO;
		goto exit;
	}

exit:
	pthread_mutex_unlock(&rmi4_data->rmi4_io_ctrl_mutex);

	return retval;
}

/*
 * perform the RMI write operation
 *
 * struct synaptics_rmi4_data *rmi4_data: RMI4 device instance data
 * unsigned short address : RMI address, 16-bit
 * unsigned char* wr_data : written data
 * int w_length           : number of bytes written
 *
 * return w_length: success
 * otherwise, operation error
 */
int synaptics_rmi4_reg_write(struct synaptics_rmi4_data *rmi4_data,
							unsigned short address, unsigned char* wr_data, int w_length)
{
	int retval;
	unsigned char retry;
	unsigned char reg;

	_CHECK_POINTER(rmi4_data);
	_CHECK_POINTER(g_syna_dev);

	pthread_mutex_lock(&rmi4_data->rmi4_io_ctrl_mutex);

	retval = synaptics_rmi4_set_page(rmi4_data, address);
	if (EOK != retval) {
		retval = -EIO;
		goto exit;
	}

	reg = (unsigned char) address & MASK_8BIT;
	for (retry = 0; retry < SYNA_I2C_RETRY_TIMES; retry++) {
		retval = mtouch_i2c_write(g_syna_dev, reg, w_length, wr_data);
		if (retval == w_length)
			break;

		mtouch_warn(MTOUCH_DEV,  "%s: rmi retry %d",
    			__FUNCTION__, retry + 1);
		usleep(20000);
	}

	if (retry == SYNA_I2C_RETRY_TIMES) {
		mtouch_error(MTOUCH_DEV,  "%s: rmi write over retry limit",
    			__FUNCTION__);
		retval = -EIO;
		goto exit;
	}

exit:
	pthread_mutex_unlock(&rmi4_data->rmi4_io_ctrl_mutex);

	return retval;
}

/*
 * to configure the settings of interrupt mask
 *
 * struct synaptics_rmi4_data *rmi4_data: RMI4 device instance data
 * bool enable: true= enable, false=disable
 *
 * return EOK: reset completion
 * otherwise, fail
 */
int synaptics_rmi4_int_enable(struct synaptics_rmi4_data *rmi4_data, bool enable)
{
	int retval = EOK;
	unsigned char data = 0x00;
	unsigned short intr_addr;

	_CHECK_POINTER(rmi4_data);
	_CHECK_POINTER(rmi4_data->f01);

	if (rmi4_data->num_of_intr_regs > 1)
		mtouch_warn(MTOUCH_DEV,  "%s: configure 1 interrupt source only", __FUNCTION__);

	intr_addr = rmi4_data->f01->base_addr.ctrl_base + 1;

	if (0x00 != rmi4_data->intr_mask) {

		if (enable) {
			retval = synaptics_rmi4_reg_write(rmi4_data,
						intr_addr,
						&(rmi4_data->intr_mask),
						sizeof(rmi4_data->intr_mask));
			if (retval < 0) {
				mtouch_error(MTOUCH_DEV,  "%s: fail to configure rmi reg 0x%x", __FUNCTION__, intr_addr);
				return retval;
			}
		}
		else {
			data = 0x00;
			retval = synaptics_rmi4_reg_write(rmi4_data,
						intr_addr,
						&data,
						sizeof(data));
			if (retval < 0) {
				mtouch_error(MTOUCH_DEV,  "%s: fail to set rmi reg 0x%x to zero", __FUNCTION__, intr_addr);
				return retval;
			}
		}
	}

	return retval;
}

/*
 * send a software reset
 *
 * struct synaptics_rmi4_data *rmi4_data: RMI4 device instance data
 *
 * return EOK: reset completion
 * otherwise, fail
 */
int synaptics_rmi4_sw_reset(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char cmd_reset = 0x01;

	_CHECK_POINTER(rmi4_data);
	_CHECK_POINTER(rmi4_data->f01);

	retval = synaptics_rmi4_reg_write(rmi4_data,
				rmi4_data->f01->base_addr.cmd_base,
				&cmd_reset,
				sizeof(char));
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: failed to reset device", __FUNCTION__);
		return -EIO;
	}

	usleep(DLEAY_AFTER_RESET);
	mtouch_info(MTOUCH_DEV, "%s: reset", __FUNCTION__);

	return EOK;
}

/*
 * collect the touch report from RMI F$11
 *
 * void *: should be the struct synaptics_rmi4_data *
 *
 * return EOK: finish
 * otherwise, fail
 */
static int synaptics_rmi4_f11_abs_report(void *input)
{
	struct synaptics_rmi4_data *rmi4_data = (struct synaptics_rmi4_data *)input;

	int retval;
	unsigned char touch_count = 0;  // number of touch points
	unsigned char reg_index;
	unsigned char finger;
	unsigned char fingers_supported;
	unsigned char num_of_finger_status_regs;
	unsigned char finger_shift;
	unsigned char finger_status;
	unsigned char finger_status_reg[3];
	unsigned short data_addr;
	unsigned short data_offset;
	int x;
	int y;
	int wx;
	int wy;
	struct synaptics_rmi4_f11_data_1_5 data;

	_CHECK_POINTER(g_syna_dev);

	// the number of finger status registers is determined by the
	// maximum number of fingers supported - 2 bits per finger. So
	// the number of finger status registers to read is:
	// register_count = ceil(max_num_of_fingers / 4)
	fingers_supported = rmi4_data->num_of_fingers;
	num_of_finger_status_regs = (fingers_supported + 3) / 4;
	data_addr = rmi4_data->f11->base_addr.data_base;

	retval = synaptics_rmi4_reg_read(rmi4_data,
				data_addr,
				finger_status_reg,
				num_of_finger_status_regs);
	if (retval < 0) {
		mtouch_info(MTOUCH_DEV, "%s: fail to read f11 reg 0x%x",
					__FUNCTION__, data_addr);
		return -EIO;
	}

	pthread_mutex_lock(&rmi4_data->rmi4_report_mutex);

	for (finger = 0; finger < fingers_supported; finger++) {
		reg_index = finger / 4;
		finger_shift = (finger % 4) * 2;
		finger_status = (finger_status_reg[reg_index] >> finger_shift) & MASK_2BIT;

		// Each 2-bit finger status field represents the following:
		// 00 = finger not present
		// 01 = finger present and data accurate
		// 10 = finger present but data may be inaccurate
		// 11 = reserved
		if (finger_status) {
			data_offset = data_addr + num_of_finger_status_regs + (finger * sizeof(data.data));

			retval = synaptics_rmi4_reg_read(rmi4_data, data_offset, data.data, sizeof(data.data));
			if (retval < 0) {
				touch_count = 0;
				goto exit;
			}

			x = (data.x_position_11_4 << 4) | data.x_position_3_0;
			y = (data.y_position_11_4 << 4) | data.y_position_3_0;
			wx = data.wx;
			wy = data.wy;

			// filling out the touched report
			g_syna_dev->touch_report[finger].is_touched = FINGER_LANDING;
			g_syna_dev->touch_report[finger].touch_points.x = x;
			g_syna_dev->touch_report[finger].touch_points.y = y;
			g_syna_dev->touch_report[finger].touch_points.wx = wx;
			g_syna_dev->touch_report[finger].touch_points.wy = wy;

			mtouch_info(MTOUCH_DEV, "%s: (finger %d) status = 0x%02x, x = %d, y = %d, wx = %d, wy = %d",
					__FUNCTION__, finger, finger_status, x, y, wx, wy);

			touch_count++;
		}
	}

	if (0 == touch_count) {
		mtouch_info(MTOUCH_DEV, "%s: fingers leave", __FUNCTION__);

		// set all supported fingers as FINGER_LEAVE
		for (finger = 0; finger < fingers_supported; finger++) {
			if (FINGER_LANDING == g_syna_dev->touch_report[finger].is_touched)
				g_syna_dev->touch_report[finger].is_touched = FINGER_LIFTING;
		}
	}

exit:
	pthread_mutex_unlock(&rmi4_data->rmi4_report_mutex);

	return touch_count;
}

/*
 * collect the touch report from RMI F$12
 *
 * void *: should be the struct synaptics_rmi4_data *
 *
 * return EOK: finish
 * otherwise, fail
 */
static int synaptics_rmi4_f12_abs_report(void *input)
{
	struct synaptics_rmi4_data *rmi4_data = (struct synaptics_rmi4_data *)input;

	int retval;
	unsigned char touch_count = 0;  // number of touch points
	unsigned char index;
	unsigned char finger;
	unsigned char fingers_to_process;
	unsigned char finger_status;
	unsigned char size_of_2d_data;
	unsigned short data_addr;
	int x;
	int y;
	int wx;
	int wy;
	struct synaptics_rmi4_f12_extra_data *extra_data;
	struct synaptics_rmi4_f12_finger_data *data;
	struct synaptics_rmi4_f12_finger_data *finger_data;
	static unsigned char objects_already_present;

	_CHECK_POINTER(g_syna_dev);
	_CHECK_POINTER(rmi4_data->f12->data);

	fingers_to_process = rmi4_data->num_of_fingers;
	data_addr = rmi4_data->f12->base_addr.data_base;
	extra_data = (struct synaptics_rmi4_f12_extra_data *)rmi4_data->f12->extra;
	size_of_2d_data = sizeof(struct synaptics_rmi4_f12_finger_data);

	// determine the total number of fingers to process
	if (extra_data->data15_size) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				data_addr + extra_data->data15_offset,
				extra_data->data15_data,
				extra_data->data15_size);
		if (retval < 0) {
			mtouch_info(MTOUCH_DEV, "%s: fail to read f12 reg 0x%x",
						__FUNCTION__, data_addr + extra_data->data15_offset);
			return -EIO;
		}

		// start checking from the highest bit
		index = extra_data->data15_size - 1; // highest byte
		finger = (fingers_to_process - 1) % 8; // highest bit
		do {
			if (extra_data->data15_data[index] & (1 << finger))
				break;

			if (finger) {
				finger--;
			} else if (index > 0) {
				index--; // move to the next lower byte
				finger = 7;
			}

			fingers_to_process--;
		} while (fingers_to_process);

		mtouch_info(MTOUCH_DEV, "%s: number of fingers to process = %d",
				__FUNCTION__, fingers_to_process);
	}

	fingers_to_process = (fingers_to_process > objects_already_present)?
						fingers_to_process : objects_already_present;
	if (0 == fingers_to_process) {
		// set all supported fingers as FINGER_LEAVE
		for (finger = 0; finger < rmi4_data->num_of_fingers; finger++) {
			if (FINGER_LANDING == g_syna_dev->touch_report[finger].is_touched)
				g_syna_dev->touch_report[finger].is_touched = FINGER_LIFTING;
		}

		return 0;
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
				data_addr + extra_data->data1_offset,
				(unsigned char *)rmi4_data->f12->data,
				fingers_to_process * size_of_2d_data);
	if (retval < 0) {
		mtouch_info(MTOUCH_DEV, "%s: fail to read f12 reg 0x%x",
					__FUNCTION__, data_addr + extra_data->data1_offset);
		return -EIO;
	}

	data = (struct synaptics_rmi4_f12_finger_data *)rmi4_data->f12->data;

	pthread_mutex_lock(&rmi4_data->rmi4_report_mutex);

	for (finger = 0; finger < fingers_to_process; finger++) {
		finger_data = data + finger;
		finger_status = finger_data->object_type_and_status;

		objects_already_present = finger + 1;

		x = (finger_data->x_msb << 8) | (finger_data->x_lsb);
		y = (finger_data->y_msb << 8) | (finger_data->y_lsb);
		wx = finger_data->wx;
		wy = finger_data->wy;

		switch (finger_status) {
		case F12_FINGER_STATUS:
		case F12_GLOVED_FINGER_STATUS:

			// filling out the touched report
			g_syna_dev->touch_report[finger].is_touched = FINGER_LANDING;
			g_syna_dev->touch_report[finger].touch_points.x = x;
			g_syna_dev->touch_report[finger].touch_points.y = y;
			g_syna_dev->touch_report[finger].touch_points.wx = wx;
			g_syna_dev->touch_report[finger].touch_points.wy = wy;

			mtouch_info(MTOUCH_DEV, "%s: (finger %d) status = 0x%02x, x = %d, y = %d, wx = %d, wy = %d",
					__FUNCTION__, finger, finger_status, x, y, wx, wy);

			touch_count++;
			break;

		case F12_PALM_STATUS:
			mtouch_info(MTOUCH_DEV, "%s: (palm %d detected ) x = %d, y = %d, wx = %d, wy = %d",
					__FUNCTION__, finger, x, y, wx, wy);
			break;

		default:
			break;
		}

	}

	if (0 == touch_count) {
		mtouch_info(MTOUCH_DEV, "%s: fingers leave", __FUNCTION__);

		objects_already_present = 0;

		// set all supported fingers as FINGER_LEAVE
		for (finger = 0; finger < rmi4_data->num_of_fingers; finger++) {
			if (FINGER_LANDING == g_syna_dev->touch_report[finger].is_touched)
				g_syna_dev->touch_report[finger].is_touched = FINGER_LIFTING;
		}
	}

	pthread_mutex_unlock(&rmi4_data->rmi4_report_mutex);

	return touch_count;
}

/*
 * prepare the sensor report, which is called by ISR
 * use interrupt status information to determine the source that are flagging the interrupt
 *
 * struct synaptics_rmi4_data *rmi4_data: RMI4 device instance data
 *
 * return EOK: finish
 * otherwise, fail
 */
int synaptics_rmi4_sersor_report(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char data[MAX_INTR_REGISTERS + 1];
	struct synaptics_rmi4_f01_device_status status;

	_CHECK_POINTER(g_syna_dev);
	_CHECK_POINTER(rmi4_data);

	// read interrupt status information
	retval = synaptics_rmi4_reg_read(rmi4_data,
				rmi4_data->f01->base_addr.data_base,
				data,
				rmi4_data->num_of_intr_regs + 1);
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: failed to read interrupt status", __FUNCTION__);
		return -EIO;
	}

	status.data[0] = data[0];
	if (status.unconfigured && !status.flash_prog) {
		mtouch_info(MTOUCH_DEV, "%s: spontaneous reset detected", __FUNCTION__);
	}

	// check-and-compare the interrupt mask
	// find the source of the interrupt accordingly
	if (data[1] & INTERRUPT_STATUS_FLASH) {
		_CHECK_POINTER(rmi4_data->f34);
		if ((data[1] & rmi4_data->f34->intr_mask) &&
			(rmi4_data->f34->attn_handle != NULL)) {
			rmi4_data->f34->attn_handle(rmi4_data);
		}

		retval = INTERRUPT_STATUS_FLASH;
	}
	if (data[1] & INTERRUPT_STATUS_DEVICE) {
		_CHECK_POINTER(rmi4_data->f01);
		if ((data[1] & rmi4_data->f01->intr_mask) &&
			(rmi4_data->f01->attn_handle != NULL)) {
			rmi4_data->f01->attn_handle(rmi4_data);
		}

		retval = INTERRUPT_STATUS_DEVICE;
	}
	if (data[1] & INTERRUPT_STATUS_TOUCH) {
		if (rmi4_data->f11) {
			if ((data[1] & rmi4_data->f11->intr_mask) &&
				(rmi4_data->f11->attn_handle != NULL)) {
				g_syna_dev->touch_count = rmi4_data->f11->attn_handle(rmi4_data);
			}
		}
		else if (rmi4_data->f12) {
			if ((data[1] & rmi4_data->f12->intr_mask) &&
				(rmi4_data->f12->attn_handle != NULL)) {
				g_syna_dev->touch_count = rmi4_data->f12->attn_handle(rmi4_data);
			}
		}

		retval = INTERRUPT_STATUS_TOUCH;
	}
	if (data[1] & INTERRUPT_STATUS_BUTTON) {
		_CHECK_POINTER(rmi4_data->f1a);
		if ((data[1] & rmi4_data->f1a->intr_mask) &&
			(rmi4_data->f1a->attn_handle != NULL)) {
			rmi4_data->f1a->attn_handle(rmi4_data);
		}

		retval = INTERRUPT_STATUS_BUTTON;
	}
	if (data[1] & INTERRUPT_STATUS_SENSOR) {
		_CHECK_POINTER(rmi4_data->f55);
		if ((data[1] & rmi4_data->f55->intr_mask) &&
			(rmi4_data->f55->attn_handle != NULL)) {
			rmi4_data->f55->attn_handle(rmi4_data);
		}

		retval = INTERRUPT_STATUS_SENSOR;
	}

	return retval;
}

/*
 * Function $11 initialization
 * f11 implements the data designed for two-dimensional touch position sensors
 *
 * struct synaptics_rmi4_data *rmi4_data: RMI4 device instance data
 *
 * return EOK: complete f11 initialization
 * otherwise, fail
 */
static int synaptics_rmi4_f11_init(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned short reg_addr;
	unsigned char offset;

	struct synaptics_rmi4_f11_query_0_5 query_0_5;
	struct synaptics_rmi4_f11_query_9 query_9;
	struct synaptics_rmi4_f11_query_12 query_12;
	struct synaptics_rmi4_f11_query_7_8 query_7_8;
	struct synaptics_rmi4_f11_ctrl_6_9 control_6_9;

	_CHECK_POINTER(rmi4_data);
	_CHECK_POINTER(rmi4_data->f11);

	/* query 0 - 5 */

	// maximum number of fingers supported
	reg_addr = rmi4_data->f11->base_addr.query_base;
	retval = synaptics_rmi4_reg_read(rmi4_data, reg_addr, query_0_5.data, sizeof(query_0_5.data));
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: failed to read register 0x%04x", __FUNCTION__, reg_addr);
		return -EIO;
	}

	if (query_0_5.num_of_fingers <= 4)
		rmi4_data->num_of_fingers = query_0_5.num_of_fingers + 1;
	else if (query_0_5.num_of_fingers == 5)
		rmi4_data->num_of_fingers = 10;

	// maximum x coordinate and y coordinate supported
	reg_addr = rmi4_data->f11->base_addr.ctrl_base + 6;
	retval = synaptics_rmi4_reg_read(rmi4_data, reg_addr, control_6_9.data, sizeof(control_6_9.data));
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: failed to read register 0x%04x", __FUNCTION__, reg_addr);
		return -EIO;
	}

	rmi4_data->sensor_max_x =
				(control_6_9.sensor_max_x_pos_7_0) |
				(control_6_9.sensor_max_x_pos_11_8 << 8);
	rmi4_data->sensor_max_y =
				(control_6_9.sensor_max_y_pos_7_0) |
				(control_6_9.sensor_max_y_pos_11_8 << 8);
	rmi4_data->max_touch_width = MAX_F11_TOUCH_WIDTH;

	offset = sizeof(query_0_5.data);


	/* query 6 */
	if (query_0_5.has_rel)
		offset += 1;

    /* queries 7 8 */
	if (query_0_5.has_gestures) {
		reg_addr = rmi4_data->f11->base_addr.query_base + offset;
		retval = synaptics_rmi4_reg_read(rmi4_data, reg_addr, query_7_8.data, sizeof(query_7_8.data));
		if (retval < 0) {
			mtouch_error(MTOUCH_DEV, "%s: failed to read register 0x%04x", __FUNCTION__, reg_addr);
			return -EIO;
		}

		offset += sizeof(query_7_8.data);
	}

	/* query 9 */
	if (query_0_5.has_query_9) {
		reg_addr = rmi4_data->f11->base_addr.query_base + offset;
		retval = synaptics_rmi4_reg_read(rmi4_data, reg_addr, query_9.data, sizeof(query_9.data));
		if (retval < 0) {
			mtouch_error(MTOUCH_DEV, "%s: failed to read register 0x%04x", __FUNCTION__, reg_addr);
			return -EIO;
		}

		offset += sizeof(query_9.data);
	}

	/* query 10 */
	if (query_0_5.has_gestures && query_7_8.has_touch_shapes)
		offset += 1;

	/* query 11 */
	if (query_0_5.has_query_11)
		offset += 1;

	/* query 12 */
	if (query_0_5.has_query_12) {
		reg_addr = rmi4_data->f11->base_addr.query_base + offset;
		retval = synaptics_rmi4_reg_read(rmi4_data, reg_addr, query_12.data, sizeof(query_12.data));
		if (retval < 0) {
			mtouch_error(MTOUCH_DEV, "%s: failed to read register 0x%04x", __FUNCTION__, reg_addr);
			return -EIO;
		}

		offset += sizeof(query_12.data);
	}

	/* query 13 */
	if (query_0_5.has_jitter_filter)
		offset += 1;

	/* query 14 */
	if (query_0_5.has_query_12 && query_12.has_general_information_2)
		offset += 1;

	/* queries 15 16 17 18 19 20 21 22 23 24 25 26*/
	if (query_0_5.has_query_12 && query_12.has_physical_properties)
		offset += 12;

	/* query 27 */
	if (query_0_5.has_query_27)
		offset += 1;

	return EOK;
}


/*
 * to look for the subpackets in Function $12
 *
 * struct synaptics_rmi4_data *rmi4_data: RMI4 device instance data
 * unsigned char *presence           : control presence
 * unsigned char presence_size       : size of control presence
 * unsigned char structure_offset    : offset
 * unsigned char reg                 :
 * unsigned char sub                 : subpacket
 *
 */
static int synaptics_rmi4_f12_find_sub(struct synaptics_rmi4_data *rmi4_data,
                            			unsigned char *presence,
                            			unsigned char presence_size,
                            			unsigned char structure_offset,
                            			unsigned char reg,
                            			unsigned char sub)
{
	int retval;
	unsigned char cnt;
	unsigned char regnum;
	unsigned char bitnum;
	unsigned char p_index;
	unsigned char s_index;
	unsigned char offset;
	unsigned char max_reg;
	unsigned char *structure;
	unsigned short reg_addr;

	_CHECK_POINTER(rmi4_data);
	_CHECK_POINTER(rmi4_data->f12);

	max_reg = (presence_size - 1) * 8 - 1;

	if (reg > max_reg) {
		mtouch_error(MTOUCH_DEV, "%s: over limit", __FUNCTION__);
		return -ENODEV;
	}

	p_index = reg / 8 + 1;
	bitnum = reg % 8;
	if ((presence[p_index] & (1 << bitnum)) == 0x00) {
		mtouch_error(MTOUCH_DEV, "%s: register %d is not present", __FUNCTION__, reg);
		return -ENODEV;
	}

	structure = (unsigned char *)malloc(presence[0]);
	if (!structure) {
		mtouch_error(MTOUCH_DEV, "%s: failed to allocate memory for structure register",
				__FUNCTION__);
		return -ENODEV;
	}

	reg_addr = rmi4_data->f12->base_addr.query_base + structure_offset;
	retval = synaptics_rmi4_reg_read(rmi4_data, reg_addr, structure, presence[0]);
    if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: failed to read register 0x%04x", __FUNCTION__, reg_addr);
		retval = -EIO;
        goto exit;
    }

	s_index = 0;

	for (regnum = 0; regnum < reg; regnum++) {
		p_index = regnum / 8 + 1;
		bitnum = regnum % 8;
		if ((presence[p_index] & (1 << bitnum)) == 0x00)
			continue;

		if (structure[s_index] == 0x00)
			s_index += 3;
		else
			s_index++;

		while (structure[s_index] & ~MASK_7BIT)
			s_index++;

		s_index++;
	}

	cnt = 0;
	s_index++;
	offset = sub / 7;
	bitnum = sub % 7;

	do {
		if (cnt == offset) {
			if (structure[s_index + cnt] & (1 << bitnum))
				retval = 1;
			else
				retval = 0;
			goto exit;
		}
		cnt++;
	} while (structure[s_index + cnt - 1] & ~MASK_7BIT);

    retval = 0;

exit:
	free(structure);

	return retval;
}

/*
 * Function $12 initialization
 * f12 implements the data designed for two-dimensional touch position sensors
 * no device will contain both Function $12 and Function $11. All F$12 registers are
 * packet registers, with all packet register contents organized into subpackets.
 *
 * struct synaptics_rmi4_data *rmi4_data: RMI4 device instance data
 *
 * return EOK: complete f12 initialization
 * otherwise, fail
 */
static int synaptics_rmi4_f12_init(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned short reg_addr;

	unsigned char subpacket;
	unsigned char ctrl_23_size;
	unsigned char size_of_2d_data;
	unsigned char size_of_query5;
	unsigned char size_of_query8;
	unsigned char ctrl_8_offset;
	unsigned char ctrl_20_offset;
	unsigned char ctrl_23_offset;
	unsigned char ctrl_28_offset;
	struct synaptics_rmi4_f12_extra_data *extra_data;
	struct synaptics_rmi4_f12_query_5 query_5;
	struct synaptics_rmi4_f12_query_8 query_8;
	struct synaptics_rmi4_f12_ctrl_8 ctrl_8;
	struct synaptics_rmi4_f12_ctrl_23 ctrl_23;

	unsigned char report_enable = RPT_DEFAULT ;

	_CHECK_POINTER(rmi4_data);
	_CHECK_POINTER(rmi4_data->f12);

	rmi4_data->f12->extra = calloc(1, sizeof(struct synaptics_rmi4_f12_extra_data));
	if (!rmi4_data->f12->extra) {
		mtouch_error(MTOUCH_DEV, "%s: failed to allocate memory for f12->extra", __FUNCTION__);
		retval = -ENOMEM;
        goto exit;
	}
	extra_data = (struct synaptics_rmi4_f12_extra_data *)rmi4_data->f12->extra;
	size_of_2d_data = sizeof(struct synaptics_rmi4_f12_finger_data);

	reg_addr = rmi4_data->f12->base_addr.query_base + 4;
	retval = synaptics_rmi4_reg_read(rmi4_data, reg_addr, &size_of_query5, sizeof(size_of_query5));
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: failed to read register 0x%04x", __FUNCTION__, reg_addr);
		retval = -EIO;
        goto exit;
	}

	if (size_of_query5 > sizeof(query_5.data))
		size_of_query5 = sizeof(query_5.data);
	memset(query_5.data, 0x00, sizeof(query_5.data));

	reg_addr = rmi4_data->f12->base_addr.query_base + 5;
	retval = synaptics_rmi4_reg_read(rmi4_data, reg_addr, query_5.data, size_of_query5);
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: failed to read register 0x%04x", __FUNCTION__, reg_addr);
		retval = -EIO;
        goto exit;
	}

	ctrl_8_offset = query_5.ctrl0_is_present +
			query_5.ctrl1_is_present +
			query_5.ctrl2_is_present +
			query_5.ctrl3_is_present +
			query_5.ctrl4_is_present +
			query_5.ctrl5_is_present +
			query_5.ctrl6_is_present +
			query_5.ctrl7_is_present;

	ctrl_20_offset = ctrl_8_offset +
			query_5.ctrl8_is_present +
			query_5.ctrl9_is_present +
			query_5.ctrl10_is_present +
			query_5.ctrl11_is_present +
			query_5.ctrl12_is_present +
			query_5.ctrl13_is_present +
			query_5.ctrl14_is_present +
			query_5.ctrl15_is_present +
			query_5.ctrl16_is_present +
			query_5.ctrl17_is_present +
			query_5.ctrl18_is_present +
			query_5.ctrl19_is_present;

	ctrl_23_offset = ctrl_20_offset +
			query_5.ctrl20_is_present +
			query_5.ctrl21_is_present +
			query_5.ctrl22_is_present;

	ctrl_28_offset = ctrl_23_offset +
			query_5.ctrl23_is_present +
			query_5.ctrl24_is_present +
			query_5.ctrl25_is_present +
			query_5.ctrl26_is_present +
			query_5.ctrl27_is_present;

	ctrl_23_size = 2;
	for (subpacket = 2; subpacket <= 4; subpacket++) {
        retval = synaptics_rmi4_f12_find_sub(rmi4_data, query_5.data,
        		sizeof(query_5.data), 6, 23, subpacket);
        if (retval == 1)
        	ctrl_23_size++;
        else if (retval < 0) {
        	mtouch_error(MTOUCH_DEV, "%s: failed to find sub packet register", __FUNCTION__);
    		retval = -ENODEV;
            goto exit;
        }
	}

	reg_addr = rmi4_data->f12->base_addr.ctrl_base + ctrl_23_offset;
	retval = synaptics_rmi4_reg_read(rmi4_data, reg_addr, ctrl_23.data, ctrl_23_size);
    if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: failed to read register 0x%04x", __FUNCTION__, reg_addr);
		retval = -ENODEV;
        goto exit;
    }

    // maximum number of fingers supported
	rmi4_data->num_of_fingers = ctrl_23.max_reported_objects;

	reg_addr = rmi4_data->f12->base_addr.query_base + 7;
	retval = synaptics_rmi4_reg_read(rmi4_data, reg_addr, &size_of_query8, sizeof(size_of_query8));
    if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: failed to read register 0x%04x", __FUNCTION__, reg_addr);
		retval = -ENODEV;
        goto exit;
    }

	if (size_of_query8 > sizeof(query_8.data))
		size_of_query8 = sizeof(query_8.data);
	memset(query_8.data, 0x00, sizeof(query_8.data));

	reg_addr = rmi4_data->f12->base_addr.query_base + 8;
	retval = synaptics_rmi4_reg_read(rmi4_data, reg_addr, query_8.data, size_of_query8);
    if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: failed to read register 0x%04x", __FUNCTION__, reg_addr);
		retval = -ENODEV;
        goto exit;
    }

	// determine the presence of the data0 register
	extra_data->data1_offset = query_8.data0_is_present;

	if ((size_of_query8 >= 3) && (query_8.data15_is_present)) {
		extra_data->data15_offset = query_8.data0_is_present +
				query_8.data1_is_present +
				query_8.data2_is_present +
				query_8.data3_is_present +
				query_8.data4_is_present +
				query_8.data5_is_present +
				query_8.data6_is_present +
				query_8.data7_is_present +
				query_8.data8_is_present +
				query_8.data9_is_present +
				query_8.data10_is_present +
				query_8.data11_is_present +
				query_8.data12_is_present +
				query_8.data13_is_present +
				query_8.data14_is_present;
		extra_data->data15_size = (rmi4_data->num_of_fingers + 7) / 8;
	} else {
		extra_data->data15_size = 0;
	}

	rmi4_data->report_enable = RPT_DEFAULT;

	rmi4_data->report_enable |= RPT_Z;
	rmi4_data->report_enable |= (RPT_WX | RPT_WY);

	if (rmi4_data->f12->base_addr.ctrl_base + ctrl_28_offset) {
		reg_addr = rmi4_data->f12->base_addr.ctrl_base + ctrl_28_offset;
		retval = synaptics_rmi4_reg_read(rmi4_data, reg_addr, &report_enable, sizeof(report_enable));
	    if (retval < 0) {
			mtouch_error(MTOUCH_DEV, "%s: failed to read register 0x%04x", __FUNCTION__, reg_addr);
			retval = -ENODEV;
	        goto exit;
	    }
	}

	if (query_5.ctrl8_is_present) {
		reg_addr = rmi4_data->f12->base_addr.ctrl_base + ctrl_8_offset;
		retval = synaptics_rmi4_reg_read(rmi4_data, reg_addr, ctrl_8.data, sizeof(ctrl_8.data));
	    if (retval < 0) {
			mtouch_error(MTOUCH_DEV, "%s: failed to read register 0x%04x", __FUNCTION__, reg_addr);
			retval = -ENODEV;
	        goto exit;
	    }

	    // maximum x coordinate and y coordinate supported
	    rmi4_data->sensor_max_x =
	    		((unsigned int)ctrl_8.max_x_coord_lsb << 0) |
	    		((unsigned int)ctrl_8.max_x_coord_msb << 8);
	    rmi4_data->sensor_max_y =
	    		((unsigned int)ctrl_8.max_y_coord_lsb << 0) |
	    		((unsigned int)ctrl_8.max_y_coord_msb << 8);

	    rmi4_data->max_touch_width = MAX_F12_TOUCH_WIDTH;
    }

	rmi4_data->f12->data_size = rmi4_data->num_of_fingers * size_of_2d_data;
	rmi4_data->f12->data = calloc(1, rmi4_data->f12->data_size);
	if (!rmi4_data->f12->data) {
		mtouch_error(MTOUCH_DEV, "%s: failed to allocate private data for f12",
					__FUNCTION__);
		return -ENODEV;
	}

	retval = EOK;

exit:
	return retval;
}


/*
 * helper function to complete the device configuration
 *
 * struct synaptics_rmi4_data *rmi4_data: RMI4 device instance data
 *
 * return EOK: complete
 * otherwise, fail
 */
static int synaptics_rmi4_set_configured(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char device_ctrl;

	_CHECK_POINTER(rmi4_data);

	retval = synaptics_rmi4_reg_read(rmi4_data,
				rmi4_data->f01->base_addr.ctrl_base,
				&device_ctrl,
				sizeof(device_ctrl));
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: failed to read f01 ctrl_base", __FUNCTION__);
		return -EIO;
	}

	device_ctrl |= CONFIGURED;

	retval = synaptics_rmi4_reg_write(rmi4_data,
				rmi4_data->f01->base_addr.ctrl_base,
				&device_ctrl,
				sizeof(device_ctrl));
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: failed to set configured", __FUNCTION__);
		return -EIO;
	}

	return EOK;
}

/*
 * helper function to setup the synaptics_rmi4_fn
 *
 * save the base address of RMI command register, RMI query register, RMI control register,
 * and RMI data register. The interrupt mask is initialized as well
 *
 * struct synaptics_rmi4_fn_desc rmi_fd: a particular function descriptor fields in PDT entry
 * struct synaptics_rmi4_fn *rmi_fn    : a particular RMI Function
 * unsigned char page                  : page located
 * unsigned char int_msk               : interrupt mask
 *
 * return void
 */
static void synaptics_rmi4_set_func(struct synaptics_rmi4_fn_desc rmi_fd, struct synaptics_rmi4_fn *rmi_fn,
							unsigned char page, unsigned char int_msk)
{
	rmi_fn->base_addr.cmd_base   = (page << 8) | rmi_fd.cmd_base_addr;
	rmi_fn->base_addr.query_base = (page << 8) | rmi_fd.query_base_addr;
	rmi_fn->base_addr.ctrl_base  = (page << 8) | rmi_fd.ctrl_base_addr;
	rmi_fn->base_addr.data_base  = (page << 8) | rmi_fd.data_base_addr;

	if (int_msk != 0xff)
		rmi_fn->intr_mask = 1 << (int_msk);

	rmi_fn->func_num = rmi_fd.fn_number;

	return;
}


/*
 * to parse the Page Description Table (PDT) and reconstruct the entire
 * RMI register address map
 *
 * the PDT is defined as a general properties query register,
 * followed by an array of Function Descriptors.  The contents of the
 * Page Description table are stored from high page offsets down towards
 * lower page offsets, starting at page offset PdtTop.
 *
 * struct synaptics_rmi4_data *rmi4_data: RMI4 device instance data
 *
 * return EOK: complete device query
 * otherwise, fail
 */
static int synaptics_rmi4_query_device(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;

	unsigned char page_number;
	unsigned short pdt_entry_addr;
	struct synaptics_rmi4_fn_desc rmi_fd;

	unsigned char intr_count = 0;
	bool is_f01_found = false;
	bool is_f34_found = false;

	unsigned char f01_query[F01_STD_QUERY_LEN] = {0};

	_CHECK_POINTER(rmi4_data);

	memset(&rmi4_data->intr_mask, 0x00, sizeof(unsigned char));

	// Scan the page description tables of the pages to service
	for (page_number = 0; page_number < PAGES_TO_SERVICE; page_number++) {
		for (pdt_entry_addr = PDT_START; pdt_entry_addr > PDT_END; pdt_entry_addr -= PDT_ENTRY_SIZE) {

			pdt_entry_addr |= (page_number << 8);

            retval = synaptics_rmi4_reg_read(rmi4_data, pdt_entry_addr,
            			(unsigned char *)&rmi_fd, sizeof(rmi_fd));
            if (retval < 0)
                return -EIO;

            pdt_entry_addr &= ~(MASK_8BIT << 8);

            if (rmi_fd.fn_number == 0) {
                break;
            }

            if ((is_f01_found) && (is_f34_found) && (rmi_fd.fn_number == SYNAPTICS_RMI4_F34)) {
                break;
            }

            mtouch_info(MTOUCH_DEV, "%s: F%02x found (page %d)",
            			__FUNCTION__, rmi_fd.fn_number, page_number);

            switch (rmi_fd.fn_number) {

            case SYNAPTICS_RMI4_F01:	/* Function $01: RMI device control */
                if (0 == rmi_fd.intr_src_count)
                    break;

                rmi4_data->f01 = calloc(1, sizeof(struct synaptics_rmi4_fn));
				if (!rmi4_data->f01) {
					mtouch_error(MTOUCH_DEV, "%s: failed to allocate memory for f01",
								__FUNCTION__);
					return -ENODEV;
				}
				synaptics_rmi4_set_func(rmi_fd, rmi4_data->f01, page_number, intr_count);

				rmi4_data->f01->attn_handle = NULL;

				is_f01_found = true;

	            rmi4_data->intr_mask |= rmi4_data->f01->intr_mask;

				break;

            case SYNAPTICS_RMI4_F11:	/* Function $11: 2D sensors  */
                if (0 == rmi_fd.intr_src_count)
                    break;

            	rmi4_data->f11 = calloc(1, sizeof(struct synaptics_rmi4_fn));
				if (!rmi4_data->f11) {
					mtouch_error(MTOUCH_DEV, "%s: failed to allocate memory for f11",
								__FUNCTION__);
					return -ENODEV;
				}
				synaptics_rmi4_set_func(rmi_fd, rmi4_data->f11, page_number, intr_count);

				rmi4_data->f11->attn_handle = synaptics_rmi4_f11_abs_report;

				retval = synaptics_rmi4_f11_init(rmi4_data);
				if (retval != EOK) {
					mtouch_error(MTOUCH_DEV, "%s: failed to init f11",
								__FUNCTION__);
					return -ENODEV;
				}

	            rmi4_data->intr_mask |= rmi4_data->f11->intr_mask;

				break;

            case SYNAPTICS_RMI4_F12:	/* Function $12: 2D sensors  */
                if (0 == rmi_fd.intr_src_count)
                    break;

            	rmi4_data->f12 = calloc(1, sizeof(struct synaptics_rmi4_fn));
				if (!rmi4_data->f12) {
					mtouch_error(MTOUCH_DEV, "%s: failed to allocate memory for f12",
								__FUNCTION__);
					return -ENODEV;
				}
				synaptics_rmi4_set_func(rmi_fd, rmi4_data->f12, page_number, intr_count);

				rmi4_data->f12->attn_handle = synaptics_rmi4_f12_abs_report;

				retval = synaptics_rmi4_f12_init(rmi4_data);
				if (retval != EOK) {
					mtouch_error(MTOUCH_DEV, "%s: failed to init f12",
								__FUNCTION__);
					return -ENODEV;
				}

	            rmi4_data->intr_mask |= rmi4_data->f12->intr_mask;

				break;

            case SYNAPTICS_RMI4_F34:	/* Function $34: flash memory management  */
            	rmi4_data->f34 = calloc(1, sizeof(struct synaptics_rmi4_fn));
				if (!rmi4_data->f34) {
					mtouch_error(MTOUCH_DEV, "%s: failed to allocate memory for f34",
								__FUNCTION__);
					return -ENODEV;
				}
				synaptics_rmi4_set_func(rmi_fd, rmi4_data->f34, page_number, intr_count);

				rmi4_data->f34->attn_handle = NULL;

				retval = synaptics_rmi4_fwu_init(rmi4_data, rmi_fd.fn_version);
				if (retval != EOK) {
					mtouch_error(MTOUCH_DEV, "%s: failed to init f34",
								__FUNCTION__);
					return -ENODEV;
				}

				is_f34_found = true;

	            rmi4_data->intr_mask |= rmi4_data->f34->intr_mask;

				break;

			case SYNAPTICS_RMI4_F54:	/* Function $54: test reporting   */
				rmi4_data->f54 = calloc(1, sizeof(struct synaptics_rmi4_fn));
				if (!rmi4_data->f54) {
					mtouch_error(MTOUCH_DEV, "%s: failed to allocate memory for f54",
								__FUNCTION__);
					return -ENODEV;
				}
				synaptics_rmi4_set_func(rmi_fd, rmi4_data->f54, page_number, intr_count);

				rmi4_data->f54->attn_handle = NULL;

	            rmi4_data->intr_mask |= rmi4_data->f54->intr_mask;

				break;

			case SYNAPTICS_RMI4_F55:	/* Function $55: sensor tuning   */
				rmi4_data->f55 = calloc(1, sizeof(struct synaptics_rmi4_fn));
				if (!rmi4_data->f55) {
					mtouch_error(MTOUCH_DEV, "%s: failed to allocate memory for f55",
								__FUNCTION__);
					return -ENODEV;
				}
				synaptics_rmi4_set_func(rmi_fd, rmi4_data->f55, page_number, intr_count);

				rmi4_data->f55->attn_handle = NULL;

	            rmi4_data->intr_mask |= rmi4_data->f55->intr_mask;

				break;

            case SYNAPTICS_RMI4_F1A:	/* Function $1A: capacitive button sensors */
                if (0 == rmi_fd.intr_src_count)
                    break;

            	rmi4_data->f1a = calloc(1, sizeof(struct synaptics_rmi4_fn));
				if (!rmi4_data->f1a) {
					mtouch_error(MTOUCH_DEV, "%s: failed to allocate memory for f1a",
								__FUNCTION__);
					return -ENODEV;
				}
				synaptics_rmi4_set_func(rmi_fd, rmi4_data->f1a, page_number, intr_count);

				rmi4_data->f1a->attn_handle = NULL;

	            rmi4_data->intr_mask |= rmi4_data->f1a->intr_mask;
				break;

			default:
				break;
            }

			intr_count += rmi_fd.intr_src_count;
		}
	}

    if (!is_f01_found || !is_f34_found) {
		mtouch_error(MTOUCH_DEV, "%s: failed to find f01 and f34", __FUNCTION__);
        return -ENODEV;
    }

	rmi4_data->num_of_intr_regs = (intr_count + 7) / 8;
	mtouch_debug(MTOUCH_DEV, "%s: number of interrupt registers = %d",
    			__FUNCTION__, rmi4_data->num_of_intr_regs);

	// read manufacturer id and product information
	retval = synaptics_rmi4_reg_read(rmi4_data,
				rmi4_data->f01->base_addr.query_base,
				f01_query,
				F01_STD_QUERY_LEN);
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: failed to read product information from F01_query", __FUNCTION__);
		return retval;
	}

	rmi4_data->rmi4_mod_info.manufacturer_id = f01_query[0];
	rmi4_data->rmi4_mod_info.product_props = f01_query[1];
	rmi4_data->rmi4_mod_info.product_info[0] = f01_query[2];
	rmi4_data->rmi4_mod_info.product_info[1] = f01_query[3];
	memcpy(rmi4_data->rmi4_mod_info.product_id_string, &f01_query[11], PRODUCT_ID_SIZE);

	if (rmi4_data->rmi4_mod_info.manufacturer_id != 1) {
    	mtouch_error(MTOUCH_DEV, "%s: non-Synaptics device found, manufacturer ID = %d",
    			__FUNCTION__, rmi4_data->rmi4_mod_info.manufacturer_id);
	}

	mtouch_info(MTOUCH_DEV, "%s: product id = %s",
    			__FUNCTION__, rmi4_data->rmi4_mod_info.product_id_string);

	// read manufacturer id and firmware information
	retval = synaptics_rmi4_reg_read(rmi4_data,
				rmi4_data->f01->base_addr.query_base + F01_BUID_ID_OFFSET,
				rmi4_data->rmi4_mod_info.build_id,
				sizeof(rmi4_data->rmi4_mod_info.build_id));
    if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: failed to read fw information from F01_query", __FUNCTION__);
		return retval;
    }

	rmi4_data->firmware_id = (unsigned int)rmi4_data->rmi4_mod_info.build_id[0] +
			(unsigned int)rmi4_data->rmi4_mod_info.build_id[1] * 0x100 +
			(unsigned int)rmi4_data->rmi4_mod_info.build_id[2] * 0x10000;

	mtouch_info(MTOUCH_DEV, "%s: firmware id = %-7d",
				__FUNCTION__, rmi4_data->firmware_id);

	return EOK;
}


/*
 * release the all RMI functions
 *
 * syna_dev_t *dev   : mtouch device instance data
 *
 * return void
 */
static void synaptics_rmi4_empty_all_rmi_func(struct synaptics_rmi4_data *rmi4_data)
{
	rmi4_data->current_page = MASK_8BIT;

	// release RMI4 function description
	if (rmi4_data->f01) {
		free(rmi4_data->f01);
		rmi4_data->f01 = NULL;
	}
	if (rmi4_data->f11) {
		free(rmi4_data->f11);
		rmi4_data->f11 = NULL;
	}
	if (rmi4_data->f12) {

		free(rmi4_data->f12->data);
		rmi4_data->f12->data = NULL;

		free(rmi4_data->f12->extra);
		rmi4_data->f12->extra = NULL;

		free(rmi4_data->f12);
		rmi4_data->f12 = NULL;
	}
	if (rmi4_data->f34) {

		synaptics_rmi4_fwu_deinit();

		free(rmi4_data->f34);
		rmi4_data->f34 = NULL;
	}
	if (rmi4_data->f54) {
		free(rmi4_data->f54);
		rmi4_data->f54 = NULL;
	}
	if (rmi4_data->f55) {
		free(rmi4_data->f55);
		rmi4_data->f55 = NULL;
	}
	if (rmi4_data->f1a) {
		free(rmi4_data->f1a);
		rmi4_data->f1a = NULL;
	}

}

/*
 * re-initialize the Synaptics RMI device
 * perform the followings:
 *    - release all existed rmi function descriptors
 *    - re-query and parse the pdt
 *
 * return EOK: complete the process
 * otherwise, fail
 */
int synaptics_rmi4_reinit(void)
{
	int retval = -ENODEV;
	unsigned char data[2];

	_CHECK_POINTER(g_syna_dev);
	_CHECK_POINTER(g_syna_dev->rmi4_data);

	// initialize defaults
	g_syna_dev->rmi4_data->current_page = MASK_8BIT;

	mtouch_info(MTOUCH_DEV, "%s: reinitialize", __FUNCTION__);

	synaptics_rmi4_sw_reset(g_syna_dev->rmi4_data);

	synaptics_rmi4_empty_all_rmi_func(g_syna_dev->rmi4_data);

	// parse the Page Description Table
	// and construct the function descriptor for each RMI Functions
	retval = synaptics_rmi4_query_device(g_syna_dev->rmi4_data);
	if (EOK != retval) {
		mtouch_error(MTOUCH_DEV, "%s: failed to query the rmi device", __FUNCTION__);
		return -EIO;
    }

	synaptics_rmi4_int_enable(g_syna_dev->rmi4_data, true);

	// complete the device configuration
	synaptics_rmi4_set_configured(g_syna_dev->rmi4_data);

	if (g_syna_dev->touch_report) {
		free(g_syna_dev->touch_report);
		g_syna_dev->touch_report = NULL;
	}

	retval = synaptics_rmi4_reg_read(g_syna_dev->rmi4_data,
				g_syna_dev->rmi4_data->f01->base_addr.data_base,
				data,
				2);
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: failed to read device status", __FUNCTION__);
	}

	// initialize the touched report
	g_syna_dev->touch_report = calloc(g_syna_dev->rmi4_data->num_of_fingers,
							sizeof(struct touch_report_t));
	if (!g_syna_dev->touch_report) {
		mtouch_error(MTOUCH_DEV, "%s: failed to allocate memory for touch report",
					__FUNCTION__);
		return -ENOMEM;
	}

	return EOK;
}

/*
 * initialize the Synaptics RMI device
 * perform the followings:
 *    - set the necessary defaults
 *    - parse the page description table
 *
 * syna_dev_t *p_dev  : mtouch device instance data
 *
 * return EOK: complete initialization
 * otherwise, fail
 */
int synaptics_rmi4_init(syna_dev_t *p_dev)
{
	int retval = -ENODEV;
	unsigned char data[2];

	_CHECK_POINTER(p_dev);

	g_syna_dev = p_dev;

	// create a synaptics_rmi4_data instance
	p_dev->rmi4_data = calloc(1, sizeof(struct synaptics_rmi4_data));
	if (!p_dev->rmi4_data) {
		mtouch_error(MTOUCH_DEV, "%s: failed to allocate synaptics_rmi4_data structure",
					__FUNCTION__);
		return -ENOMEM;
	}

	// initialize defaults
	p_dev->rmi4_data->current_page = MASK_8BIT;

	p_dev->rmi4_data->f01 = NULL;
	p_dev->rmi4_data->f11 = NULL;
	p_dev->rmi4_data->f12 = NULL;
	p_dev->rmi4_data->f34 = NULL;
	p_dev->rmi4_data->f54 = NULL;
	p_dev->rmi4_data->f55 = NULL;
	p_dev->rmi4_data->f1a = NULL;

	// initialize the mutex
	pthread_mutex_init(&p_dev->rmi4_data->rmi4_io_ctrl_mutex, NULL);
	pthread_mutex_init(&p_dev->rmi4_data->rmi4_report_mutex, NULL);
	pthread_mutex_init(&p_dev->rmi4_data->rmi4_fwu_mutex, NULL);

	// parse the Page Description Table
	// and construct the function descriptor for each RMI Functions
	retval = synaptics_rmi4_query_device(p_dev->rmi4_data);
	if (EOK != retval) {
		mtouch_error(MTOUCH_DEV, "%s: failed to query the rmi device", __FUNCTION__);
		retval = -EIO;
        goto exit;
    }

	synaptics_rmi4_int_enable(p_dev->rmi4_data, true);

	// complete the device configuration
	synaptics_rmi4_set_configured(p_dev->rmi4_data);

	retval = synaptics_rmi4_reg_read(p_dev->rmi4_data,
				p_dev->rmi4_data->f01->base_addr.data_base,
				data,
				2);
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: failed to read device status", __FUNCTION__);
		retval = -EIO;
		goto exit;
	}
	else {
		if ((data[0] & 0x0f) != STATUS_NO_ERROR)
			mtouch_info(MTOUCH_DEV, "%s: device status = 0x%x", __FUNCTION__, data[0]);
	}

	// initialize the touched report
	p_dev->touch_report = calloc(p_dev->rmi4_data->num_of_fingers,
							sizeof(struct touch_report_t));
	if (!p_dev->touch_report) {
		mtouch_error(MTOUCH_DEV, "%s: failed to allocate memory for touch report",
					__FUNCTION__);
		retval = -ENOMEM;
		goto exit;
	}

    // RMI Version 4.0 currently supported
	p_dev->rmi4_data->rmi4_mod_info.version_major = 4;
	p_dev->rmi4_data->rmi4_mod_info.version_minor = 0;

	return EOK;

exit:
	synaptics_rmi4_empty_all_rmi_func(p_dev->rmi4_data);

	if (p_dev->rmi4_data) {
		free(p_dev->rmi4_data);
		p_dev->rmi4_data = NULL;
	}

	if (p_dev->touch_report) {
		free(p_dev->touch_report);
		p_dev->touch_report = NULL;
	}

	return retval;
}

/*
 * de-initialize the Synaptics RMI device
 *
 * syna_dev_t *p_dev  : mtouch device instance data
 *
 * return EOK: complete de-initialization
 * otherwise, fail
 */
int synaptics_rmi4_deinit(syna_dev_t *p_dev)
{
	_CHECK_POINTER(p_dev);
	_CHECK_POINTER(p_dev->rmi4_data);

	synaptics_rmi4_empty_all_rmi_func(p_dev->rmi4_data);

	// release the touch report
	if (p_dev->touch_report) {
		free(p_dev->touch_report);
		p_dev->touch_report = NULL;
	}

	// release RMI4 device instance data
	if (p_dev->rmi4_data) {
		free(p_dev->rmi4_data);
		p_dev->rmi4_data = NULL;
	}

	return EOK;
}
