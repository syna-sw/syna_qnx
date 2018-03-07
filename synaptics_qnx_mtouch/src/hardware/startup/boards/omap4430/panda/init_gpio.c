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


#include "startup.h"
#include "omap4430.h"

#define delay(x) {volatile unsigned _delay = x; while (_delay--); }

#define GPIO_GROUP(x)           ((x)/32)
#define GPIO_BIT_MASK(x)        ((1U) << ((x)%32))

unsigned GPIO_BANK_OFF[6] =
{
    OMAP44XX_GPIO1_BASE,
    OMAP44XX_GPIO2_BASE,
    OMAP44XX_GPIO3_BASE,
    OMAP44XX_GPIO4_BASE,
    OMAP44XX_GPIO5_BASE,
    OMAP44XX_GPIO6_BASE,
};

void early_init_gpio(void)
{
}

void init_gpio(void)
{

  // From OMAPTM 4 PandaBoard System Reference Manual
  // Table 9: USB Host Port/Ethernet GPIO Definitions

  // GPIO 0 (TFP410_NPD):  set as output, high
  // Deassert power down to video serializer @ U2
  out32(OMAP44XX_GPIO_OFF_SETDATAOUT + GPIO_BANK_OFF[GPIO_GROUP(0)],
        GPIO_BIT_MASK(0));
  out32(OMAP44XX_GPIO_OFF_OE + GPIO_BANK_OFF[GPIO_GROUP(0)],
        in32(OMAP44XX_GPIO_OFF_OE + GPIO_BANK_OFF[GPIO_GROUP(0)]) &
        ~GPIO_BIT_MASK(0));

  // GPIO 1 (HUB_LDO_EN):  set as output, active high
  // Enable to the Hub 3.3V LDO @ U11
  out32(OMAP44XX_GPIO_OFF_SETDATAOUT + GPIO_BANK_OFF[GPIO_GROUP(1)], 
        GPIO_BIT_MASK(1));
  out32(OMAP44XX_GPIO_OFF_OE + GPIO_BANK_OFF[GPIO_GROUP(1)],
        in32(OMAP44XX_GPIO_OFF_OE + GPIO_BANK_OFF[GPIO_GROUP(1)]) &
        ~GPIO_BIT_MASK(1));

  // GPIO 62 (HUB_NRESET):  set as output, active high
  // USB/Ethernet Hub start to operate
  out32(OMAP44XX_GPIO_OFF_SETDATAOUT + GPIO_BANK_OFF[GPIO_GROUP(62)], 
        GPIO_BIT_MASK(62));
  out32(OMAP44XX_GPIO_OFF_OE + GPIO_BANK_OFF[GPIO_GROUP(62)],
        in32(OMAP44XX_GPIO_OFF_OE + GPIO_BANK_OFF[GPIO_GROUP(62)]) &
        ~GPIO_BIT_MASK(62));

  // GPIO 34 (ENET_IRQ):  set as input, trigger on falling edge
  out32(OMAP44XX_GPIO_OFF_OE + GPIO_BANK_OFF[GPIO_GROUP(34)], 
        in32(OMAP44XX_GPIO_OFF_OE + GPIO_BANK_OFF[GPIO_GROUP(34)]) |
        GPIO_BIT_MASK(34));  
  out32(OMAP44XX_GPIO_OFF_FALLINGDETECT + GPIO_BANK_OFF[GPIO_GROUP(34)], 
        in32(OMAP44XX_GPIO_OFF_FALLINGDETECT + GPIO_BANK_OFF[GPIO_GROUP(34)]) |
        GPIO_BIT_MASK(34));  

  // GPIO 37 (CAM_PDN_A):  set as output, low
  out32(OMAP44XX_GPIO_OFF_CLEARDATAOUT + GPIO_BANK_OFF[GPIO_GROUP(37)],
        GPIO_BIT_MASK(37));
  out32(OMAP44XX_GPIO_OFF_OE + GPIO_BANK_OFF[GPIO_GROUP(37)],
        in32(OMAP44XX_GPIO_OFF_OE + GPIO_BANK_OFF[GPIO_GROUP(37)]) &
        ~GPIO_BIT_MASK(37));

  // GPIO 39 (MTOUCH_IRQ):  set as input, trigger on falling edge
  out32(OMAP44XX_GPIO_OFF_OE + GPIO_BANK_OFF[GPIO_GROUP(39)],
        in32(OMAP44XX_GPIO_OFF_OE + GPIO_BANK_OFF[GPIO_GROUP(39)]) |
        GPIO_BIT_MASK(39));
  out32(OMAP44XX_GPIO_OFF_FALLINGDETECT + GPIO_BANK_OFF[GPIO_GROUP(39)],
        in32(OMAP44XX_GPIO_OFF_FALLINGDETECT + GPIO_BANK_OFF[GPIO_GROUP(39)]) |
        GPIO_BIT_MASK(39));

  // GPIO 41 (HDMI_LS_OE):  set as output, low
  out32(OMAP44XX_GPIO_OFF_CLEARDATAOUT + GPIO_BANK_OFF[GPIO_GROUP(41)], 
        GPIO_BIT_MASK(41));  
  out32(OMAP44XX_GPIO_OFF_OE + GPIO_BANK_OFF[GPIO_GROUP(41)], 
        in32(OMAP44XX_GPIO_OFF_OE + GPIO_BANK_OFF[GPIO_GROUP(41)]) &
        ~GPIO_BIT_MASK(41));  

  // GPIO 43 (WLAN_EN):  set as output, low
  out32(OMAP44XX_GPIO_OFF_CLEARDATAOUT + GPIO_BANK_OFF[GPIO_GROUP(43)], 
        GPIO_BIT_MASK(43));  
  out32(OMAP44XX_GPIO_OFF_OE + GPIO_BANK_OFF[GPIO_GROUP(43)], 
        in32(OMAP44XX_GPIO_OFF_OE + GPIO_BANK_OFF[GPIO_GROUP(43)]) &
        ~GPIO_BIT_MASK(43));  
  // GPIO 48 (ENET_ENABLE):  set as output, low
  out32(OMAP44XX_GPIO_OFF_CLEARDATAOUT + GPIO_BANK_OFF[GPIO_GROUP(48)], 
        GPIO_BIT_MASK(48));  
  out32(OMAP44XX_GPIO_OFF_OE + GPIO_BANK_OFF[GPIO_GROUP(48)], 
        in32(OMAP44XX_GPIO_OFF_OE + GPIO_BANK_OFF[GPIO_GROUP(48)]) &
        ~GPIO_BIT_MASK(48));  

  // GPIO 53 (EXP1_IRQA): input, trigger interrupt on falling edge
  out32(OMAP44XX_GPIO_OFF_OE + GPIO_BANK_OFF[GPIO_GROUP(53)], 
        in32(OMAP44XX_GPIO_OFF_OE + GPIO_BANK_OFF[GPIO_GROUP(53)]) |
        GPIO_BIT_MASK(53));  
  out32(OMAP44XX_GPIO_OFF_FALLINGDETECT + GPIO_BANK_OFF[GPIO_GROUP(53)], 
        in32(OMAP44XX_GPIO_OFF_FALLINGDETECT + GPIO_BANK_OFF[GPIO_GROUP(53)]) |
        GPIO_BIT_MASK(53));  

  // GPIO 54 (WLAN_ENABLE): output, set low
  out32(OMAP44XX_GPIO_OFF_CLEARDATAOUT + GPIO_BANK_OFF[GPIO_GROUP(54)], 
        GPIO_BIT_MASK(54));  
  out32(OMAP44XX_GPIO_OFF_OE + GPIO_BANK_OFF[GPIO_GROUP(54)], 
        in32(OMAP44XX_GPIO_OFF_OE + GPIO_BANK_OFF[GPIO_GROUP(54)]) &
        ~GPIO_BIT_MASK(54));  

  // GPIO 46 (BT_EN): output, set low
  out32(OMAP44XX_GPIO_OFF_CLEARDATAOUT + GPIO_BANK_OFF[GPIO_GROUP(46)],
        GPIO_BIT_MASK(46));
  out32(OMAP44XX_GPIO_OFF_OE + GPIO_BANK_OFF[GPIO_GROUP(46)],
        in32(OMAP44XX_GPIO_OFF_OE + GPIO_BANK_OFF[GPIO_GROUP(46)]) &
        ~GPIO_BIT_MASK(46));

  // GPIO 60 (HDMI_CT_CP_HPD):  set as output, low
  out32(OMAP44XX_GPIO_OFF_CLEARDATAOUT + GPIO_BANK_OFF[GPIO_GROUP(60)], 
        GPIO_BIT_MASK(60));  
  out32(OMAP44XX_GPIO_OFF_OE + GPIO_BANK_OFF[GPIO_GROUP(60)], 
        in32(OMAP44XX_GPIO_OFF_OE + GPIO_BANK_OFF[GPIO_GROUP(60)]) &
        ~GPIO_BIT_MASK(60));  

  // GPIO 83 (CAM_GLB_nRST):  set as output, low
  out32(OMAP44XX_GPIO_OFF_CLEARDATAOUT + GPIO_BANK_OFF[GPIO_GROUP(83)],
        GPIO_BIT_MASK(83));
  out32(OMAP44XX_GPIO_OFF_OE + GPIO_BANK_OFF[GPIO_GROUP(83)],
        in32(OMAP44XX_GPIO_OFF_OE + GPIO_BANK_OFF[GPIO_GROUP(83)]) &
        ~GPIO_BIT_MASK(83));

  // GPIO 102 (LVDS_RST#):  set as output, low
  out32(OMAP44XX_GPIO_OFF_CLEARDATAOUT + GPIO_BANK_OFF[GPIO_GROUP(102)],
        GPIO_BIT_MASK(102));
  out32(OMAP44XX_GPIO_OFF_OE + GPIO_BANK_OFF[GPIO_GROUP(102)],
        in32(OMAP44XX_GPIO_OFF_OE + GPIO_BANK_OFF[GPIO_GROUP(102)]) &
        ~GPIO_BIT_MASK(102));

  // make GPIO 138 an output, with output high
  out32(OMAP44XX_GPIO_OFF_SETDATAOUT + GPIO_BANK_OFF[GPIO_GROUP(138)],
        GPIO_BIT_MASK(138));
  out32(OMAP44XX_GPIO_OFF_OE + GPIO_BANK_OFF[GPIO_GROUP(138)],
        in32(OMAP44XX_GPIO_OFF_OE + GPIO_BANK_OFF[GPIO_GROUP(138)]) &
        ~GPIO_BIT_MASK(138));
}

#if defined(__QNXNTO__) && defined(__USESRCVERSION)
#include <sys/srcversion.h>
__SRCVERSION("$URL: http://svn/product/branches/6.6.0/trunk/hardware/startup/boards/omap4430/panda/init_gpio.c $ $Rev: 731022 $")
#endif
