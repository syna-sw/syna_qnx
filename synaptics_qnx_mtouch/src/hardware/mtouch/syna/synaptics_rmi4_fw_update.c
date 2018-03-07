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

enum flash_area {
	NONE = 0,
	UI_FIRMWARE,
	UI_CONFIG,
};

#define ENABLE_WAIT_MS (200)
#define WRITE_WAIT_MS (3)
#define ERASE_WAIT_MS (1000)

#define WAIT_FOR_IDLE_US (1000)
#define ENTER_FLASH_PROG_WAIT_US (20*1000)

#define READ_CONFIG_WAIT_US (20*1000)

#define MAX_WRITE_SIZE (64)

static struct synaptics_rmi4_fwu_handle *g_fwu;

/*
 * miscellaneous helper functions
 */
static inline void batohs(unsigned short *dest, unsigned char *src)
{
	*dest = src[1] * 0x100 + src[0];
}

static unsigned int le_to_uint(const unsigned char *ptr)
{
	return (unsigned int)ptr[0] +
			(unsigned int)ptr[1] * 0x100 +
			(unsigned int)ptr[2] * 0x10000 +
			(unsigned int)ptr[3] * 0x1000000;
}

/*
 * to get the size config buffer, and allocate the buffer
 *
 * return EOK: complete
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_allocate_read_config_buf(unsigned int count)
{
	_CHECK_POINTER(g_fwu);

	if (count > g_fwu->read_config_buf_size) {
		free(g_fwu->read_config_buf);
		g_fwu->read_config_buf = calloc(count, sizeof(unsigned char));
		if (!g_fwu->read_config_buf) {
			mtouch_error(MTOUCH_DEV, "%s: failed to alloc mem for fwu->read_config_buf",
					__FUNCTION__);

			g_fwu->read_config_buf_size = 0;
			return -ENOMEM;
		}
		g_fwu->read_config_buf_size = count;
	}

	return EOK;
}
/*
 * to check the contents
 *
 * return EOK: complete
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_compare_partition_tables(void)
{
	_CHECK_POINTER(g_fwu);

	g_fwu->incompatible_partition_tables = false;

	if (g_fwu->phyaddr.bl_image != g_fwu->img.phyaddr.bl_image)
		g_fwu->incompatible_partition_tables = true;
	else if (g_fwu->phyaddr.lockdown != g_fwu->img.phyaddr.lockdown)
		g_fwu->incompatible_partition_tables = true;
	else if (g_fwu->phyaddr.bl_config != g_fwu->img.phyaddr.bl_config)
		g_fwu->incompatible_partition_tables = true;
	else if (g_fwu->phyaddr.utility_param != g_fwu->img.phyaddr.utility_param)
		g_fwu->incompatible_partition_tables = true;

	if (g_fwu->bl_version == BL_V7) {
		if (g_fwu->phyaddr.fl_config != g_fwu->img.phyaddr.fl_config)
			g_fwu->incompatible_partition_tables = true;
	}

	g_fwu->new_partition_table = false;

	if (g_fwu->phyaddr.ui_firmware != g_fwu->img.phyaddr.ui_firmware)
		g_fwu->new_partition_table = true;
	else if (g_fwu->phyaddr.ui_config != g_fwu->img.phyaddr.ui_config)
		g_fwu->new_partition_table = true;

	if (g_fwu->flash_properties.has_disp_config) {
		if (g_fwu->phyaddr.dp_config != g_fwu->img.phyaddr.dp_config)
			g_fwu->new_partition_table = true;
	}

	if (g_fwu->has_guest_code) {
		if (g_fwu->phyaddr.guest_code != g_fwu->img.phyaddr.guest_code)
			g_fwu->new_partition_table = true;
	}

	return EOK;
}

/*
 * to parse the partition table
 *
 * return EOK: complete the parsing
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_parse_partition_table(const unsigned char *partition_table,
		struct block_count *blkcount, struct physical_address *phyaddr)
{
	unsigned char ii;
	unsigned char index;
	unsigned short partition_length;
	unsigned short physical_address;
	struct partition_table *ptable;

	_CHECK_POINTER(g_fwu);

	for (ii = 0; ii < g_fwu->partitions; ii++) {
		index = ii * 8 + 2;
		ptable = (struct partition_table *)&partition_table[index];
		partition_length = ptable->partition_length_15_8 << 8 |
				ptable->partition_length_7_0;
		physical_address = ptable->start_physical_address_15_8 << 8 |
				ptable->start_physical_address_7_0;

		mtouch_debug(MTOUCH_DEV, "%s: partition entry %d", __FUNCTION__, ii);
		mtouch_debug(MTOUCH_DEV, "%s: data: 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x",
					__FUNCTION__, partition_table[index + 0], partition_table[index + 1],
					partition_table[index + 2], partition_table[index + 3], partition_table[index + 4],
					partition_table[index + 5], partition_table[index + 6], partition_table[index + 7]);

		switch (ptable->partition_id) {
		case CORE_CODE_PARTITION:
			blkcount->ui_firmware = partition_length;
			phyaddr->ui_firmware = physical_address;
			mtouch_debug(MTOUCH_DEV, "%s: ui_firmware block count: %d",
						__FUNCTION__, blkcount->ui_firmware);
			blkcount->total_count += partition_length;
			break;
		case CORE_CONFIG_PARTITION:
			blkcount->ui_config = partition_length;
			phyaddr->ui_config = physical_address;
			mtouch_debug(MTOUCH_DEV, "%s: ui_config block count: %d",
						__FUNCTION__, blkcount->ui_config);
			blkcount->total_count += partition_length;
			break;
		case BOOTLOADER_PARTITION:
			blkcount->bl_image = partition_length;
			phyaddr->bl_image = physical_address;
			mtouch_debug(MTOUCH_DEV, "%s: bootloader block count: %d",
						__FUNCTION__, blkcount->bl_image);
			blkcount->total_count += partition_length;
			break;
		case UTILITY_PARAMETER_PARTITION:
			blkcount->utility_param = partition_length;
			phyaddr->utility_param = physical_address;
			mtouch_debug(MTOUCH_DEV, "%s: utility parameter block count: %d",
						__FUNCTION__, blkcount->utility_param);
			blkcount->total_count += partition_length;
			break;
		case DISPLAY_CONFIG_PARTITION:
			blkcount->dp_config = partition_length;
			phyaddr->dp_config = physical_address;
			mtouch_debug(MTOUCH_DEV, "%s: display config block count: %d",
						__FUNCTION__, blkcount->dp_config);
			blkcount->total_count += partition_length;
			break;
		case FLASH_CONFIG_PARTITION:
			blkcount->fl_config = partition_length;
			phyaddr->fl_config = physical_address;
			mtouch_debug(MTOUCH_DEV, "%s: flash config block count: %d",
						__FUNCTION__, blkcount->fl_config);
			blkcount->total_count += partition_length;
			break;
		case GUEST_CODE_PARTITION:
			blkcount->guest_code = partition_length;
			phyaddr->guest_code = physical_address;
			mtouch_debug(MTOUCH_DEV, "%s: guest code block count: %d",
						__FUNCTION__, blkcount->guest_code);
			blkcount->total_count += partition_length;
			break;
		case GUEST_SERIALIZATION_PARTITION:
			blkcount->pm_config = partition_length;
			phyaddr->pm_config = physical_address;
			mtouch_debug(MTOUCH_DEV, "%s: guest serialization block count: %d",
						__FUNCTION__, blkcount->pm_config);
			blkcount->total_count += partition_length;
			break;
		case GLOBAL_PARAMETERS_PARTITION:
			blkcount->bl_config = partition_length;
			phyaddr->bl_config = physical_address;
			mtouch_debug(MTOUCH_DEV, "%s: global parameters block count: %d",
						__FUNCTION__, blkcount->bl_config);
			blkcount->total_count += partition_length;
			break;
		case DEVICE_CONFIG_PARTITION:
			blkcount->lockdown = partition_length;
			phyaddr->lockdown = physical_address;
			mtouch_debug(MTOUCH_DEV, "%s: device config block count: %d",
					__FUNCTION__, blkcount->lockdown);
			blkcount->total_count += partition_length;
			break;
		};
	}

	return EOK;
}


/*
 * to parse the utility parameter part in the image file
 *
 * return void
 */
static void synaptics_rmi4_fwu_parse_image_header_10_utility(const unsigned char *image)
{
	unsigned char ii;
	unsigned char num_of_containers;
	unsigned int addr;
	unsigned int container_id;
	unsigned int length;
	const unsigned char *content;
	struct container_descriptor *descriptor;

	num_of_containers = g_fwu->img.utility.size / 4;

	for (ii = 0; ii < num_of_containers; ii++) {
		if (ii >= MAX_UTILITY_PARAMS)
			continue;
		addr = le_to_uint(g_fwu->img.utility.data + (ii * 4));
		descriptor = (struct container_descriptor *)(image + addr);
		container_id = descriptor->container_id[0] |
				descriptor->container_id[1] << 8;
		content = image + le_to_uint(descriptor->content_address);
		length = le_to_uint(descriptor->content_length);
		switch (container_id) {
		case UTILITY_PARAMETER_CONTAINER:
			g_fwu->img.utility_param[ii].data = content;
			g_fwu->img.utility_param[ii].size = length;
			g_fwu->img.utility_param_id[ii] = content[0];
			break;
		default:
			break;
		};
	}

	return;
}

/*
 * to parse the bootloader part in the image file
 *
 * return void
 */
static void synaptics_rmi4_fwu_parse_image_header_10_bootloader(const unsigned char *image)
{
	unsigned char ii;
	unsigned char num_of_containers;
	unsigned int addr;
	unsigned int container_id;
	unsigned int length;
	const unsigned char *content;
	struct container_descriptor *descriptor;

	num_of_containers = (g_fwu->img.bootloader.size - 4) / 4;

	for (ii = 1; ii <= num_of_containers; ii++) {
		addr = le_to_uint(g_fwu->img.bootloader.data + (ii * 4));
		descriptor = (struct container_descriptor *)(image + addr);
		container_id = descriptor->container_id[0] |
				descriptor->container_id[1] << 8;
		content = image + le_to_uint(descriptor->content_address);
		length = le_to_uint(descriptor->content_length);
		switch (container_id) {
		case BL_IMAGE_CONTAINER:
			g_fwu->img.bl_image.data = content;
			g_fwu->img.bl_image.size = length;
			break;
		case BL_CONFIG_CONTAINER:
		case GLOBAL_PARAMETERS_CONTAINER:
			g_fwu->img.bl_config.data = content;
			g_fwu->img.bl_config.size = length;
			break;
		case BL_LOCKDOWN_INFO_CONTAINER:
		case DEVICE_CONFIG_CONTAINER:
			g_fwu->img.lockdown.data = content;
			g_fwu->img.lockdown.size = length;
			break;
		default:
			break;
		};
	}

	return;
}

