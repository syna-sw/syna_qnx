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


#ifndef _SYNAPTICS_RMI_CORE_H_
#define _SYNAPTICS_RMI_CORE_H_

#define PDT_PROPS				(0X00EF)
#define PDT_START				(0x00E9)
#define PDT_END					(0x00D0)
#define PDT_ENTRY_SIZE			(0x0006)
#define PAGES_TO_SERVICE		(10)
#define PAGE_SELECT_LEN 		(2)
#define ADDRESS_LEN 			(2)

#define SYNAPTICS_RMI4_F01 		(0x01)
#define SYNAPTICS_RMI4_F11 		(0x11)
#define SYNAPTICS_RMI4_F12 		(0x12)
#define SYNAPTICS_RMI4_F1A 		(0x1A)
#define SYNAPTICS_RMI4_F34 		(0x34)
#define SYNAPTICS_RMI4_F35 		(0x35)
#define SYNAPTICS_RMI4_F38 		(0x38)
#define SYNAPTICS_RMI4_F51 		(0x51)
#define SYNAPTICS_RMI4_F54 		(0x54)
#define SYNAPTICS_RMI4_F55 		(0x55)
#define SYNAPTICS_RMI4_FDB 		(0xDB)
#define MAX_RMI4_FUNCS			(20)

#define MAX_INTR_REGISTERS		(4)

#define MASK_16BIT				(0xFFFF)
#define MASK_8BIT				(0xFF)
#define MASK_7BIT				(0x7F)
#define MASK_6BIT				(0x3F)
#define MASK_5BIT 				(0x1F)
#define MASK_4BIT 				(0x0F)
#define MASK_3BIT 				(0x07)
#define MASK_2BIT 				(0x03)
#define MASK_1BIT 				(0x01)

#define SYNA_I2C_RETRY_TIMES	(10)

#define F01_STD_QUERY_LEN		(21)
#define F01_BUID_ID_OFFSET 		(18)

#define PRODUCT_INFO_SIZE		(2)
#define PRODUCT_ID_SIZE 		(10)
#define BUILD_ID_SIZE 			(3)

#define F12_FINGERS_TO_SUPPORT	(10)
#define F12_NO_OBJECT_STATUS	(0x00)
#define F12_FINGER_STATUS		(0x01)
#define F12_ACTIVE_STYLUS_STATUS (0x02)
#define F12_PALM_STATUS 		(0x03)
#define F12_HOVERING_FINGER_STATUS (0x05)
#define F12_GLOVED_FINGER_STATUS (0x06)
#define F12_NARROW_OBJECT_STATUS (0x07)
#define F12_HAND_EDGE_STATUS 	(0x08)
#define F12_COVER_STATUS 		(0x0A)
#define F12_STYLUS_STATUS 		(0x0B)
#define F12_ERASER_STATUS 		(0x0C)
#define F12_SMALL_OBJECT_STATUS (0x0D)

#define MAX_F11_TOUCH_WIDTH 	(15)
#define MAX_F12_TOUCH_WIDTH 	(255)

#define RPT_TYPE 				(1 << 0)
#define RPT_X_LSB 				(1 << 1)
#define RPT_X_MSB 				(1 << 2)
#define RPT_Y_LSB 				(1 << 3)
#define RPT_Y_MSB 				(1 << 4)
#define RPT_Z 					(1 << 5)
#define RPT_WX 					(1 << 6)
#define RPT_WY 					(1 << 7)
#define RPT_DEFAULT 			(RPT_TYPE | RPT_X_LSB | RPT_X_MSB | RPT_Y_LSB | RPT_Y_MSB)

#define STATUS_NO_ERROR			(0x00)
#define STATUS_RESET_OCCURRED 	(0x01)
#define STATUS_INVALID_CONFIG 	(0x02)
#define STATUS_DEVICE_FAILURE 	(0x03)
#define STATUS_CONFIG_CRC_FAILURE (0x04)
#define STATUS_FIRMWARE_CRC_FAILURE (0x05)
#define STATUS_CRC_IN_PROGRESS (0x06)

#define NORMAL_OPERATION		(0 << 0)
#define SENSOR_SLEEP			(1 << 0)
#define NO_SLEEP_OFF			(0 << 2)
#define NO_SLEEP_ON				(1 << 2)
#define CONFIGURED				(1 << 7)

