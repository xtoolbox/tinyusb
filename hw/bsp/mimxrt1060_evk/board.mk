CFLAGS += \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m7 \
  -mfloat-abi=hard \
  -mfpu=fpv5-d16 \
  -D__ARMVFP__=0 -D__ARMFPV5__=0\
  -DCPU_MIMXRT1062DVL6A \
  -DXIP_EXTERNAL_FLASH=1 \
  -DXIP_BOOT_HEADER_ENABLE=1 \
  -DCFG_TUSB_MCU=OPT_MCU_MIMXRT10XX \
  -DCFG_TUSB_MEM_SECTION='__attribute__((section(".data")))' \
  -DCFG_TUSB_MEM_ALIGN='__attribute__((aligned(64)))'

# mcu driver cause following warnings
#CFLAGS += -Wno-error=float-equal -Wno-error=nested-externs
CFLAGS += -Wno-error=unused-parameter

MCU_DIR = hw/mcu/nxp/sdk/devices/MIMXRT1062

# All source paths should be relative to the top level.
LD_FILE = $(MCU_DIR)/gcc/MIMXRT1062xxxxx_flexspi_nor.ld

SRC_C += \
	$(MCU_DIR)/system_MIMXRT1062.c \
	$(MCU_DIR)/xip/fsl_flexspi_nor_boot.c \
	$(MCU_DIR)/project_template/clock_config.c \
	$(MCU_DIR)/drivers/fsl_clock.c \
	$(MCU_DIR)/drivers/fsl_gpio.c \
	$(MCU_DIR)/drivers/fsl_common.c \
	$(MCU_DIR)/drivers/fsl_lpuart.c

INC += \
	$(TOP)/hw/bsp/$(BOARD) \
	$(TOP)/$(MCU_DIR)/../../CMSIS/Include \
	$(TOP)/$(MCU_DIR) \
	$(TOP)/$(MCU_DIR)/drivers \
	$(TOP)/$(MCU_DIR)/project_template \

SRC_S += $(MCU_DIR)/gcc/startup_MIMXRT1062.S

# For TinyUSB port source
VENDOR = nxp
CHIP_FAMILY = transdimension

# For freeRTOS port source
FREERTOS_PORT = ARM_CM7

# For flash-jlink target
JLINK_DEVICE = MIMXRT1062xxx6A
JLINK_IF = swd

# flash using pyocd
#flash: $(BUILD)/$(BOARD)-firmware.hex
#	pyocd flash -t mimxrt1050_quadspi $<

flash: $(BUILD)/$(BOARD)-firmware.bin
	cp $< /media/$(USER)/RT1060-EVK/