/*
 * to parse the image file contents
 *
 * return EOK: complete the parsing
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_parse_image_header_10(void)
{
	unsigned char ii;
	unsigned char num_of_containers;
	unsigned int addr;
	unsigned int offset;
	unsigned int container_id;
	unsigned int length;
	const unsigned char *image;
	const unsigned char *content;
	struct container_descriptor *descriptor;
	struct image_header_10 *header;

	_CHECK_POINTER(g_fwu);
	_CHECK_POINTER(g_fwu->image);

	image = g_fwu->image;
	header = (struct image_header_10 *)image;

	g_fwu->img.checksum = le_to_uint(header->checksum);

	/* address of top level container */
	offset = le_to_uint(header->top_level_container_start_addr);
	descriptor = (struct container_descriptor *)(image + offset);

	/* address of top level container content */
	offset = le_to_uint(descriptor->content_address);
	num_of_containers = le_to_uint(descriptor->content_length) / 4;

	for (ii = 0; ii < num_of_containers; ii++) {
		addr = le_to_uint(image + offset);
		offset += 4;
		descriptor = (struct container_descriptor *)(image + addr);
		container_id = descriptor->container_id[0] |
				descriptor->container_id[1] << 8;
		content = image + le_to_uint(descriptor->content_address);
		length = le_to_uint(descriptor->content_length);
		switch (container_id) {
		case UI_CONTAINER:
		case CORE_CODE_CONTAINER:
			g_fwu->img.ui_firmware.data = content;
			g_fwu->img.ui_firmware.size = length;
			break;
		case UI_CONFIG_CONTAINER:
		case CORE_CONFIG_CONTAINER:
			g_fwu->img.ui_config.data = content;
			g_fwu->img.ui_config.size = length;
			break;
		case BL_CONTAINER:
			g_fwu->img.bl_version = *content;
			g_fwu->img.bootloader.data = content;
			g_fwu->img.bootloader.size = length;
			synaptics_rmi4_fwu_parse_image_header_10_bootloader(image);
			break;
		case UTILITY_CONTAINER:
			g_fwu->img.utility.data = content;
			g_fwu->img.utility.size = length;
			synaptics_rmi4_fwu_parse_image_header_10_utility(image);
			break;
		case GUEST_CODE_CONTAINER:
			g_fwu->img.contains_guest_code = true;
			g_fwu->img.guest_code.data = content;
			g_fwu->img.guest_code.size = length;
			break;
		case DISPLAY_CONFIG_CONTAINER:
			g_fwu->img.contains_disp_config = true;
			g_fwu->img.dp_config.data = content;
			g_fwu->img.dp_config.size = length;
			break;
		case PERMANENT_CONFIG_CONTAINER:
		case GUEST_SERIALIZATION_CONTAINER:
			g_fwu->img.contains_perm_config = true;
			g_fwu->img.pm_config.data = content;
			g_fwu->img.pm_config.size = length;
			break;
		case FLASH_CONFIG_CONTAINER:
			g_fwu->img.contains_flash_config = true;
			g_fwu->img.fl_config.data = content;
			g_fwu->img.fl_config.size = length;
			break;
		case GENERAL_INFORMATION_CONTAINER:
			g_fwu->img.contains_firmware_id = true;
			g_fwu->img.firmware_id = le_to_uint(content + 4);
			break;
		default:
			break;
		}
	}

	return EOK;
}