#define INTERRUPT_STATUS_FLASH	(1 << 0)
#define INTERRUPT_STATUS_DEVICE	(1 << 1)
#define INTERRUPT_STATUS_TOUCH	(1 << 2)
#define INTERRUPT_STATUS_ANALOG	(1 << 3)
#define INTERRUPT_STATUS_BUTTON	(1 << 4)
#define INTERRUPT_STATUS_SENSOR	(1 << 5)

#define FINGER_LANDING			(1)
#define FINGER_LIFTING			(0)

/*
 * struct synaptics_rmi4_fn_desc - function descriptor fields in PDT entry
 *
 * query_base_addr: base address for query registers
 * cmd_base_addr: base address for command registers
 * ctrl_base_addr: base address for control registers
 * data_base_addr: base address for data registers
 * intr_src_count: number of interrupt sources
 * fn_version: version of function
 * fn_number: function number
 */
struct synaptics_rmi4_fn_desc {
	union {
		struct {
			unsigned char query_base_addr;
			unsigned char cmd_base_addr;
			unsigned char ctrl_base_addr;
			unsigned char data_base_addr;
			unsigned char intr_src_count:3;
			unsigned char reserved_1:2;
			unsigned char fn_version:2;
			unsigned char reserved_2:1;
			unsigned char fn_number;
		} __attribute__((packed));;
		unsigned char data[6];
	};
};

/*
 * struct synaptics_rmi4_fn_full_addr - base address definition
 *
 * query_base: base address for query registers
 * cmd_base: base address for command registers
 * ctrl_base: base address for control registers
 * data_base: base address for data registers
 */
struct synaptics_rmi4_fn_full_addr {
	unsigned short query_base;
	unsigned short cmd_base;
	unsigned short ctrl_base;
	unsigned short data_base;
};

/*
 * struct synaptics_rmi4_fn - function handler
 *
 * func_num: function name
 * intr_mask: interrupt mask
 * base_addr: contains the base address
 * attn_handle: function pointer to interrupt handler
 * data_size: size of private data
 * data: pointer to private data
 * extra: pointer to extra data
 */
struct synaptics_rmi4_fn {
	unsigned char func_num;
	unsigned char intr_mask;
	struct synaptics_rmi4_fn_full_addr base_addr;
	int (*attn_handle)(void *data);
	int data_size;
	void *data;
	void *extra;
};

/*
 * struct synaptics_rmi4_device_info - device information
 *
 * version_major: RMI protocol major version number
 * version_minor: RMI protocol minor version number
 * manufacturer_id: manufacturer ID
 * product_props: product properties
 * product_info: product information
 * product_id_string: product ID
 * build_id: firmware build ID
 */
struct synaptics_rmi4_device_info {
	unsigned int version_major;
	unsigned int version_minor;
	unsigned char manufacturer_id;
	unsigned char product_props;
	unsigned char product_info[PRODUCT_INFO_SIZE];
	unsigned char product_id_string[PRODUCT_ID_SIZE + 1];
	unsigned char build_id[BUILD_ID_SIZE];
};


/*
 * struct synaptics_rmi4_data - RMI4 device instance data
 *
 *  rmi4_mod_info: device information
 *  num_of_intr_regs: number of interrupt registers
 *  intr_mask: interrupt enable mask
 *  num_of_fingers: maximum number of fingers for 2D touch
 *  report_enable: input data to report for F$12
 *  sensor_max_x: maximum x coordinate for 2D touch
 *  sensor_max_y: maximum y coordinate for 2D touch
 *  max_touch_width: maximum touch width
 *  valid_button_count: number of valid 0D buttons
 *  firmware_id: firmware build id
 *
 *  f01: function handler of f$01
 *  f11: function handler of f$11
 *  f12: function handler of f$12
 *  f34: function handler of f$34
 *  f1a: function handler of f$1a
 *  f54: function handler of f$54
 *  f55: function handler of f$55
 *
 *  current_page: current RMI page for register access
 *  rmi4_io_ctrl_mutex: mutex for the RMI io control
 *  rmi4_report_mutex: mutex for getting the touch report
 *  rmi4_fwu_mutex: mutex to protect the fw update
 */
struct synaptics_rmi4_data {

