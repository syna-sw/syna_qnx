QNX TOUCH CONTROLLERS SUPPORTED
---------------------------

The QNX mtouch driver supports the synaptics discrete touch controllers,
and the communication protocol is listed below
   RMI over I2C


DEVELOPMENT PLATFORM
-------------

The followings are the information about the development platform and the BSP
using in Synaptics
   Platform : TI Panda Board ES
   BSP      : BSP_ti-omap4430-panda_br-660_be-660_SVN737104_JBN4
   QNX SDP  : QNX SDP 6.6.0


DRIVER SOURCE
-------------

The mtouch driver source code and the configuration of platform can be found
in the \src\hardware directory inside the driver tarball. Here is the source
tree in this directory.

src\hardware\mtouch\
   
   syna\synaptics_mtouch.[ch]
      Source code of mtouch driver interface, which is used to connect 
	  the driver to the QNX input event framework
   
   syna\synaptics_rmi4_core.[ch]
      Source code of core driver
   
   syna\synaptics_rmi4_fw_update.[ch]
      Source code of driver related to firmware updating

   syna\Makefile
      Inner makefile

   syna\nto\
      Folder of built object files

   common.mk
      Qconfig for setting up the build configuration of QNX mtouch driver

   Makefile
      Top-level makefile for building the QNX mtouch driver


src\hardware\startup\boards\
   
   omap4430\panda\build
      Example build script for PandaBoard to include mtouch driver
      Please refer to the keyword, "## Touch driver", and copy all strings in
      this section

      The libmtouch-syna.so is target built binary of Synaptics QNX mtouch driver

   
   omap4430\panda\init_gpio.c
   omap4430\panda\io_cfg.c
      Example code to configure the gpio as an interrupt
      Please reference for the setting of "GPIO 39"


prebuilt\arm-v7\usr\lib\graphics\
   
   omap4430\graphics.conf
      An example configuration file to setup mtouch driver begigning with "begin mtouch"
      and end with "end mtouch"

      The followings are definded parameters 
            - i2c_devname       : i2c bus
            - i2c_slave         : i2c slave address
            - i2c_speed         : i2c speed, default is 400K (option)
            - fw_update_startup : firmware update during system power-on
                                  1 = enable / 0 = disable 
            - fw_img            : path of fw image file (option)
            - fw_img_id         : fw id defined in image file (option)


install\etc\system\
   
   config\scaling.conf
      Example file to provide scalling information to Screen
   
   fw\
      Folder to place the firmare image file

install\usr\include\
   
   input\
      Libraries about mtouch framework


DRIVER PORTING
--------------

The procedures listed below describes the general process to port the mtouch driver
to the target platform. Depending on the actual BSP used for the target platform,
adjustments and additional implementation may be needed.

1) Copy entire mtouch folder from src\hardware\ directory of the released tarball
   to the equivalent directory in the source tree of the target platform.

2) Copy all header files in the install\usr\include\input\ directory of the
   released tarball to the equivalent directory in the source tree of the
   target platform.

3) Update the graphics.conf of the target platform by referring to the graphics.conf 
   in the released tarball. The default configuration of a mtouch device is shown below.

      begin mtouch
         driver = syna
         options = i2c_devname=/dev/i2c3,i2c_slave=0x20,i2c_speed=100000,fw_update_startup=0
         display = 1
      end mtouch
   
   The parameters, i2c_devname and i2c_slave, must be set properly based on the DUT.

4) Add the lines below, as the build script in the src\hardware\startup\boards\omap4430\panda\ 
   directory of the released tarball, to the equivalent file in the source tree of the 
   target platform.

      libmtouch-syna.so
      libmtouch-calib.so.1
      /usr/bin/calib-touch=calib-touch
      /etc/system/config/scaling.conf=../install/etc/system/config/scaling.conf

5) At the end of a successful build, the libmtouch-syna.so should be generated



FIRMWARE UPDATE DURING SYSTEM POWER-ON
--------------------------------------

This section provides a few steps to enable the firmware update during the system power-on.

1) Enable the "fw_update_startup" parameter in the graphics.conf (ex, fw_update_startup=1),
   and "fw_img" and "fw_img_id" parameters are expected to provide the full path of the iamge file
   and the target firmware id.

   The driver will use these two parameters to parse the appointed image file and run firmware update.
   Here is an example with the image file, PR1708562-s3501.img, and the target firmware id is 1708562 

      begin mtouch
         driver = syna
         options = i2c_devname=/dev/i2c3,i2c_slave=0x20,i2c_speed=100000,fw_update_startup=1,fw_img=/etc/system/fw/PR1708562-s3501.img,fw_img_id=1708562 
         display = 1
      end mtouch 
	  
2) Put the appointed .img image file in the /install/etc/system/fw/ directory
	  
3) Modify the built script to inlcude a firmware image file(.img)

      /etc/system/fw/PR1708562-s3501.img=../install/etc/system/fw/PR1708562-s3501.img

   Here is an example of the modified built script

      libmtouch-syna.so
      libmtouch-calib.so.1
      /usr/bin/calib-touch=calib-touch
      /etc/system/config/scaling.conf=../install/etc/system/config/scaling.conf
      /etc/system/fw/PR1708562-s3501.img=../install/etc/system/fw/PR1708562-s3501.img


   