/*
 * to parse the image file contents
 *
 * return EOK: complete the parsing
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_parse_image_header_05_06(void)
{
	const unsigned char * image;
	struct image_header_05_06 *header;

	_CHECK_POINTER(g_fwu);
	_CHECK_POINTER(g_fwu->image);

	image = g_fwu->image;
	header = (struct image_header_05_06 *)g_fwu->image;

	g_fwu->img.checksum = le_to_uint(header->checksum);

	g_fwu->img.bl_version = header->header_version;

	g_fwu->img.contains_bootloader = header->options_bootloader;

	if (g_fwu->img.contains_bootloader)
		g_fwu->img.bootloader_size = le_to_uint(header->bootloader_size);

	g_fwu->img.ui_firmware.size = le_to_uint(header->firmware_size);
	if (g_fwu->img.ui_firmware.size) {
		g_fwu->img.ui_firmware.data = image + IMAGE_AREA_OFFSET;
		if (g_fwu->img.contains_bootloader)
			g_fwu->img.ui_firmware.data += g_fwu->img.bootloader_size;
	}

	if ((g_fwu->img.bl_version == BL_V6) && header->options_tddi)
		g_fwu->img.ui_firmware.data = image + IMAGE_AREA_OFFSET;

	g_fwu->img.ui_config.size = le_to_uint(header->config_size);
	if (g_fwu->img.ui_config.size) {
		g_fwu->img.ui_config.data = g_fwu->img.ui_firmware.data + g_fwu->img.ui_firmware.size;
	}

	if (g_fwu->img.contains_bootloader || header->options_tddi)
		g_fwu->img.contains_disp_config = true;
	else
		g_fwu->img.contains_disp_config = false;

	if (g_fwu->img.contains_disp_config) {
		g_fwu->img.disp_config_offset = le_to_uint(header->dsp_cfg_addr);
		g_fwu->img.dp_config.size = le_to_uint(header->dsp_cfg_size);
		g_fwu->img.dp_config.data = image + g_fwu->img.disp_config_offset;
	}
	else {
		if ((PRODUCT_ID_SIZE > sizeof(g_fwu->img.cstmr_product_id)) ||
			(PRODUCT_ID_SIZE > sizeof(header->cstmr_product_id))) {
			mtouch_error(MTOUCH_DEV, "%s: invalid size of customer product id", __FUNCTION__);
			return -EINVAL;
		}
		memcpy(g_fwu->img.cstmr_product_id, header->cstmr_product_id, PRODUCT_ID_SIZE);
		g_fwu->img.cstmr_product_id[PRODUCT_ID_SIZE] = 0;
	}

	g_fwu->img.contains_firmware_id = header->options_firmware_id;
	if (g_fwu->img.contains_firmware_id)
		g_fwu->img.firmware_id = le_to_uint(header->firmware_id);

	if ((PRODUCT_ID_SIZE > sizeof(g_fwu->img.product_id)) ||
		(PRODUCT_ID_SIZE > sizeof(header->product_id))) {
		mtouch_error(MTOUCH_DEV, "%s: invalid size of product id", __FUNCTION__);
		return -EINVAL;
	}
	memcpy(g_fwu->img.product_id, header->product_id, PRODUCT_ID_SIZE);
	g_fwu->img.product_id[PRODUCT_ID_SIZE] = 0;

	g_fwu->img.lockdown.size = LOCKDOWN_SIZE;
	g_fwu->img.lockdown.data = image + IMAGE_AREA_OFFSET - LOCKDOWN_SIZE;

	return EOK;
}

/*
 * to call the proper function to parse the target image file
 * based on the header version defined in file
 *
 * return EOK: complete
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_parse_image_info(void)
{
	int retval;
	struct image_header_10 *header;

	_CHECK_POINTER(g_fwu);
	_CHECK_POINTER(g_fwu->image);

	header = (struct image_header_10 *)g_fwu->image;

	memset(&g_fwu->img, 0x00, sizeof(g_fwu->img));

	switch (header->major_header_version) {
	case IMAGE_HEADER_VERSION_05:
	case IMAGE_HEADER_VERSION_06:
		retval = synaptics_rmi4_fwu_parse_image_header_05_06();
		break;
	case IMAGE_HEADER_VERSION_10:
		retval = synaptics_rmi4_fwu_parse_image_header_10();
		break;
	default:
		mtouch_error(MTOUCH_DEV, "%s: unsupported image file format (0x%02x)",
					__FUNCTION__, header->major_header_version);
		retval = -ENODEV;
		break;
	}

	if (g_fwu->bl_version == BL_V7 || g_fwu->bl_version == BL_V8) {
		if (!g_fwu->img.contains_flash_config) {
			mtouch_error(MTOUCH_DEV, "%s: no flash config found in firmware image",
						__FUNCTION__);
			return -EINVAL;
		}

		synaptics_rmi4_fwu_parse_partition_table(g_fwu->img.fl_config.data,
				&g_fwu->img.blkcount, &g_fwu->img.phyaddr);

		if (g_fwu->img.blkcount.utility_param)
			g_fwu->img.contains_utility_param = true;

		synaptics_rmi4_fwu_compare_partition_tables();
	}
	else {
		g_fwu->new_partition_table = false;
		g_fwu->incompatible_partition_tables = false;
	}

	return retval;
}

/*
 * check the flash status from F34_FLASH_DATA03
 * to determine the device stays in bootloader mode
 *
 * and check the flash command from F34_FLASH_DATA02 as well
 *
 * return EOK: complete the status reading
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_read_flash_status(void)
{
	int retval;
	unsigned char status;
	unsigned char command;

	_CHECK_POINTER(g_fwu);
	_CHECK_POINTER(g_fwu->rmi4_data);

	retval = synaptics_rmi4_reg_read(g_fwu->rmi4_data,
				g_fwu->f34->base_addr.data_base + g_fwu->off.flash_status,
				&status,
				sizeof(status));
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: fail to read flash status (address=0x%x)"
				, __FUNCTION__
				, g_fwu->f34->base_addr.data_base + g_fwu->off.flash_status);
		return retval;
	}

	g_fwu->in_bl_mode = status >> 7;

	if (g_fwu->bl_version == BL_V5)
		g_fwu->flash_status = (status >> 4) & MASK_3BIT;
	else if (g_fwu->bl_version == BL_V6)
		g_fwu->flash_status = status & MASK_3BIT;
	else if (g_fwu->bl_version == BL_V7 || g_fwu->bl_version == BL_V8)
		g_fwu->flash_status = status & MASK_5BIT;

	if (g_fwu->flash_status != 0x00) {
		mtouch_warn(MTOUCH_DEV, "%s: flash status = %d, command = 0x%02x"
				, __FUNCTION__, g_fwu->flash_status, g_fwu->command);
	}

	if (g_fwu->bl_version == BL_V7 || g_fwu->bl_version == BL_V8) {
		if (g_fwu->flash_status == 0x08)
			g_fwu->flash_status = 0x00;
	}

	retval = synaptics_rmi4_reg_read(g_fwu->rmi4_data,
			g_fwu->f34->base_addr.data_base + g_fwu->off.flash_cmd,
			&command,
			sizeof(command));
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: fail to read flash command (address=0x%x)"
				, __FUNCTION__
				, g_fwu->f34->base_addr.data_base + g_fwu->off.flash_cmd);
		return retval;
	}

	if (g_fwu->bl_version == BL_V5)
		g_fwu->command = command & MASK_4BIT;
	else if (g_fwu->bl_version == BL_V6)
		g_fwu->command = command & MASK_6BIT;
	else if (g_fwu->bl_version == BL_V7 || g_fwu->bl_version == BL_V8)
		g_fwu->command = command;

	return EOK;
}

/*
 * a routine to wait for the completion of rmi operation
 * and also check the flash status
 *
 * return EOK: complete
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_wait_for_idle(int timeout_ms, bool poll)
{
	int count = 0;
	int timeout_count = timeout_ms + 1;

	_CHECK_POINTER(g_fwu);
	_CHECK_POINTER(g_fwu->rmi4_data);

	do {
		usleep(WAIT_FOR_IDLE_US); // 1 ms

		count++;
		if (poll || (count == timeout_count))
			synaptics_rmi4_fwu_read_flash_status();

		if ((g_fwu->command == CMD_IDLE) && (g_fwu->flash_status == 0x00))
			return EOK;
	} while (count < timeout_count);

	mtouch_error(MTOUCH_DEV, "%s: timed out waiting for idle status"
			, __FUNCTION__);

	return -ETIMEDOUT;
}

/*
 * for boot-loader 7, to write f34_data_01 to data_05 in one write operation
 *
 * return EOK: complete
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_write_f34_v7_command_single_transaction(unsigned char cmd)
{
	int retval;
	unsigned char data_base;
	struct f34_v7_data_1_5 data_1_5;

	_CHECK_POINTER(g_fwu);
	_CHECK_POINTER(g_fwu->rmi4_data);

	data_base = g_fwu->f34->base_addr.data_base;

	memset(data_1_5.data, 0x00, sizeof(data_1_5.data));

	switch (cmd) {
	case CMD_ERASE_ALL:
		data_1_5.partition_id = CORE_CODE_PARTITION;
		data_1_5.command = CMD_V7_ERASE_AP;
		break;
	case CMD_ERASE_UI_FIRMWARE:
		data_1_5.partition_id = CORE_CODE_PARTITION;
		data_1_5.command = CMD_V7_ERASE;
		break;
	case CMD_ERASE_BL_CONFIG:
		data_1_5.partition_id = GLOBAL_PARAMETERS_PARTITION;
		data_1_5.command = CMD_V7_ERASE;
		break;
	case CMD_ERASE_UI_CONFIG:
		data_1_5.partition_id = CORE_CONFIG_PARTITION;
		data_1_5.command = CMD_V7_ERASE;
		break;
	case CMD_ERASE_DISP_CONFIG:
		data_1_5.partition_id = DISPLAY_CONFIG_PARTITION;
		data_1_5.command = CMD_V7_ERASE;
		break;
	case CMD_ERASE_FLASH_CONFIG:
		data_1_5.partition_id = FLASH_CONFIG_PARTITION;
		data_1_5.command = CMD_V7_ERASE;
		break;
	case CMD_ERASE_GUEST_CODE:
		data_1_5.partition_id = GUEST_CODE_PARTITION;
		data_1_5.command = CMD_V7_ERASE;
		break;
	case CMD_ERASE_BOOTLOADER:
		data_1_5.partition_id = BOOTLOADER_PARTITION;
		data_1_5.command = CMD_V7_ERASE;
		break;
	case CMD_ERASE_UTILITY_PARAMETER:
		data_1_5.partition_id = UTILITY_PARAMETER_PARTITION;
		data_1_5.command = CMD_V7_ERASE;
		break;
	case CMD_ENABLE_FLASH_PROG:
		data_1_5.partition_id = BOOTLOADER_PARTITION;
		data_1_5.command = CMD_V7_ENTER_BL;
		break;
	};

	data_1_5.payload_0 = g_fwu->bootloader_id[0];
	data_1_5.payload_1 = g_fwu->bootloader_id[1];

	retval = synaptics_rmi4_reg_write(g_fwu->rmi4_data,
				data_base + g_fwu->off.partition_id,
				data_1_5.data,
				sizeof(data_1_5.data));
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: failed to write single transaction command", __FUNCTION__);
		return retval;
	}

	return EOK;
}

/*
 * for boot-loader 7, helper function to write a f34 command
 *
 * return EOK: complete
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_write_f34_v7_command(unsigned char cmd)
{
	int retval;
	unsigned char data_base;
	unsigned char command;

	_CHECK_POINTER(g_fwu);
	_CHECK_POINTER(g_fwu->rmi4_data);

	data_base = g_fwu->f34->base_addr.data_base;

	switch (cmd) {
	case CMD_WRITE_FW:
	case CMD_WRITE_CONFIG:
	case CMD_WRITE_LOCKDOWN:
	case CMD_WRITE_GUEST_CODE:
	case CMD_WRITE_BOOTLOADER:
	case CMD_WRITE_UTILITY_PARAM:
		command = CMD_V7_WRITE;
		break;
	case CMD_READ_CONFIG:
		command = CMD_V7_READ;
		break;
	case CMD_ERASE_ALL:
		command = CMD_V7_ERASE_AP;
		break;
	case CMD_ERASE_UI_FIRMWARE:
	case CMD_ERASE_BL_CONFIG:
	case CMD_ERASE_UI_CONFIG:
	case CMD_ERASE_DISP_CONFIG:
	case CMD_ERASE_FLASH_CONFIG:
	case CMD_ERASE_GUEST_CODE:
	case CMD_ERASE_BOOTLOADER:
	case CMD_ERASE_UTILITY_PARAMETER:
		command = CMD_V7_ERASE;
		break;
	case CMD_ENABLE_FLASH_PROG:
		command = CMD_V7_ENTER_BL;
		break;
	default:
		mtouch_error(MTOUCH_DEV, "%s: invalid command 0x%02x", __FUNCTION__, cmd);
		return -EINVAL;
	};

	g_fwu->command = command;

	switch (cmd) {
	case CMD_ERASE_ALL:
	case CMD_ERASE_UI_FIRMWARE:
	case CMD_ERASE_BL_CONFIG:
	case CMD_ERASE_UI_CONFIG:
	case CMD_ERASE_DISP_CONFIG:
	case CMD_ERASE_FLASH_CONFIG:
	case CMD_ERASE_GUEST_CODE:
	case CMD_ERASE_BOOTLOADER:
	case CMD_ERASE_UTILITY_PARAMETER:
	case CMD_ENABLE_FLASH_PROG:
		retval = synaptics_rmi4_fwu_write_f34_v7_command_single_transaction(cmd);
		if (retval < 0)
			return retval;
		else
			return EOK;
	default:
		break;
	};

	retval = synaptics_rmi4_reg_write(g_fwu->rmi4_data,
				data_base + g_fwu->off.flash_cmd,
				&command,
				sizeof(command));
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: failed to write flash command", __FUNCTION__);
		return retval;
	}

	return EOK;
}

/*
 * for boot-loader 5/6, helper function to write a f34 command
 *
 * return EOK: complete
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_write_f34_v5v6_command(unsigned char cmd)
{
	int retval;
	unsigned char data_base;
	unsigned char command;

	_CHECK_POINTER(g_fwu);
	_CHECK_POINTER(g_fwu->rmi4_data);

	data_base = g_fwu->f34->base_addr.data_base;

	switch (cmd) {
	case CMD_IDLE:
		command = CMD_V5V6_IDLE;
		break;
	case CMD_WRITE_FW:
		command = CMD_V5V6_WRITE_FW;
		break;
	case CMD_WRITE_CONFIG:
		command = CMD_V5V6_WRITE_CONFIG;
		break;
	case CMD_WRITE_LOCKDOWN:
		command = CMD_V5V6_WRITE_LOCKDOWN;
		break;
	case CMD_WRITE_GUEST_CODE:
		command = CMD_V5V6_WRITE_GUEST_CODE;
		break;
	case CMD_READ_CONFIG:
		command = CMD_V5V6_READ_CONFIG;
		break;
	case CMD_ERASE_ALL:
		command = CMD_V5V6_ERASE_ALL;
		break;
	case CMD_ERASE_UI_CONFIG:
		command = CMD_V5V6_ERASE_UI_CONFIG;
		break;
	case CMD_ERASE_DISP_CONFIG:
		command = CMD_V5V6_ERASE_DISP_CONFIG;
		break;
	case CMD_ERASE_GUEST_CODE:
		command = CMD_V5V6_ERASE_GUEST_CODE;
		break;
	case CMD_ENABLE_FLASH_PROG:
		command = CMD_V5V6_ENABLE_FLASH_PROG;
		break;
	default:
		mtouch_error(MTOUCH_DEV, "%s: invalid command 0x%02x", __FUNCTION__, cmd);
		return -EINVAL;
	}

	switch (cmd) {
	case CMD_ERASE_ALL:
	case CMD_ERASE_UI_CONFIG:
	case CMD_ERASE_DISP_CONFIG:
	case CMD_ERASE_GUEST_CODE:
	case CMD_ENABLE_FLASH_PROG:
		retval = synaptics_rmi4_reg_write(g_fwu->rmi4_data,
					data_base + g_fwu->off.payload,
					g_fwu->bootloader_id,
					sizeof(g_fwu->bootloader_id));
		if (retval < 0) {
			mtouch_error(MTOUCH_DEV, "%s: fail to write bootloader id", __FUNCTION__);
			return retval;
		}
		break;
	default:
		break;
	};

	g_fwu->command = command;

	retval = synaptics_rmi4_reg_write(g_fwu->rmi4_data,
				data_base +  g_fwu->off.flash_cmd,
				&command,
				sizeof(command));
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: fail to write command 0x%02x", __FUNCTION__, command);
		return retval;
	}

	return EOK;
}


/*
 * to dispatch the helper function to execute the f34 command
 *
 * return EOK: complete
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_write_f34_command(unsigned char cmd)
{
	int retval;

	_CHECK_POINTER(g_fwu);

	if (g_fwu->bl_version == BL_V7 || g_fwu->bl_version == BL_V8) {
		retval = synaptics_rmi4_fwu_write_f34_v7_command(cmd);
	}
	else
		retval = synaptics_rmi4_fwu_write_f34_v5v6_command(cmd);

	return retval;
}

/*
 * for boot-loader 7, to write the partition id by issuing specified f34 command
 *
 * return EOK: complete
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_write_f34_v7_partition_id(unsigned char cmd)
{
	int retval;
	unsigned char data_base;
	unsigned char partition;

	_CHECK_POINTER(g_fwu);
	_CHECK_POINTER(g_fwu->rmi4_data);

	data_base = g_fwu->f34->base_addr.data_base;

	switch (cmd) {
	case CMD_WRITE_FW:
		partition = CORE_CODE_PARTITION;
		break;
	case CMD_WRITE_CONFIG:
	case CMD_READ_CONFIG:
		if (g_fwu->config_area == UI_CONFIG_AREA)
			partition = CORE_CONFIG_PARTITION;
		else if (g_fwu->config_area == DP_CONFIG_AREA)
			partition = DISPLAY_CONFIG_PARTITION;
		else if (g_fwu->config_area == PM_CONFIG_AREA)
			partition = GUEST_SERIALIZATION_PARTITION;
		else if (g_fwu->config_area == BL_CONFIG_AREA)
			partition = GLOBAL_PARAMETERS_PARTITION;
		else if (g_fwu->config_area == FLASH_CONFIG_AREA)
			partition = FLASH_CONFIG_PARTITION;
		else if (g_fwu->config_area == UPP_AREA)
			partition = UTILITY_PARAMETER_PARTITION;
		break;
	case CMD_WRITE_LOCKDOWN:
		partition = DEVICE_CONFIG_PARTITION;
		break;
	case CMD_WRITE_GUEST_CODE:
		partition = GUEST_CODE_PARTITION;
		break;
	case CMD_WRITE_BOOTLOADER:
		partition = BOOTLOADER_PARTITION;
		break;
	case CMD_WRITE_UTILITY_PARAM:
		partition = UTILITY_PARAMETER_PARTITION;
		break;
	case CMD_ERASE_ALL:
		partition = CORE_CODE_PARTITION;
		break;
	case CMD_ERASE_BL_CONFIG:
		partition = GLOBAL_PARAMETERS_PARTITION;
		break;
	case CMD_ERASE_UI_CONFIG:
		partition = CORE_CONFIG_PARTITION;
		break;
	case CMD_ERASE_DISP_CONFIG:
		partition = DISPLAY_CONFIG_PARTITION;
		break;
	case CMD_ERASE_FLASH_CONFIG:
		partition = FLASH_CONFIG_PARTITION;
		break;
	case CMD_ERASE_GUEST_CODE:
		partition = GUEST_CODE_PARTITION;
		break;
	case CMD_ERASE_BOOTLOADER:
		partition = BOOTLOADER_PARTITION;
		break;
	case CMD_ENABLE_FLASH_PROG:
		partition = BOOTLOADER_PARTITION;
		break;
	default:
		mtouch_error(MTOUCH_DEV, "%s: invalid command 0x%02x", __FUNCTION__, cmd);
		return -EINVAL;
	};

	retval = synaptics_rmi4_reg_write(g_fwu->rmi4_data,
				data_base + g_fwu->off.partition_id,
				&partition,
				sizeof(partition));
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: failed to write partition ID", __FUNCTION__);
		return retval;
	}

	return EOK;
}

/*
 * to dispatch to write the partition id
 *
 * return EOK: complete
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_write_f34_partition_id(unsigned char cmd)
{
	int retval;

	if (g_fwu->bl_version == BL_V7 || g_fwu->bl_version == BL_V8)
		retval = synaptics_rmi4_fwu_write_f34_v7_partition_id(cmd);
	else
		retval = EOK;

	return retval;
}

/*
 * helper function to read the information of partition table for boot-loader 7
 *
 * return EOK: complete the parsing
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_read_f34_v7_partition_table(unsigned char *partition_table)
{
	int retval;
	unsigned char data_base;
	unsigned char length[2];
	unsigned short block_number = 0;

	_CHECK_POINTER(g_fwu);
	_CHECK_POINTER(g_fwu->rmi4_data);
	_CHECK_POINTER(partition_table);

	data_base = g_fwu->f34->base_addr.data_base;

	g_fwu->config_area = FLASH_CONFIG_AREA;

	retval = synaptics_rmi4_fwu_write_f34_partition_id(CMD_READ_CONFIG);
	if (retval < 0)
		return retval;

	retval = synaptics_rmi4_reg_write(g_fwu->rmi4_data,
				data_base + g_fwu->off.block_number,
				(unsigned char *)&block_number,
				sizeof(block_number));
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: failed to write block number", __FUNCTION__);
		return retval;
	}

	length[0] = (unsigned char)(g_fwu->flash_config_length & MASK_8BIT);
	length[1] = (unsigned char)(g_fwu->flash_config_length >> 8);

	retval = synaptics_rmi4_reg_write(g_fwu->rmi4_data,
				data_base + g_fwu->off.transfer_length,
				length,
				sizeof(length));
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: failed to write transfer length", __FUNCTION__);
		return retval;
	}

	retval = synaptics_rmi4_fwu_write_f34_command(CMD_READ_CONFIG);
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: failed to write command", __FUNCTION__);
		return retval;
	}

	usleep(READ_CONFIG_WAIT_US);

	retval = synaptics_rmi4_fwu_wait_for_idle(WRITE_WAIT_MS, true);
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: failed to wait for idle status", __FUNCTION__);
		return retval;
	}

	retval = synaptics_rmi4_reg_read(g_fwu->rmi4_data,
				data_base + g_fwu->off.payload,
				partition_table,
				g_fwu->partition_table_bytes);
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: failed to read block data", __FUNCTION__);
		return retval;
	}

	return EOK;
}

/*
 * to walk through the f34 query registers and
 * initialize the essential settings for boot-loader 7
 *
 * return EOK: complete the query
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_read_f34_v7_queries(void)
{
	int retval;
	unsigned char ii;
	unsigned char query_base;
	unsigned char index;
	unsigned char offset;
	unsigned char *ptable;
	struct f34_v7_query_0 query_0;
	struct f34_v7_query_1_7 query_1_7;

	_CHECK_POINTER(g_fwu);
	_CHECK_POINTER(g_fwu->rmi4_data);

	query_base = g_fwu->f34->base_addr.query_base;

	retval = synaptics_rmi4_reg_read(g_fwu->rmi4_data,
				query_base,
				query_0.data,
				sizeof(query_0.data));
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: fail to read query 0", __FUNCTION__);
		return retval;
	}

	offset = query_0.subpacket_1_size + 1;

	retval = synaptics_rmi4_reg_read(g_fwu->rmi4_data,
				query_base + offset,
				query_1_7.data,
				sizeof(query_1_7.data));
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: fail to read query 1 to 7", __FUNCTION__);
		return retval;
	}

	g_fwu->bootloader_id[0] = query_1_7.bl_minor_revision;
	g_fwu->bootloader_id[1] = query_1_7.bl_major_revision;

	if (g_fwu->bootloader_id[1] == BL_V8)
		g_fwu->bl_version = BL_V8;

	g_fwu->block_size = query_1_7.block_size_15_8 << 8 |
			query_1_7.block_size_7_0;

	g_fwu->flash_config_length = query_1_7.flash_config_length_15_8 << 8 |
			query_1_7.flash_config_length_7_0;

	g_fwu->payload_length = query_1_7.payload_length_15_8 << 8 |
			query_1_7.payload_length_7_0;

	g_fwu->off.flash_status = V7_FLASH_STATUS_OFFSET;
	g_fwu->off.partition_id = V7_PARTITION_ID_OFFSET;
	g_fwu->off.block_number = V7_BLOCK_NUMBER_OFFSET;
	g_fwu->off.transfer_length = V7_TRANSFER_LENGTH_OFFSET;
	g_fwu->off.flash_cmd = V7_COMMAND_OFFSET;
	g_fwu->off.payload = V7_PAYLOAD_OFFSET;

	index = sizeof(query_1_7.data) - V7_PARTITION_SUPPORT_BYTES;

	g_fwu->partitions = 0;
	for (offset = 0; offset < V7_PARTITION_SUPPORT_BYTES; offset++) {
		for (ii = 0; ii < 8; ii++) {
			if (query_1_7.data[index + offset] & (1 << ii))
				g_fwu->partitions++;
		}

		mtouch_debug(MTOUCH_DEV, "%s: supported partitions: 0x%02x",
					__FUNCTION__, query_1_7.data[index + offset]);
	}

	g_fwu->partition_table_bytes = g_fwu->partitions * 8 + 2;


	ptable = calloc(g_fwu->partition_table_bytes, sizeof(unsigned char));
	if (!ptable) {
		mtouch_error(MTOUCH_DEV, "%s: failed to allocate memory for partition table",
					__FUNCTION__);
		return -ENOMEM;
	}

	retval = synaptics_rmi4_fwu_read_f34_v7_partition_table(ptable);
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: fail to read partition table", __FUNCTION__);
		free(ptable);
		return retval;
	}

	synaptics_rmi4_fwu_parse_partition_table(ptable, &g_fwu->blkcount, &g_fwu->phyaddr);

	if (g_fwu->blkcount.dp_config)
		g_fwu->flash_properties.has_disp_config = 1;
	else
		g_fwu->flash_properties.has_disp_config = 0;

	if (g_fwu->blkcount.pm_config)
		g_fwu->flash_properties.has_pm_config = 1;
	else
		g_fwu->flash_properties.has_pm_config = 0;

	if (g_fwu->blkcount.bl_config)
		g_fwu->flash_properties.has_bl_config = 1;
	else
		g_fwu->flash_properties.has_bl_config = 0;

	if (g_fwu->blkcount.guest_code)
		g_fwu->has_guest_code = true;
	else
		g_fwu->has_guest_code = false;

	if (g_fwu->blkcount.utility_param)
		g_fwu->has_utility_param = true;
	else
		g_fwu->has_utility_param = false;

	free(ptable);

	return EOK;
}

/*
 * to walk through the f34 query registers and
 * initialize the essential settings for boot-loader 5 or 6
 *
 * return EOK: complete the query
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_read_f34_v5v6_queries(void)
{
	int retval;
	unsigned char count;
	unsigned char base;
	unsigned char offset;
	unsigned char buf[10];
	struct f34_v5v6_flash_properties_2 properties_2;

	_CHECK_POINTER(g_fwu);
	_CHECK_POINTER(g_fwu->rmi4_data);

	base = g_fwu->f34->base_addr.query_base;

	retval = synaptics_rmi4_reg_read(g_fwu->rmi4_data,
				base + V5V6_BOOTLOADER_ID_OFFSET,
				g_fwu->bootloader_id,
				sizeof(g_fwu->bootloader_id));
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: fail to read bootloader id", __FUNCTION__);
		return retval;
	}

	if (g_fwu->bl_version == BL_V5) {
		g_fwu->off.properties = V5_PROPERTIES_OFFSET;
		g_fwu->off.block_size = V5_BLOCK_SIZE_OFFSET;
		g_fwu->off.block_count = V5_BLOCK_COUNT_OFFSET;
		g_fwu->off.block_number = V5_BLOCK_NUMBER_OFFSET;
		g_fwu->off.payload = V5_BLOCK_DATA_OFFSET;
	}
	else if (g_fwu->bl_version == BL_V6) {
		g_fwu->off.properties = V6_PROPERTIES_OFFSET;
		g_fwu->off.properties_2 = V6_PROPERTIES_2_OFFSET;
		g_fwu->off.block_size = V6_BLOCK_SIZE_OFFSET;
		g_fwu->off.block_count = V6_BLOCK_COUNT_OFFSET;
		g_fwu->off.gc_block_count = V6_GUEST_CODE_BLOCK_COUNT_OFFSET;
		g_fwu->off.block_number = V6_BLOCK_NUMBER_OFFSET;
		g_fwu->off.payload = V6_BLOCK_DATA_OFFSET;
	}

	retval = synaptics_rmi4_reg_read(g_fwu->rmi4_data,
			base + g_fwu->off.block_size, buf, 2);
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: fail to read block size info", __FUNCTION__);
		return retval;
	}

	batohs(&g_fwu->block_size, &(buf[0]));

	if (g_fwu->bl_version == BL_V5) {
		g_fwu->off.flash_cmd = g_fwu->off.payload + g_fwu->block_size;
		g_fwu->off.flash_status = g_fwu->off.flash_cmd;
	}
	else if (g_fwu->bl_version == BL_V6) {
		g_fwu->off.flash_cmd = V6_FLASH_COMMAND_OFFSET;
		g_fwu->off.flash_status = V6_FLASH_STATUS_OFFSET;
	}

	retval = synaptics_rmi4_reg_read(g_fwu->rmi4_data,
				base + g_fwu->off.properties,
				g_fwu->flash_properties.data,
				sizeof(g_fwu->flash_properties.data));
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: fail to read flash properties", __FUNCTION__);
		return retval;
	}

	count = 4;

	if (g_fwu->flash_properties.has_pm_config)
		count += 2;

	if (g_fwu->flash_properties.has_bl_config)
		count += 2;

	if (g_fwu->flash_properties.has_disp_config)
		count += 2;

	retval = synaptics_rmi4_reg_read(g_fwu->rmi4_data,
			base + g_fwu->off.block_count, buf, count);
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: fail to read block count", __FUNCTION__);
		return retval;
	}

	batohs(&g_fwu->blkcount.ui_firmware, &(buf[0]));
	batohs(&g_fwu->blkcount.ui_config, &(buf[2]));

	count = 4;

	if (g_fwu->flash_properties.has_pm_config) {
		batohs(&g_fwu->blkcount.pm_config, &(buf[count]));
		count += 2;
	}

	if (g_fwu->flash_properties.has_bl_config) {
		batohs(&g_fwu->blkcount.bl_config, &(buf[count]));
		count += 2;
	}

	if (g_fwu->flash_properties.has_disp_config)
		batohs(&g_fwu->blkcount.dp_config, &(buf[count]));

	if (g_fwu->flash_properties.has_query4) {
		retval = synaptics_rmi4_reg_read(g_fwu->rmi4_data,
					base + g_fwu->off.properties_2,
					properties_2.data,
					sizeof(properties_2.data));
		if (retval < 0) {
			mtouch_error(MTOUCH_DEV, "%s: fail to read flash properties 2", __FUNCTION__);
			return retval;
		}
		offset = g_fwu->off.properties_2 + 1;
		count = 0;
		if (properties_2.has_guest_code) {
			retval = synaptics_rmi4_reg_read(g_fwu->rmi4_data,
						base + offset + count,
						buf,
						2);
			if (retval < 0) {
				mtouch_error(MTOUCH_DEV, "%s: fail to read guest code block count", __FUNCTION__);
				return retval;
			}

			batohs(&g_fwu->blkcount.guest_code, &(buf[0]));
			count++;
			g_fwu->has_guest_code = true;
		}
	}

	g_fwu->has_utility_param = false;

	return EOK;
}

/*
 * dispatch the helper function to initialize the f34 query registers
 *
 * return EOK: complete
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_read_f34_queries(void)
{
	int retval;

	_CHECK_POINTER(g_fwu);

	memset(&g_fwu->blkcount, 0x00, sizeof(g_fwu->blkcount));
	memset(&g_fwu->phyaddr, 0x00, sizeof(g_fwu->phyaddr));

	if (g_fwu->bl_version == BL_V7 || g_fwu->bl_version == BL_V8) {
		retval = synaptics_rmi4_fwu_read_f34_v7_queries();
	}
	else
		retval = synaptics_rmi4_fwu_read_f34_v5v6_queries();

	return retval;
}

/*
 * for boot-loader 7, to write assigned data blocks into the flash memory
 *
 * return EOK: complete the process
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_write_f34_v7_blocks(unsigned char *block_ptr,
		unsigned short block_cnt, unsigned char command)
{
	int retval;
	unsigned char data_base;
	unsigned char length[2];
	unsigned short transfer;
	unsigned short remaining = block_cnt;
	unsigned short block_number = 0;
	unsigned short left_bytes;
	unsigned short write_size;
	unsigned short max_write_size;
	struct synaptics_rmi4_data *rmi4_data;

	_CHECK_POINTER(g_fwu);
	_CHECK_POINTER(g_fwu->rmi4_data);

	rmi4_data = g_fwu->rmi4_data;
	data_base = g_fwu->f34->base_addr.data_base;;

	retval = synaptics_rmi4_fwu_write_f34_partition_id(command);
	if (retval < 0)
		return retval;

	retval = synaptics_rmi4_reg_write(rmi4_data,
				data_base + g_fwu->off.block_number,
				(unsigned char *)&block_number,
				sizeof(block_number));
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: failed to write block number",
					__FUNCTION__);
		return retval;
	}

	mtouch_debug(MTOUCH_DEV, "%s: block_cnt = %d, g_fwu->block_size = %d, payload_length = %d",
				__FUNCTION__, block_cnt, g_fwu->block_size, g_fwu->payload_length);

	do {
		if (remaining / g_fwu->payload_length)
			transfer = g_fwu->payload_length;
		else
			transfer = remaining;

		mtouch_debug(MTOUCH_DEV, "%s: transfer = %d, remaining = %d",
					__FUNCTION__, transfer, remaining);

		length[0] = (unsigned char)(transfer & MASK_8BIT);
		length[1] = (unsigned char)(transfer >> 8);

		retval = synaptics_rmi4_reg_write(rmi4_data,
					data_base + g_fwu->off.transfer_length,
					length,
					sizeof(length));
		if (retval < 0) {
			mtouch_error(MTOUCH_DEV, "%s: failed to write transfer length (remaining = %d)",
						__FUNCTION__, remaining);
			return retval;
		}

		retval = synaptics_rmi4_fwu_write_f34_command(command);
		if (retval < 0) {
			mtouch_error(MTOUCH_DEV, "%s: failed to write command (remaining = %d)",
						__FUNCTION__, remaining);
			return retval;
		}

		max_write_size = MAX_WRITE_SIZE;
		if (max_write_size >= transfer * g_fwu->block_size)
			max_write_size = transfer * g_fwu->block_size;
		else if (max_write_size > g_fwu->block_size)
			max_write_size -= max_write_size % g_fwu->block_size;
		else
			max_write_size = g_fwu->block_size;

		left_bytes = transfer * g_fwu->block_size;

		do {
			if (left_bytes / max_write_size)
				write_size = max_write_size;
			else
				write_size = left_bytes;

			retval = synaptics_rmi4_reg_write(rmi4_data,
						data_base + g_fwu->off.payload,
						block_ptr,
						write_size);
			if (retval < 0) {
				mtouch_error(MTOUCH_DEV, "%s: failed to write block data (remaining = %d)",
							__FUNCTION__, remaining);
				return retval;
			}

			block_ptr += write_size;
			left_bytes -= write_size;
		} while (left_bytes);

		retval = synaptics_rmi4_fwu_wait_for_idle(WRITE_WAIT_MS*10, true);
		if (retval < 0) {
			mtouch_error(MTOUCH_DEV, "%s: failed to wait for idle status (remaining = %d)",
						__FUNCTION__, remaining);
			return retval;
		}

		remaining -= transfer;
	} while (remaining);

	return EOK;
}

/*
 * for boot-loader 5/6, to write assigned data blocks into the flash memory
 *
 * return EOK: complete the process
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_write_f34_v5v6_blocks(unsigned char *block_ptr,
		unsigned short block_cnt, unsigned char command)
{
	int retval;
	unsigned char data_base;
	unsigned char block_number[] = {0, 0};
	unsigned short blk;
	struct synaptics_rmi4_data *rmi4_data;

	_CHECK_POINTER(g_fwu);
	_CHECK_POINTER(g_fwu->rmi4_data);

	rmi4_data = g_fwu->rmi4_data;
	data_base = g_fwu->f34->base_addr.data_base;;

	block_number[1] |= (g_fwu->config_area << 5);

	retval = synaptics_rmi4_reg_write(rmi4_data,
				data_base + g_fwu->off.block_number,
				block_number,
				sizeof(block_number));
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: failed to write block number %d to 0x%x",
					__FUNCTION__, block_number, data_base + g_fwu->off.block_number);
		return retval;
	}

	for (blk = 0; blk < block_cnt; blk++) {
		retval = synaptics_rmi4_reg_write(rmi4_data,
					data_base + g_fwu->off.payload,
					block_ptr,
					g_fwu->block_size);
		if (retval < 0) {
			mtouch_error(MTOUCH_DEV, "%s: failed to write block data (block %d) to 0x%x",
						__FUNCTION__, blk, data_base + g_fwu->off.payload);
			return retval;
		}

		retval = synaptics_rmi4_fwu_write_f34_command(command);
		if (retval < 0) {
			mtouch_error(MTOUCH_DEV, "%s: failed to write command for block %d",
						__FUNCTION__, blk);
			return retval;
		}

		retval = synaptics_rmi4_fwu_wait_for_idle(WRITE_WAIT_MS, false);
		if (retval < 0) {
			mtouch_error(MTOUCH_DEV, "%s: failed to wait for idle status (block %d)",
						__FUNCTION__, blk);
			return retval;
		}

		block_ptr += g_fwu->block_size;
	}

	return EOK;
}

/*
 * to dispatch the proper function to write the data block
 *
 * return EOK: complete
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_write_f34_blocks(unsigned char *block_ptr,
			unsigned short block_cnt, unsigned char cmd)
{
	int retval;

	_CHECK_POINTER(g_fwu);

	if (g_fwu->bl_version == BL_V7 || g_fwu->bl_version == BL_V8) {
		retval = synaptics_rmi4_fwu_write_f34_v7_blocks(block_ptr, block_cnt, cmd);
	}
	else
		retval = synaptics_rmi4_fwu_write_f34_v5v6_blocks(block_ptr, block_cnt, cmd);

	return retval;
}

/*
 * for boot-loader 7, to read the data blocks
 *
 * return EOK: complete the process
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_read_f34_v7_blocks(unsigned short block_cnt,
			unsigned char command)
{
	int retval;
	unsigned char data_base;
	unsigned char length[2];
	unsigned short transfer;
	unsigned short remaining = block_cnt;
	unsigned short block_number = 0;
	unsigned short index = 0;

	_CHECK_POINTER(g_fwu);
	_CHECK_POINTER(g_fwu->rmi4_data);

	data_base = g_fwu->f34->base_addr.data_base;

	retval = synaptics_rmi4_fwu_write_f34_partition_id(command);
	if (retval < 0)
		return retval;

	retval = synaptics_rmi4_reg_write(g_fwu->rmi4_data,
				data_base + g_fwu->off.block_number,
				(unsigned char *)&block_number,
				sizeof(block_number));
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: failed to write block number",
					__FUNCTION__);
		return retval;
	}

	do {
		if (remaining / g_fwu->payload_length)
			transfer = g_fwu->payload_length;
		else
			transfer = remaining;

		length[0] = (unsigned char)(transfer & MASK_8BIT);
		length[1] = (unsigned char)(transfer >> 8);

		retval = synaptics_rmi4_reg_write(g_fwu->rmi4_data,
					data_base + g_fwu->off.transfer_length,
					length,
					sizeof(length));
		if (retval < 0) {
			mtouch_error(MTOUCH_DEV, "%s: failed to write transfer length (remaining = %d)",
						__FUNCTION__, remaining);
			return retval;
		}

		retval = synaptics_rmi4_fwu_write_f34_command(command);
		if (retval < 0) {
			mtouch_error(MTOUCH_DEV, "%s: failed to write command (remaining = %d)",
						__FUNCTION__, remaining);
			return retval;
		}

		retval = synaptics_rmi4_fwu_wait_for_idle(WRITE_WAIT_MS, false);
		if (retval < 0) {
			mtouch_error(MTOUCH_DEV, "%s: failed to wait for idle status (remaining = %d)",
						__FUNCTION__, remaining);
			return retval;
		}

		retval = synaptics_rmi4_reg_read(g_fwu->rmi4_data,
					data_base + g_fwu->off.payload,
					&g_fwu->read_config_buf[index],
					transfer * g_fwu->block_size);
		if (retval < 0) {
			mtouch_error(MTOUCH_DEV, "%s: failed to read block data (remaining = %d)",
						__FUNCTION__, remaining);
			return retval;
		}

		index += (transfer * g_fwu->block_size);
		remaining -= transfer;
	} while (remaining);

	return EOK;
}

/*
 * for boot-loader 5/6, to read the data blocks
 *
 * return EOK: complete the process
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_read_f34_v5v6_blocks(unsigned short block_cnt,
			unsigned char command)
{
	int retval;
	unsigned char data_base;
	unsigned char block_number[] = {0, 0};
	unsigned short blk;
	unsigned short index = 0;

	_CHECK_POINTER(g_fwu);
	_CHECK_POINTER(g_fwu->rmi4_data);


	data_base = g_fwu->f34->base_addr.data_base;

	block_number[1] |= (g_fwu->config_area << 5);

	retval = synaptics_rmi4_reg_write(g_fwu->rmi4_data,
				data_base + g_fwu->off.block_number,
				block_number,
				sizeof(block_number));
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: failed to write block number",
					__FUNCTION__);
		return retval;
	}

	for (blk = 0; blk < block_cnt; blk++) {
		retval = synaptics_rmi4_fwu_write_f34_command(command);
		if (retval < 0) {
			mtouch_error(MTOUCH_DEV, "%s: failed to write read config command",
						__FUNCTION__);
			return retval;
		}

		retval = synaptics_rmi4_fwu_wait_for_idle(WRITE_WAIT_MS*10, true);
		if (retval < 0) {
			mtouch_error(MTOUCH_DEV, "%s: failed to wait for idle status",
						__FUNCTION__);
			return retval;
		}

		retval = synaptics_rmi4_reg_read(g_fwu->rmi4_data,
					data_base + g_fwu->off.payload,
					&g_fwu->read_config_buf[index],
					g_fwu->block_size);
		if (retval < 0) {
			mtouch_error(MTOUCH_DEV, "%s: failed to read block data (block %d)",
						__FUNCTION__, blk);
			return retval;
		}

		index += g_fwu->block_size;
	}

	return EOK;
}

/*
 * to dispatch the proper function to read the data block
 *
 * return EOK: complete
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_read_f34_blocks(unsigned short block_cnt, unsigned char cmd)
{
	int retval;

	_CHECK_POINTER(g_fwu);

	if (g_fwu->bl_version == BL_V7 || g_fwu->bl_version == BL_V8)
		retval = synaptics_rmi4_fwu_read_f34_v7_blocks(block_cnt, cmd);
	else
		retval = synaptics_rmi4_fwu_read_f34_v5v6_blocks(block_cnt, cmd);

	return retval;
}

/*
 * to read the configuration id
 *
 * return EOK: complete
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_get_device_config_id(void)
{
	int retval;
	unsigned char config_id_size;

	_CHECK_POINTER(g_fwu);
	_CHECK_POINTER(g_fwu->rmi4_data);

	if (g_fwu->bl_version == BL_V7 || g_fwu->bl_version == BL_V8)
		config_id_size = V7_CONFIG_ID_SIZE;
	else
		config_id_size = V5V6_CONFIG_ID_SIZE;

	retval = synaptics_rmi4_reg_read(g_fwu->rmi4_data,
				g_fwu->f34->base_addr.ctrl_base,
				g_fwu->config_id,
				config_id_size);
	if (retval < 0)
		return retval;

	return EOK;
}

/*
 * to determine the flash area
 *
 * return EOK: complete
 * otherwise, fail
 */
