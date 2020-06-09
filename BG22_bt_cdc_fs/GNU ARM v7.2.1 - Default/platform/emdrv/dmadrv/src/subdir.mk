################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../platform/emdrv/dmadrv/src/dmadrv.c 

OBJS += \
./platform/emdrv/dmadrv/src/dmadrv.o 

C_DEPS += \
./platform/emdrv/dmadrv/src/dmadrv.d 


# Each subdirectory must supply rules for building sources it contributes
platform/emdrv/dmadrv/src/dmadrv.o: ../platform/emdrv/dmadrv/src/dmadrv.c
	@echo 'Building file: $<'
	@echo 'Invoking: GNU ARM C Compiler'
	arm-none-eabi-gcc -g -gdwarf-2 -mcpu=cortex-m33 -mthumb -std=c99 '-DNVM3_DEFAULT_NVM_SIZE=24576' '-DHAL_CONFIG=1' '-D__StackLimit=0x20000000' '-D__HEAP_SIZE=0xD00' '-D__STACK_SIZE=0x800' '-DEFR32BG22C224F512IM40=1' -I"/Users/nams/SimplicityStudio/v4_workspace/BG22_bt_cdc_fs/platform/Device/SiliconLabs/EFR32BG22/Include" -I"/Users/nams/SimplicityStudio/v4_workspace/BG22_bt_cdc_fs/platform/emlib/inc" -I"/Users/nams/SimplicityStudio/v4_workspace/BG22_bt_cdc_fs/platform/CMSIS/Include" -I"/Users/nams/SimplicityStudio/v4_workspace/BG22_bt_cdc_fs/platform/emdrv/sleep/src" -I"/Users/nams/SimplicityStudio/v4_workspace/BG22_bt_cdc_fs/platform/emlib/src" -I"/Users/nams/SimplicityStudio/v4_workspace/BG22_bt_cdc_fs/hardware/kit/common/halconfig" -I"/Users/nams/SimplicityStudio/v4_workspace/BG22_bt_cdc_fs/platform/radio/rail_lib/protocol/ieee802154" -I"/Users/nams/SimplicityStudio/v4_workspace/BG22_bt_cdc_fs/protocol/bluetooth/ble_stack/inc/soc" -I"/Users/nams/SimplicityStudio/v4_workspace/BG22_bt_cdc_fs/app/bluetooth/common/util" -I"/Users/nams/SimplicityStudio/v4_workspace/BG22_bt_cdc_fs/hardware/kit/common/bsp" -I"/Users/nams/SimplicityStudio/v4_workspace/BG22_bt_cdc_fs/platform/service/sleeptimer/inc" -I"/Users/nams/SimplicityStudio/v4_workspace/BG22_bt_cdc_fs/hardware/kit/common/drivers" -I"/Users/nams/SimplicityStudio/v4_workspace/BG22_bt_cdc_fs/platform/Device/SiliconLabs/EFR32BG22/Source" -I"/Users/nams/SimplicityStudio/v4_workspace/BG22_bt_cdc_fs/platform/radio/rail_lib/common" -I"/Users/nams/SimplicityStudio/v4_workspace/BG22_bt_cdc_fs/platform/emdrv/nvm3/inc" -I"/Users/nams/SimplicityStudio/v4_workspace/BG22_bt_cdc_fs/platform/service/sleeptimer/src" -I"/Users/nams/SimplicityStudio/v4_workspace/BG22_bt_cdc_fs/platform/radio/rail_lib/chip/efr32/efr32xg2x" -I"/Users/nams/SimplicityStudio/v4_workspace/BG22_bt_cdc_fs/platform/emdrv/nvm3/src" -I"/Users/nams/SimplicityStudio/v4_workspace/BG22_bt_cdc_fs/platform/emdrv/gpiointerrupt/inc" -I"/Users/nams/SimplicityStudio/v4_workspace/BG22_bt_cdc_fs/platform/halconfig/inc/hal-config" -I"/Users/nams/SimplicityStudio/v4_workspace/BG22_bt_cdc_fs/platform/service/sleeptimer/config" -I"/Users/nams/SimplicityStudio/v4_workspace/BG22_bt_cdc_fs/platform/radio/rail_lib/protocol/ble" -I"/Users/nams/SimplicityStudio/v4_workspace/BG22_bt_cdc_fs/platform/common/inc" -I"/Users/nams/SimplicityStudio/v4_workspace/BG22_bt_cdc_fs/platform/emdrv/gpiointerrupt/src" -I"/Users/nams/SimplicityStudio/v4_workspace/BG22_bt_cdc_fs/protocol/bluetooth/ble_stack/inc/common" -I"/Users/nams/SimplicityStudio/v4_workspace/BG22_bt_cdc_fs/hardware/kit/EFR32BG22_BRD4184A/config" -I"/Users/nams/SimplicityStudio/v4_workspace/BG22_bt_cdc_fs/platform/emdrv/common/inc" -I"/Users/nams/SimplicityStudio/v4_workspace/BG22_bt_cdc_fs/platform/emdrv/sleep/inc" -I"/Users/nams/SimplicityStudio/v4_workspace/BG22_bt_cdc_fs/platform/emdrv/uartdrv/inc" -I"/Users/nams/SimplicityStudio/v4_workspace/BG22_bt_cdc_fs/platform/bootloader/api" -I"/Users/nams/SimplicityStudio/v4_workspace/BG22_bt_cdc_fs/platform/emdrv/dmadrv/inc" -I"/Users/nams/SimplicityStudio/v4_workspace/BG22_bt_cdc_fs/platform/Device/SiliconLabs/EFR32BG22/Source/GCC" -I"/Users/nams/SimplicityStudio/v4_workspace/BG22_bt_cdc_fs/platform/emdrv/dmadrv/src" -I"/Users/nams/SimplicityStudio/v4_workspace/BG22_bt_cdc_fs" -I"/Users/nams/SimplicityStudio/v4_workspace/BG22_bt_cdc_fs/platform/bootloader" -O2 -Wall -c -fmessage-length=0 -ffunction-sections -fdata-sections -mfpu=fpv5-sp-d16 -mfloat-abi=hard -MMD -MP -MF"platform/emdrv/dmadrv/src/dmadrv.d" -MT"platform/emdrv/dmadrv/src/dmadrv.o" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