	// device information
    struct synaptics_rmi4_device_info rmi4_mod_info;
    unsigned short num_of_intr_regs;
    unsigned char intr_mask;
	unsigned char num_of_fingers;
	unsigned char report_enable;
	int sensor_max_x;
	int sensor_max_y;
	unsigned char max_touch_width;
	unsigned char valid_button_count;
	unsigned int firmware_id;

    // RMI Function Description
	struct synaptics_rmi4_fn *f01;
	struct synaptics_rmi4_fn *f11;
	struct synaptics_rmi4_fn *f12;
	struct synaptics_rmi4_fn *f34;
	struct synaptics_rmi4_fn *f1a;
	struct synaptics_rmi4_fn *f54;
	struct synaptics_rmi4_fn *f55;

	// RMI helper
    unsigned char current_page;
	pthread_mutex_t rmi4_io_ctrl_mutex;
	pthread_mutex_t rmi4_report_mutex;
	pthread_mutex_t rmi4_fwu_mutex;

};

/*
 * struct synaptics_rmi4_f12_extra_data - extra data of F$12
 * data1_offset: offset to F12_2D_DATA01 register
 * data4_offset: offset to F12_2D_DATA04 register
 * data15_offset: offset to F12_2D_DATA15 register
 * data15_size: size of F12_2D_DATA15 register
 * data15_data: buffer for reading F12_2D_DATA15 register
 * data29_offset: offset to F12_2D_DATA29 register
 * data29_size: size of F12_2D_DATA29 register
 * data29_data: buffer for reading F12_2D_DATA29 register
 * ctrl20_offset: offset to F12_2D_CTRL20 register
 */
struct synaptics_rmi4_f12_extra_data {
	unsigned char data1_offset;
	unsigned char data4_offset;
	unsigned char data15_offset;
	unsigned char data15_size;
	unsigned char data15_data[(F12_FINGERS_TO_SUPPORT + 7) / 8];
	unsigned char data29_offset;
	unsigned char data29_size;
	unsigned char data29_data[F12_FINGERS_TO_SUPPORT * 2];
	unsigned char ctrl20_offset;
};


/*
 *
 * RMI4 Register
 *
 */

/*
 * Function $01 RMI register
 */
struct synaptics_rmi4_f01_device_status {
	union {
		struct {
			unsigned char status_code:4;
			unsigned char reserved:2;
			unsigned char flash_prog:1;
			unsigned char unconfigured:1;
		} __attribute__((packed));;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_f01_device_control {
	union {
		struct {
			unsigned char sleep_mode:2;
			unsigned char nosleep:1;
			unsigned char reserved:2;
			unsigned char charger_connected:1;
			unsigned char report_rate:1;
			unsigned char configured:1;
		}  __attribute__((packed));;
		unsigned char data[1];
	};
};


/*
 * Function $11 RMI register
 */
struct synaptics_rmi4_f11_query_0_5 {
	union {
		struct {
			/* query 0 */
			unsigned char f11_query0_b0__2:3;
			unsigned char has_query_9:1;
			unsigned char has_query_11:1;
			unsigned char has_query_12:1;
			unsigned char has_query_27:1;
			unsigned char has_query_28:1;

			/* query 1 */
			unsigned char num_of_fingers:3;
			unsigned char has_rel:1;
			unsigned char has_abs:1;
			unsigned char has_gestures:1;
			unsigned char has_sensitibity_adjust:1;
			unsigned char f11_query1_b7:1;

			/* query 2 */
			unsigned char num_of_x_electrodes;

			/* query 3 */
			unsigned char num_of_y_electrodes;

			/* query 4 */
			unsigned char max_electrodes:7;
			unsigned char f11_query4_b7:1;

			/* query 5 */
			unsigned char abs_data_size:2;
			unsigned char has_anchored_finger:1;
			unsigned char has_adj_hyst:1;
			unsigned char has_dribble:1;
			unsigned char has_bending_correction:1;
			unsigned char has_large_object_suppression:1;
			unsigned char has_jitter_filter:1;
		} __attribute__((packed));;
		unsigned char data[6];
	};
};


struct synaptics_rmi4_f11_query_7_8 {
	union {
		struct {
			/* query 7 */
			unsigned char has_single_tap:1;
			unsigned char has_tap_and_hold:1;
			unsigned char has_double_tap:1;
			unsigned char has_early_tap:1;
			unsigned char has_flick:1;
			unsigned char has_press:1;
			unsigned char has_pinch:1;
			unsigned char has_chiral_scroll:1;