static enum flash_area synaptics_rmi4_fwu_go_nogo(const unsigned int image_fw_id)
{
	int retval;
	enum flash_area flash_area = NONE;
	unsigned char ii;
	unsigned char config_id_size;
	unsigned int device_fw_id;
	struct synaptics_rmi4_data *rmi4_data;

	_CHECK_POINTER(g_fwu);
	_CHECK_POINTER(g_fwu->rmi4_data);

	rmi4_data = g_fwu->rmi4_data;

	if (g_fwu->force_update) {
		flash_area = UI_FIRMWARE;
		goto exit;
	}

	// update both UI and config if device is in bootloader mode
	if (g_fwu->bl_mode_device) {
		flash_area = UI_FIRMWARE;
		goto exit;
	}

	// get device firmware ID
	device_fw_id = rmi4_data->firmware_id;
	mtouch_info(MTOUCH_DEV, "%s: device firmware ID = %d",
			__FUNCTION__, device_fw_id);

	// get image firmware ID
	mtouch_info(MTOUCH_DEV, "%s: image firmware ID = %d",
			__FUNCTION__, image_fw_id);

	if (image_fw_id != device_fw_id) {
		flash_area = UI_FIRMWARE;
		goto exit;
	}
	else {
		flash_area = NONE;
		goto exit;
	}

	// get device config ID
	retval = synaptics_rmi4_fwu_get_device_config_id();
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: fail to read device config ID", __FUNCTION__);
		flash_area = NONE;
		goto exit;
	}

	if (g_fwu->bl_version == BL_V7 || g_fwu->bl_version == BL_V8)
		config_id_size = V7_CONFIG_ID_SIZE;
	else
		config_id_size = V5V6_CONFIG_ID_SIZE;

	for (ii = 0; ii < config_id_size; ii++) {
		if (g_fwu->img.ui_config.data[ii] > g_fwu->config_id[ii]) {
			flash_area = UI_CONFIG;
			goto exit;
		}
		else if (g_fwu->img.ui_config.data[ii] < g_fwu->config_id[ii]) {
			flash_area = NONE;
			goto exit;
		}
	}

	flash_area = NONE;

exit:
	if (flash_area == NONE) {
		mtouch_info(MTOUCH_DEV, "%s: no need to do reflash",
				__FUNCTION__);
	}
	else {
		mtouch_info(MTOUCH_DEV, "%s: updating %s",
				__FUNCTION__,
				flash_area == UI_FIRMWARE ? "UI firmware and Config":"UI Config only");
	}

	return flash_area;
}

/*
 * specified helper function to parse the pdt for f34
 * since the register mapping may be changed in bootloader mode
 *
 * return EOK: complete the process
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_scan_pdt(void)
{
	int retval;
	unsigned char ii;
	unsigned char intr_count = 0;
	unsigned char intr_off;
	unsigned char intr_src;
	unsigned short addr;
	unsigned char zero = 0x00;
	bool is_f01found = false;
	bool is_f34found = false;
	struct synaptics_rmi4_fn_desc rmi_fd;
	struct synaptics_rmi4_data *rmi4_data;

	_CHECK_POINTER(g_fwu);
	_CHECK_POINTER(g_fwu->rmi4_data);

	rmi4_data = g_fwu->rmi4_data;

	for (addr = PDT_START; addr > PDT_END; addr -= PDT_ENTRY_SIZE) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
					addr,
					(unsigned char *)&rmi_fd,
					sizeof(rmi_fd));
		if (retval < 0)
			return retval;

		if (rmi_fd.fn_number) {

            mtouch_info(MTOUCH_DEV, "%s: F%02x found (page %d)",
            			__FUNCTION__, rmi_fd.fn_number);

			switch (rmi_fd.fn_number) {
			case SYNAPTICS_RMI4_F01:
				is_f01found = true;

				if (rmi4_data->f01) {
					rmi4_data->f01->base_addr.cmd_base   = rmi_fd.cmd_base_addr;
					rmi4_data->f01->base_addr.query_base = rmi_fd.query_base_addr;
					rmi4_data->f01->base_addr.ctrl_base  = rmi_fd.ctrl_base_addr;
					rmi4_data->f01->base_addr.data_base  = rmi_fd.data_base_addr;
				}
				break;

			case SYNAPTICS_RMI4_F34:
				is_f34found = true;

				if (rmi4_data->f34) {
					rmi4_data->f34->base_addr.query_base = rmi_fd.query_base_addr;
					rmi4_data->f34->base_addr.ctrl_base  = rmi_fd.ctrl_base_addr;
					rmi4_data->f34->base_addr.data_base  = rmi_fd.data_base_addr;
				}

				if (F34_V0 == rmi_fd.fn_version)
					g_fwu->bl_version = BL_V5;
				else if (F34_V2 == rmi_fd.fn_version)
					g_fwu->bl_version = BL_V7;
				else if (F34_V1 == rmi_fd.fn_version)
					g_fwu->bl_version = BL_V6;
				else {
		            mtouch_info(MTOUCH_DEV, "%s: unrecognized F34 version",
		            			__FUNCTION__);
		            return -EINVAL;
				}

				g_fwu->intr_mask = 0;
				intr_src = rmi_fd.intr_src_count;
				intr_off = intr_count % 8;
				for (ii = intr_off; ii < (intr_src + intr_off); ii++) {
					g_fwu->intr_mask |= 1 << ii;
				}
				break;
			}
		}
		else {
			break;
		}

		intr_count += rmi_fd.intr_src_count;
	}

    if (!is_f01found || !is_f34found) {
		mtouch_error(MTOUCH_DEV, "%s: failed to find f01 and f34", __FUNCTION__);
        return -ENODEV;
    }

	addr = rmi4_data->f01->base_addr.ctrl_base + 1;
	retval = synaptics_rmi4_reg_write(rmi4_data,
				addr,
				&zero, 1);
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: failed to disable interrupt enable bit",
					__FUNCTION__);
		return retval;
	}

	return EOK;
}

/*
 * to enter the bootloader mode
 *
 * return EOK: complete the process
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_enter_flash_prog(void)
{
	int retval;
	struct synaptics_rmi4_f01_device_control f01_device_control;
	struct synaptics_rmi4_data *rmi4_data;

	_CHECK_POINTER(g_fwu);
	_CHECK_POINTER(g_fwu->rmi4_data);

	rmi4_data = g_fwu->rmi4_data;

	retval = synaptics_rmi4_fwu_read_flash_status();
	if (retval < 0)
		return retval;

	if (g_fwu->in_bl_mode)
		return EOK;

	// to disable the interrupt
	retval = synaptics_rmi4_int_enable(rmi4_data, false);

	retval = synaptics_rmi4_fwu_write_f34_command(CMD_ENABLE_FLASH_PROG);
	if (retval < 0)
		return retval;

	retval = synaptics_rmi4_fwu_wait_for_idle(ENABLE_WAIT_MS, false);
	if (retval < 0)
		return retval;

	if (!g_fwu->in_bl_mode) {
		mtouch_error(MTOUCH_DEV, "%s: bootloader mode no entered", __FUNCTION__);
		return -EINVAL;
	}

	retval = synaptics_rmi4_fwu_scan_pdt();
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: fail to parse pdt in bootloader mode", __FUNCTION__);
		return retval;
	}

	retval = synaptics_rmi4_fwu_read_f34_queries();
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: fail to read the f34 queries ", __FUNCTION__);
		return retval;
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
				rmi4_data->f01->base_addr.ctrl_base,
				f01_device_control.data,
				sizeof(f01_device_control.data));
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: fail to read f01 device control 0x%x",
				__FUNCTION__, rmi4_data->f01->base_addr.ctrl_base);
		return retval;
	}

	f01_device_control.nosleep = true;
	f01_device_control.sleep_mode = NORMAL_OPERATION;

	retval = synaptics_rmi4_reg_write(rmi4_data,
				rmi4_data->f01->base_addr.ctrl_base,
				f01_device_control.data,
				sizeof(f01_device_control.data));
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: fail to write f01 device control 0x%x",
				__FUNCTION__, rmi4_data->f01->base_addr.ctrl_base);
		return retval;
	}

	usleep(ENTER_FLASH_PROG_WAIT_US); // 20 ms

	return retval;
}

/*
 * to check the size of ui firmware
 *
 * return EOK: okay
 * otherwise, size mismatch
 */