			/* query 8 */
			unsigned char has_palm_detect:1;
			unsigned char has_rotate:1;
			unsigned char has_touch_shapes:1;
			unsigned char has_scroll_zones:1;
			unsigned char individual_scroll_zones:1;
			unsigned char has_multi_finger_scroll:1;
			unsigned char has_multi_finger_scroll_edge_motion:1;
			unsigned char has_multi_finger_scroll_inertia:1;
		} __attribute__((packed));;
		unsigned char data[2];
	};
};

struct synaptics_rmi4_f11_query_9 {
	union {
		struct {
			unsigned char has_pen:1;
			unsigned char has_proximity:1;
			unsigned char has_large_object_sensitivity:1;
			unsigned char has_suppress_on_large_object_detect:1;
			unsigned char has_two_pen_thresholds:1;
			unsigned char has_contact_geometry:1;
			unsigned char has_pen_hover_discrimination:1;
			unsigned char has_pen_hover_and_edge_filters:1;
		} __attribute__((packed));;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_f11_query_12 {
	union {
		struct {
			unsigned char has_small_object_detection:1;
			unsigned char has_small_object_detection_tuning:1;
			unsigned char has_8bit_w:1;
			unsigned char has_2d_adjustable_mapping:1;
			unsigned char has_general_information_2:1;
			unsigned char has_physical_properties:1;
			unsigned char has_finger_limit:1;
			unsigned char has_linear_cofficient_2:1;
		} __attribute__((packed));;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_f11_query_27 {
	union {
		struct {
			unsigned char f11_query27_b0:1;
			unsigned char has_pen_position_correction:1;
			unsigned char has_pen_jitter_filter_coefficient:1;
			unsigned char has_group_decomposition:1;
			unsigned char has_wakeup_gesture:1;
			unsigned char has_small_finger_correction:1;
			unsigned char has_data_37:1;
			unsigned char f11_query27_b7:1;
		} __attribute__((packed));;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_f11_ctrl_6_9 {
	union {
		struct {
			unsigned char sensor_max_x_pos_7_0;
			unsigned char sensor_max_x_pos_11_8:4;
			unsigned char f11_ctrl7_b4__7:4;
			unsigned char sensor_max_y_pos_7_0;
			unsigned char sensor_max_y_pos_11_8:4;
			unsigned char f11_ctrl9_b4__7:4;
		} __attribute__((packed));;
		unsigned char data[4];
	};
};


struct synaptics_rmi4_f11_data_1_5 {
	union {
		struct {
			unsigned char x_position_11_4;
			unsigned char y_position_11_4;
			unsigned char x_position_3_0:4;
			unsigned char y_position_3_0:4;
			unsigned char wx:4;
			unsigned char wy:4;
			unsigned char z;
		} __attribute__((packed));;
		unsigned char data[5];
	};
};


/*
 * Function $12 RMI register
 */
struct synaptics_rmi4_f12_query_5 {
	union {
		struct {
			unsigned char size_of_query6;
			struct {
				unsigned char ctrl0_is_present:1;
				unsigned char ctrl1_is_present:1;
				unsigned char ctrl2_is_present:1;
				unsigned char ctrl3_is_present:1;
				unsigned char ctrl4_is_present:1;
				unsigned char ctrl5_is_present:1;
				unsigned char ctrl6_is_present:1;
				unsigned char ctrl7_is_present:1;
			} __attribute__((packed));;
			struct {
				unsigned char ctrl8_is_present:1;
				unsigned char ctrl9_is_present:1;
				unsigned char ctrl10_is_present:1;
				unsigned char ctrl11_is_present:1;
				unsigned char ctrl12_is_present:1;
				unsigned char ctrl13_is_present:1;
				unsigned char ctrl14_is_present:1;
				unsigned char ctrl15_is_present:1;
			} __attribute__((packed));;
			struct {
				unsigned char ctrl16_is_present:1;
				unsigned char ctrl17_is_present:1;
				unsigned char ctrl18_is_present:1;
				unsigned char ctrl19_is_present:1;
				unsigned char ctrl20_is_present:1;
				unsigned char ctrl21_is_present:1;
				unsigned char ctrl22_is_present:1;
				unsigned char ctrl23_is_present:1;
			} __attribute__((packed));;
			struct {
				unsigned char ctrl24_is_present:1;
				unsigned char ctrl25_is_present:1;
				unsigned char ctrl26_is_present:1;
				unsigned char ctrl27_is_present:1;
				unsigned char ctrl28_is_present:1;
				unsigned char ctrl29_is_present:1;
				unsigned char ctrl30_is_present:1;
				unsigned char ctrl31_is_present:1;
			} __attribute__((packed));;
			struct {
				unsigned char ctrl32_is_present:1;
				unsigned char ctrl33_is_present:1;
				unsigned char ctrl34_is_present:1;
				unsigned char ctrl35_is_present:1;
				unsigned char ctrl36_is_present:1;
				unsigned char ctrl37_is_present:1;
				unsigned char ctrl38_is_present:1;
				unsigned char ctrl39_is_present:1;
			} __attribute__((packed));;
			struct {
				unsigned char ctrl40_is_present:1;
				unsigned char ctrl41_is_present:1;
				unsigned char ctrl42_is_present:1;
				unsigned char ctrl43_is_present:1;
				unsigned char ctrl44_is_present:1;
				unsigned char ctrl45_is_present:1;
				unsigned char ctrl46_is_present:1;
				unsigned char ctrl47_is_present:1;
			} __attribute__((packed));;
			struct {
				unsigned char ctrl48_is_present:1;
				unsigned char ctrl49_is_present:1;
				unsigned char ctrl50_is_present:1;
				unsigned char ctrl51_is_present:1;
				unsigned char ctrl52_is_present:1;
				unsigned char ctrl53_is_present:1;
				unsigned char ctrl54_is_present:1;
				unsigned char ctrl55_is_present:1;
			} __attribute__((packed));;
			struct {
				unsigned char ctrl56_is_present:1;
				unsigned char ctrl57_is_present:1;
				unsigned char ctrl58_is_present:1;
				unsigned char ctrl59_is_present:1;
				unsigned char ctrl60_is_present:1;
				unsigned char ctrl61_is_present:1;
				unsigned char ctrl62_is_present:1;
				unsigned char ctrl63_is_present:1;
			} __attribute__((packed));;
		};
		unsigned char data[9];
	};
};


struct synaptics_rmi4_f12_query_8 {
	union {
		struct {
			unsigned char size_of_query9;
			struct {
				unsigned char data0_is_present:1;
				unsigned char data1_is_present:1;
				unsigned char data2_is_present:1;
				unsigned char data3_is_present:1;
				unsigned char data4_is_present:1;
				unsigned char data5_is_present:1;
				unsigned char data6_is_present:1;
				unsigned char data7_is_present:1;
			} __attribute__((packed));;
			struct {
				unsigned char data8_is_present:1;
				unsigned char data9_is_present:1;
				unsigned char data10_is_present:1;
				unsigned char data11_is_present:1;
				unsigned char data12_is_present:1;
				unsigned char data13_is_present:1;
				unsigned char data14_is_present:1;
				unsigned char data15_is_present:1;
			} __attribute__((packed));;
			struct {
				unsigned char data16_is_present:1;
				unsigned char data17_is_present:1;
				unsigned char data18_is_present:1;
				unsigned char data19_is_present:1;
				unsigned char data20_is_present:1;
				unsigned char data21_is_present:1;
				unsigned char data22_is_present:1;
				unsigned char data23_is_present:1;
			} __attribute__((packed));;
			struct {
				unsigned char data24_is_present:1;
				unsigned char data25_is_present:1;
				unsigned char data26_is_present:1;
				unsigned char data27_is_present:1;
				unsigned char data28_is_present:1;
				unsigned char data29_is_present:1;
				unsigned char data30_is_present:1;
				unsigned char data31_is_present:1;
			} __attribute__((packed));;
		};
		unsigned char data[5];
	};
};

struct synaptics_rmi4_f12_ctrl_8 {
	union {
		struct {
			unsigned char max_x_coord_lsb;
			unsigned char max_x_coord_msb;
			unsigned char max_y_coord_lsb;
			unsigned char max_y_coord_msb;
			unsigned char rx_pitch_lsb;
			unsigned char rx_pitch_msb;
			unsigned char tx_pitch_lsb;
			unsigned char tx_pitch_msb;
			unsigned char low_rx_clip;
			unsigned char high_rx_clip;
			unsigned char low_tx_clip;
			unsigned char high_tx_clip;
			unsigned char num_of_rx;
			unsigned char num_of_tx;
		} __attribute__((packed));;
		unsigned char data[14];
	};
};

struct synaptics_rmi4_f12_ctrl_23 {
	union {
		struct {
			unsigned char finger_enable:1;
			unsigned char active_stylus_enable:1;
			unsigned char palm_enable:1;
			unsigned char unclassified_object_enable:1;
			unsigned char hovering_finger_enable:1;
			unsigned char gloved_finger_enable:1;
			unsigned char f12_ctr23_00_b6__7:2;
			unsigned char max_reported_objects;
			unsigned char f12_ctr23_02_b0:1;
			unsigned char report_active_stylus_as_finger:1;
			unsigned char report_palm_as_finger:1;
			unsigned char report_unclassified_object_as_finger:1;
			unsigned char report_hovering_finger_as_finger:1;
			unsigned char report_gloved_finger_as_finger:1;
			unsigned char report_narrow_object_swipe_as_finger:1;
			unsigned char report_handedge_as_finger:1;
			unsigned char cover_enable:1;
			unsigned char stylus_enable:1;
			unsigned char eraser_enable:1;
			unsigned char small_object_enable:1;
			unsigned char f12_ctr23_03_b4__7:4;
			unsigned char report_cover_as_finger:1;
			unsigned char report_stylus_as_finger:1;
			unsigned char report_eraser_as_finger:1;
			unsigned char report_small_object_as_finger:1;
			unsigned char f12_ctr23_04_b4__7:4;
		} __attribute__((packed));;
		unsigned char data[5];
	};
};

struct synaptics_rmi4_f12_ctrl_31 {
	union {
		struct {
			unsigned char max_x_coord_lsb;
			unsigned char max_x_coord_msb;
			unsigned char max_y_coord_lsb;
			unsigned char max_y_coord_msb;
			unsigned char rx_pitch_lsb;
			unsigned char rx_pitch_msb;
			unsigned char rx_clip_low;
			unsigned char rx_clip_high;
			unsigned char wedge_clip_low;
			unsigned char wedge_clip_high;
			unsigned char num_of_p;
			unsigned char num_of_q;
		} __attribute__((packed));;
		unsigned char data[12];
	};
};

struct synaptics_rmi4_f12_ctrl_58 {
	union {
		struct {
			unsigned char reporting_format;
			unsigned char f12_ctr58_00_reserved;
			unsigned char min_force_lsb;
			unsigned char min_force_msb;
			unsigned char max_force_lsb;
			unsigned char max_force_msb;
			unsigned char light_press_threshold_lsb;
			unsigned char light_press_threshold_msb;
			unsigned char light_press_hysteresis_lsb;
			unsigned char light_press_hysteresis_msb;
			unsigned char hard_press_threshold_lsb;
			unsigned char hard_press_threshold_msb;
			unsigned char hard_press_hysteresis_lsb;
			unsigned char hard_press_hysteresis_msb;
		} __attribute__((packed));;
		unsigned char data[14];
	};
};

struct synaptics_rmi4_f12_finger_data {
	unsigned char object_type_and_status;
	unsigned char x_lsb;
	unsigned char x_msb;
	unsigned char y_lsb;
	unsigned char y_msb;
	unsigned char z;
	unsigned char wx;
	unsigned char wy;
};


extern int synaptics_rmi4_reg_read(struct synaptics_rmi4_data *rmi4_data,
		unsigned short address, unsigned char* rd_data, int r_length);
extern int synaptics_rmi4_reg_write(struct synaptics_rmi4_data *rmi4_data,
		unsigned short address, unsigned char* wr_data, int w_length);
extern int synaptics_rmi4_sw_reset(struct synaptics_rmi4_data *rmi4_data);
extern int synaptics_rmi4_int_enable(struct synaptics_rmi4_data *rmi4_data, bool enable);
extern int synaptics_rmi4_reinit();

#endif /* _SYNAPTICS_RMI_CORE_H_ */