static int synaptics_rmi4_fwu_check_ui_firmware_size(void)
{
	unsigned short block_count;

	_CHECK_POINTER(g_fwu);

	block_count = g_fwu->img.ui_firmware.size / g_fwu->block_size;

	if (block_count != g_fwu->blkcount.ui_firmware) {
		mtouch_error(MTOUCH_DEV, "%s: ui firmware size mismatch", __FUNCTION__);
		return -EINVAL;
	}

	return EOK;
}

/*
 * to check the size of ui configuration
 *
 * return EOK: okay
 * otherwise, size mismatch
 */
static int synaptics_rmi4_fwu_check_ui_configuration_size(void)
{
	unsigned short block_count;

	_CHECK_POINTER(g_fwu);

	block_count = g_fwu->img.ui_config.size / g_fwu->block_size;

	if (block_count != g_fwu->blkcount.ui_config) {
		mtouch_error(MTOUCH_DEV, "%s: ui configuration size mismatch", __FUNCTION__);
		return -EINVAL;
	}

	return EOK;
}

/*
 * to check the size of display configuration
 *
 * return EOK: okay
 * otherwise, size mismatch
 */
static int synaptics_rmi4_fwu_check_dp_configuration_size(void)
{
	unsigned short block_count;

	_CHECK_POINTER(g_fwu);

	block_count = g_fwu->img.dp_config.size / g_fwu->block_size;

	if (block_count != g_fwu->blkcount.dp_config) {
		mtouch_error(MTOUCH_DEV, "%s: display configuration size mismatch", __FUNCTION__);
		return -EINVAL;
	}

	return EOK;
}

/*
 * to check the size of bootloader configuration
 *
 * return EOK: okay
 * otherwise, size mismatch
 */
static int synaptics_rmi4_fwu_check_bl_configuration_size(void)
{
	unsigned short block_count;

	_CHECK_POINTER(g_fwu);

	block_count = g_fwu->img.bl_config.size / g_fwu->block_size;

	if (block_count != g_fwu->blkcount.bl_config) {
		mtouch_error(MTOUCH_DEV, "%s: bootloader configuration size mismatch", __FUNCTION__);
		return -EINVAL;
	}

	return EOK;
}

/*
 * to check the size of guest code
 *
 * return EOK: okay
 * otherwise, size mismatch
 */
static int synaptics_rmi4_fwu_check_guest_code_size(void)
{
	unsigned short block_count;

	_CHECK_POINTER(g_fwu);

	block_count = g_fwu->img.guest_code.size / g_fwu->block_size;

	if (block_count != g_fwu->blkcount.guest_code) {
		mtouch_error(MTOUCH_DEV, "%s: guest code configuration size mismatch", __FUNCTION__);
		return -EINVAL;
	}

	return EOK;
}

/*
 * to erase the configuration area only
 *
 * return EOK: complete
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_erase_configuration(void)
{
	int retval;

	_CHECK_POINTER(g_fwu);

	switch (g_fwu->config_area) {
	case UI_CONFIG_AREA:
		retval = synaptics_rmi4_fwu_write_f34_command(CMD_ERASE_UI_CONFIG);
		if (retval < 0)
			return retval;
		break;
	case DP_CONFIG_AREA:
		retval = synaptics_rmi4_fwu_write_f34_command(CMD_ERASE_DISP_CONFIG);
		if (retval < 0)
			return retval;
		break;
	case BL_CONFIG_AREA:
		retval = synaptics_rmi4_fwu_write_f34_command(CMD_ERASE_BL_CONFIG);
		if (retval < 0)
			return retval;
		break;
	case FLASH_CONFIG_AREA:
		retval = synaptics_rmi4_fwu_write_f34_command(CMD_ERASE_FLASH_CONFIG);
		if (retval < 0)
			return retval;
		break;
	case UPP_AREA:
		retval = synaptics_rmi4_fwu_write_f34_command(CMD_ERASE_UTILITY_PARAMETER);
		if (retval < 0)
			return retval;
		break;
	default:
		mtouch_error(MTOUCH_DEV, "%s: invalid config area", __FUNCTION__);
		return -EINVAL;
	}

	retval = synaptics_rmi4_fwu_wait_for_idle(ERASE_WAIT_MS, false);
	if (retval < 0)
		return retval;

	mtouch_info(MTOUCH_DEV, "%s: finish the F34 configuration erasement", __FUNCTION__);

	return retval;
}

/*
 * to erase the guest code area
 *
 * return EOK: complete
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_erase_guest_code(void)
{
	int retval;

	_CHECK_POINTER(g_fwu);

	retval = synaptics_rmi4_fwu_write_f34_command(CMD_ERASE_GUEST_CODE);
	if (retval < 0)
		return retval;

	mtouch_info(MTOUCH_DEV, "%s: complete the F34_ERASE_GUEST_CODE command",
				__FUNCTION__);

	retval = synaptics_rmi4_fwu_wait_for_idle(ERASE_WAIT_MS, false);
	if (retval < 0)
		return retval;

	mtouch_info(MTOUCH_DEV, "%s: idle status detected", __FUNCTION__);

	return EOK;
}

/*
 * to erase entire flash memory
 *
 * return EOK: complete
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_erase_all(void)
{
	int retval;

	_CHECK_POINTER(g_fwu);

	if (g_fwu->bl_version == BL_V7) {
		retval = synaptics_rmi4_fwu_write_f34_command(CMD_ERASE_UI_FIRMWARE);
		if (retval < 0)
			return retval;

		mtouch_info(MTOUCH_DEV, "%s: complete the F34_ERASE_UI_FW command", __FUNCTION__);

		retval = synaptics_rmi4_fwu_wait_for_idle(ERASE_WAIT_MS, false);
		if (retval < 0)
			return retval;

		mtouch_info(MTOUCH_DEV, "%s: idle status detected", __FUNCTION__);

		g_fwu->config_area = UI_CONFIG_AREA;
		retval = synaptics_rmi4_fwu_erase_configuration();
		if (retval < 0)
			return retval;
	}
	else {
		retval = synaptics_rmi4_fwu_write_f34_command(CMD_ERASE_ALL);
		if (retval < 0)
			return retval;

		mtouch_info(MTOUCH_DEV, "%s: complete the F34_ERASE_ALL command", __FUNCTION__);

		retval = synaptics_rmi4_fwu_wait_for_idle(ERASE_WAIT_MS, false);
		if (!(g_fwu->bl_version == BL_V8 &&
				g_fwu->flash_status == BAD_PARTITION_TABLE)) {
			if (retval < 0)
				return retval;
		}

		mtouch_info(MTOUCH_DEV, "%s: idle status detected", __FUNCTION__);

		if (g_fwu->bl_version == BL_V8)
			return EOK;
	}

	if (g_fwu->flash_properties.has_disp_config) {
		g_fwu->config_area = DP_CONFIG_AREA;
		retval = synaptics_rmi4_fwu_erase_configuration();
		if (retval < 0)
			return retval;
	}

	if (g_fwu->has_guest_code) {
		retval = synaptics_rmi4_fwu_erase_guest_code();
		if (retval < 0)
			return retval;
	}

	return EOK;
}


/*
 * to write the data into ui firmware area
 *
 * return EOK: complete
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_write_firmware(void)
{
	unsigned short firmware_block_count;

	firmware_block_count = g_fwu->img.ui_firmware.size / g_fwu->block_size;

	return synaptics_rmi4_fwu_write_f34_blocks((unsigned char *)g_fwu->img.ui_firmware.data,
											firmware_block_count,
											CMD_WRITE_FW);
}

/*
 * call fwu_write_f34_blocks to write configuration data
 *
 * return EOK: complete
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_write_configuration(void)
{
	return synaptics_rmi4_fwu_write_f34_blocks((unsigned char *)g_fwu->config_data,
											g_fwu->config_block_count,
											CMD_WRITE_CONFIG);
}

/*
 * helper functions to write the ui configuration
 *
 * return EOK: complete
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_write_ui_configuration(void)
{
	_CHECK_POINTER(g_fwu);

	g_fwu->config_area = UI_CONFIG_AREA;
	g_fwu->config_data = g_fwu->img.ui_config.data;
	g_fwu->config_size = g_fwu->img.ui_config.size;
	g_fwu->config_block_count = g_fwu->config_size / g_fwu->block_size;

	return synaptics_rmi4_fwu_write_configuration();
}

/*
 * helper functions to write the display configuration
 *
 * return EOK: complete
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_write_dp_configuration(void)
{
	_CHECK_POINTER(g_fwu);

	g_fwu->config_area = DP_CONFIG_AREA;
	g_fwu->config_data = g_fwu->img.dp_config.data;
	g_fwu->config_size = g_fwu->img.dp_config.size;
	g_fwu->config_block_count = g_fwu->config_size / g_fwu->block_size;

	return synaptics_rmi4_fwu_write_configuration();
}

/*
 * helper functions to write flash configuration
 *
 * return EOK: complete
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_write_flash_configuration(void)
{
	int retval;

	_CHECK_POINTER(g_fwu);

	g_fwu->config_area = FLASH_CONFIG_AREA;
	g_fwu->config_data = g_fwu->img.fl_config.data;
	g_fwu->config_size = g_fwu->img.fl_config.size;
	g_fwu->config_block_count = g_fwu->config_size / g_fwu->block_size;

	if (g_fwu->config_block_count != g_fwu->blkcount.fl_config) {
		mtouch_error(MTOUCH_DEV, "%s: flash configuration size mismatch", __FUNCTION__);
		return -EINVAL;
	}

	retval = synaptics_rmi4_fwu_erase_configuration();
	if (retval < 0)
		return retval;

	retval = synaptics_rmi4_fwu_write_configuration();
	if (retval < 0)
		return retval;

	synaptics_rmi4_sw_reset(g_fwu->rmi4_data);

	retval = synaptics_rmi4_fwu_wait_for_idle(ENABLE_WAIT_MS, false);
	if (retval < 0)
		return retval;

	return EOK;
}

/*
 * helper functions to write the flash config for bootloader 8
 *
 * return EOK: complete
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_write_partition_table_v8(void)
{
	int retval;

	_CHECK_POINTER(g_fwu);

	g_fwu->config_area = FLASH_CONFIG_AREA;
	g_fwu->config_data = g_fwu->img.fl_config.data;
	g_fwu->config_size = g_fwu->img.fl_config.size;
	g_fwu->config_block_count = g_fwu->config_size / g_fwu->block_size;

	if (g_fwu->config_block_count != g_fwu->blkcount.fl_config) {
		mtouch_error(MTOUCH_DEV, "%s: flash configuration size mismatch", __FUNCTION__);
		return -EINVAL;
	}

	retval = synaptics_rmi4_fwu_write_configuration();
	if (retval < 0)
		return retval;

	retval = synaptics_rmi4_fwu_wait_for_idle(ENABLE_WAIT_MS, false);
	if (retval < 0)
		return retval;

	return EOK;
}

/*
 * helper functions to write the flash config for bootloader 8
 *
 * return EOK: complete
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_write_partition_table_v7(void)
{
	int retval;
	unsigned short block_count;

	_CHECK_POINTER(g_fwu);

	block_count = g_fwu->blkcount.bl_config;
	g_fwu->config_area = BL_CONFIG_AREA;
	g_fwu->config_size = g_fwu->block_size * block_count;

	retval = synaptics_rmi4_fwu_allocate_read_config_buf(g_fwu->config_size);
	if (retval < 0)
		return retval;

	retval = synaptics_rmi4_fwu_read_f34_blocks(block_count, CMD_READ_CONFIG);
	if (retval < 0)
		return retval;

	retval = synaptics_rmi4_fwu_erase_configuration();
	if (retval < 0)
		return retval;

	retval = synaptics_rmi4_fwu_write_flash_configuration();
	if (retval < 0)
		return retval;

	g_fwu->config_area = BL_CONFIG_AREA;
	g_fwu->config_data = g_fwu->read_config_buf;
	g_fwu->config_size = g_fwu->img.bl_config.size;
	g_fwu->config_block_count = g_fwu->config_size / g_fwu->block_size;

	retval = synaptics_rmi4_fwu_write_configuration();
	if (retval < 0)
		return retval;

	return EOK;
}

/*
 * helper functions to write the guest code area
 *
 * return EOK: complete
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_write_guest_code(void)
{
	int retval;
	unsigned short guest_code_block_count;

	_CHECK_POINTER(g_fwu);

	guest_code_block_count = g_fwu->img.guest_code.size / g_fwu->block_size;

	retval = synaptics_rmi4_fwu_write_f34_blocks((unsigned char *)g_fwu->img.guest_code.data,
											guest_code_block_count,
											CMD_WRITE_GUEST_CODE);
	if (retval < 0)
		return retval;

	return EOK;
}
/*
 * to erase and re-program the flash memory
 *
 * return EOK: complete the process
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_do_reflash(void)
{
	int retval;

	_CHECK_POINTER(g_fwu);

	if (!g_fwu->new_partition_table) {
		retval = synaptics_rmi4_fwu_check_ui_firmware_size();
		if (retval < 0)
			return retval;

		retval = synaptics_rmi4_fwu_check_ui_configuration_size();
		if (retval < 0)
			return retval;

		if (g_fwu->flash_properties.has_disp_config && g_fwu->img.contains_disp_config) {
			retval = synaptics_rmi4_fwu_check_dp_configuration_size();
			if (retval < 0)
				return retval;
		}

		if (g_fwu->has_guest_code && g_fwu->img.contains_guest_code) {
			retval = synaptics_rmi4_fwu_check_guest_code_size();
			if (retval < 0)
				return retval;
		}
	} else if (g_fwu->bl_version == BL_V7) {
		retval = synaptics_rmi4_fwu_check_bl_configuration_size();
		if (retval < 0)
			return retval;
	}

	retval = synaptics_rmi4_fwu_erase_all();
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: fail to do erase command", __FUNCTION__);
		return retval;
	}
	mtouch_info(MTOUCH_DEV, "%s: erase completed", __FUNCTION__);

	if (g_fwu->bl_version == BL_V7 && g_fwu->new_partition_table) {
		retval = synaptics_rmi4_fwu_write_partition_table_v7();
		if (retval < 0)
			return retval;
		mtouch_info(MTOUCH_DEV, "%s: partition table programmed", __FUNCTION__);
	} else if (g_fwu->bl_version == BL_V8) {
		retval = synaptics_rmi4_fwu_write_partition_table_v8();
		if (retval < 0)
			return retval;
		mtouch_info(MTOUCH_DEV, "%s: partition table programmed", __FUNCTION__);
	}

	retval = synaptics_rmi4_fwu_write_firmware();
	if (retval < 0)
		return retval;
	mtouch_info(MTOUCH_DEV, "%s: firmware programmed", __FUNCTION__);

	g_fwu->config_area = UI_CONFIG_AREA;
	if (g_fwu->flash_properties.has_disp_config && g_fwu->img.contains_disp_config) {
		retval = synaptics_rmi4_fwu_write_dp_configuration();
		if (retval < 0)
			return retval;
		mtouch_info(MTOUCH_DEV, "%s: display configuration programmed", __FUNCTION__);
	}

	retval = synaptics_rmi4_fwu_write_ui_configuration();
	if (retval < 0)
		return retval;
	mtouch_info(MTOUCH_DEV, "%s: configuration programmed", __FUNCTION__);

	if (g_fwu->has_guest_code && g_fwu->img.contains_guest_code) {
		retval = synaptics_rmi4_fwu_write_guest_code();
		if (retval < 0)
			return retval;
		mtouch_info(MTOUCH_DEV, "%s: guest code programmed", __FUNCTION__);
	}

	return retval;
}

/*
 * to perform the firmware update
 *
 * the procedure will tend to load the specified image file
 * based on the option "fw_img" defined in graphics.conf
 *
 * then, upgrade the device firmware by re-programming the flash memory
 *
 * return EOK: complete the process
 * otherwise, fail
 */
static int synaptics_rmi4_fwu_start_reflash(const char *path_fw_image,
											const unsigned int image_fw_id)
{
	int retval;
	enum flash_area flash_area;
	struct synaptics_rmi4_data *rmi4_data;
	int numBytesRead;
	FILE *fp;

	_CHECK_POINTER(g_fwu);
	_CHECK_POINTER(g_fwu->rmi4_data);

	rmi4_data = g_fwu->rmi4_data;

	sprintf(g_fwu->image_name, "%s", path_fw_image);

	mtouch_info(MTOUCH_DEV, "%s: start of reflash process",
				__FUNCTION__);

	// open the target image file and store in g_fwu->image
	if (NULL == g_fwu->image) {
		mtouch_info(MTOUCH_DEV, "%s: fw image file: %s", __FUNCTION__, g_fwu->image_name);

		if (strlen(g_fwu->image_name) <= 0) {
			mtouch_error(MTOUCH_DEV, "%s: invalid path of image file", __FUNCTION__);
			retval = -EINVAL;
			goto exit;
		}

		fp = fopen(g_fwu->image_name, "r");
		if (!fp) {
			mtouch_error(MTOUCH_DEV, "%s: image file %s not found", __FUNCTION__, g_fwu->image_name);
			retval = -EINVAL;
			goto exit;
		}

		fseek(fp, 0L, SEEK_END);
		g_fwu->image_file_size = ftell(fp);
		if (g_fwu->image_file_size == -1) {
			mtouch_error(MTOUCH_DEV, "%s: fail to determine size of %s", __FUNCTION__, g_fwu->image_name);
			retval = -EIO;
			fclose(fp);
			goto exit;
		}

		fseek(fp, 0L, SEEK_SET);
		g_fwu->image = calloc(g_fwu->image_file_size + 1, sizeof(unsigned char));
		if (!g_fwu->image) {
			mtouch_error(MTOUCH_DEV, "%s: failed to allocate memory for g_fwu->image (size = %d)",
						__FUNCTION__, g_fwu->image_file_size + 1);
			retval = -ENOMEM;
			fclose(fp);
			goto exit;
		}
		else {
			numBytesRead = fread(g_fwu->image, sizeof(unsigned char), g_fwu->image_file_size, fp);
			if (numBytesRead != g_fwu->image_file_size) {
				mtouch_error(MTOUCH_DEV, "%s: failed to read entire content of image file (bytes_read = %d)(size = %d)",
							__FUNCTION__, numBytesRead, g_fwu->image_file_size);
				retval = -EIO;
				fclose(fp);
				goto exit;
			}
		}

		fclose(fp);
	}

	pthread_mutex_lock(&rmi4_data->rmi4_fwu_mutex);

	// parse the target image file
	retval = synaptics_rmi4_fwu_parse_image_info();
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: fail to parse the image file contents", __FUNCTION__);
		goto exit;
	}

	if (g_fwu->blkcount.total_count != g_fwu->img.blkcount.total_count) {
		mtouch_error(MTOUCH_DEV, "%s: flash size mismatch", __FUNCTION__);
		goto exit;
	}

	if (g_fwu->bl_version != g_fwu->img.bl_version) {
		mtouch_error(MTOUCH_DEV, "%s: bootloader version mismatch", __FUNCTION__);
		goto exit;
	}

	// check the flash status
	retval = synaptics_rmi4_fwu_read_flash_status();
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: fail to read flash status", __FUNCTION__);
		goto exit;
	}

	if (g_fwu->in_bl_mode) {
		g_fwu->bl_mode_device = true;
		mtouch_info(MTOUCH_DEV, "%s: device in bootloader mode", __FUNCTION__);
	}
	else {
		g_fwu->bl_mode_device = false;
	}

	// determine the flash area
	// if the flash area != NONE, enter the bootloader mode
	flash_area = synaptics_rmi4_fwu_go_nogo(image_fw_id);

	if (flash_area != NONE) {
		retval = synaptics_rmi4_fwu_enter_flash_prog();
		if (retval < 0) {
			synaptics_rmi4_sw_reset(rmi4_data);
			goto exit;
		}
	}

	// program the flash memory
	switch (flash_area) {

	case UI_FIRMWARE:
		retval = synaptics_rmi4_fwu_do_reflash();
		if (retval < 0) {
			mtouch_error(MTOUCH_DEV, "%s: fail to do reflash", __FUNCTION__);
			goto exit;
		}

		g_fwu->updated = true;
		break;

	case UI_CONFIG:
		if (g_fwu->blkcount.ui_config != (g_fwu->img.ui_config.size/g_fwu->block_size)) {
			mtouch_error(MTOUCH_DEV, "%s: ui configuration size mismatch", __FUNCTION__);
			retval = -EINVAL;
			goto exit;
		}
		g_fwu->config_area = UI_CONFIG_AREA;
		retval = synaptics_rmi4_fwu_erase_configuration();
		if (retval < 0) {
			mtouch_error(MTOUCH_DEV, "%s: fail to erase config area", __FUNCTION__);
			goto exit;
		}

		retval = synaptics_rmi4_fwu_write_ui_configuration();
		if (retval < 0)
			return retval;
		mtouch_info(MTOUCH_DEV, "%s: configuration programmed", __FUNCTION__);

		g_fwu->updated = true;
		break;

	case NONE:
		g_fwu->updated = false;
		break;

	default:
		mtouch_error(MTOUCH_DEV, "%s: unknown flash area", __FUNCTION__);
		break;
	}


exit:
	mtouch_info(MTOUCH_DEV, "%s: end of reflash process", __FUNCTION__);

	pthread_mutex_unlock(&rmi4_data->rmi4_fwu_mutex);

	return retval;
}

/*
 * the entry point of firmware update
 * call synaptics_rmi4_fwu_start_reflash() to perform the firmware upgrade
 *
 * return EOK: complete
 * otherwise, fail
 */
int synaptics_rmi4_fwu_updater(const char *path_fw_image,
							const unsigned int image_fw_id)
{
	int retval;
	static unsigned char do_once = 1;

	if (!do_once)
		return EOK;

	_CHECK_POINTER(g_fwu);

	mtouch_info(MTOUCH_DEV, "%s: fw_updating is enabled",
				__FUNCTION__);

	if (!g_fwu->initialized)
		return -ENODEV;

	g_fwu->image = NULL;

	retval = synaptics_rmi4_fwu_start_reflash(path_fw_image, image_fw_id);

	if (g_fwu->updated) {
		// re-build the RMI4 device instance
		retval = synaptics_rmi4_reinit();
		if (retval < 0) {
			mtouch_error(MTOUCH_DEV, "%s: fail to do reinit the device", __FUNCTION__);
		}
	}

	do_once = 0;

	if (g_fwu->image) {
		free(g_fwu->image);
		g_fwu->image = NULL;
	}

	return retval;
}

/*
 * Function $34 initialization
 * f34 implements the function related to flash memory
 * flash programming can be used to upgrade the device firmware
 *
 * struct synaptics_rmi4_data *rmi4_data: RMI4 device instance data
 * const unsigned char fn_version: use the fn_version to determine the version of bootloader
 *
 * return EOK: complete the initialization
 * otherwise, fail
 */
int synaptics_rmi4_fwu_init(struct synaptics_rmi4_data *rmi4_data,
                            const unsigned char fn_version)
{
	int retval;

	if (g_fwu) {
		mtouch_info(MTOUCH_DEV, "%s: handle already exists", __FUNCTION__);
		return EOK;
	}

	_CHECK_POINTER(rmi4_data);
	_CHECK_POINTER(rmi4_data->f34);

	g_fwu = calloc(1, sizeof(struct synaptics_rmi4_fwu_handle));
	if (!g_fwu) {
		mtouch_error(MTOUCH_DEV, "%s: failed to allocate memory for fwu", __FUNCTION__);
		retval = -ENOMEM;
        goto exit;
	}

	g_fwu->image_name = calloc(MAX_IMAGE_NAME_LEN, sizeof(unsigned char));
	if (!g_fwu->image_name) {
		mtouch_error(MTOUCH_DEV, "%s: failed to allocate memory for g_fwu->image_name", __FUNCTION__);
		retval = -ENOMEM;
		goto exit_free_fwu;
	}

	g_fwu->rmi4_data = rmi4_data;
	g_fwu->f34 = rmi4_data->f34;
	g_fwu->initialized = false;
	g_fwu->updated = false;

	if (F34_V0 == fn_version)
		g_fwu->bl_version = BL_V5;
	else if (F34_V1 == fn_version)
		g_fwu->bl_version = BL_V6;
	else if (F34_V2 == fn_version)
		g_fwu->bl_version = BL_V7;

	retval = synaptics_rmi4_fwu_read_f34_queries();
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: fail to read the f34 queries ", __FUNCTION__);
		goto exit_free_mem;
	}

	mtouch_info(MTOUCH_DEV, "%s: bootloader version %d ", __FUNCTION__, g_fwu->bl_version);

	retval = synaptics_rmi4_fwu_get_device_config_id();
	if (retval < 0) {
		mtouch_error(MTOUCH_DEV, "%s: fail to read device config ID ", __FUNCTION__);
		goto exit_free_mem;
	}

	g_fwu->force_update = FORCE_UPDATE;
	g_fwu->do_lockdown = DO_LOCKDOWN;
	g_fwu->initialized = true;

	return EOK;

exit_free_mem:
	free(g_fwu->image_name);

exit_free_fwu:
	free(g_fwu);
	g_fwu = NULL;

exit:
	return retval;
}

/*
 * release the allocated resource
 *
 * return void
 */
void synaptics_rmi4_fwu_deinit(void)
{
	if (g_fwu->image_name) {
		free(g_fwu->image_name);
		g_fwu->image_name = NULL;
	}
	if (g_fwu->read_config_buf) {
		free(g_fwu->read_config_buf);
		g_fwu->read_config_buf = NULL;
	}
	if (g_fwu) {
		free(g_fwu);
		g_fwu = NULL;
	}
}

