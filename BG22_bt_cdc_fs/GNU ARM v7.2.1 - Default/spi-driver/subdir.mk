################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../spi-driver/btl_driver_spi.c 

OBJS += \
./spi-driver/btl_driver_spi.o 

C_DEPS += \
./spi-driver/btl_driver_spi.d 


# Each subdirectory must supply rules for building sources it contributes
spi-driver/btl_driver_spi.o: ../spi-driver/btl_driver_spi.c
	@echo 'Building file: $<'
	@echo 'Invoking: GNU ARM C Compiler'
	arm-none-eabi-gcc -g -gdwarf-2 -mcpu=cortex-m33 -mthumb -std=c99 '-DNVM3_DEFAULT_NVM_SIZE=24576' '-DHAL_CONFIG=1' '-D__StackLimit=0x20000000' '-D__HEAP_SIZE=0xD00' '-D__STACK_SIZE=0x800' '-DEFR32BG22C224F512IM40=1' -I"/Users/nams/SimplicityStudio/v4_workspace/Lab 1/platform/Device/SiliconLabs/EFR32BG22/Include" -I"/Users/nams/SimplicityStudio/v4_workspace/Lab 1/platform/emlib/inc" -I"/Users/nams/SimplicityStudio/v4_workspace/Lab 1/platform/CMSIS/Include" -I"/Users/nams/SimplicityStudio/v4_workspace/Lab 1/platform/emdrv/sleep/src" -I"/Users/nams/SimplicityStudio/v4_workspace/Lab 1/platform/emlib/src" -I"/Users/nams/SimplicityStudio/v4_workspace/Lab 1/hardware/kit/common/halconfig" -I"/Users/nams/SimplicityStudio/v4_workspace/Lab 1/platform/radio/rail_lib/protocol/ieee802154" -I"/Users/nams/SimplicityStudio/v4_workspace/Lab 1/protocol/bluetooth/ble_stack/inc/soc" -I"/Users/nams/SimplicityStudio/v4_workspace/Lab 1/app/bluetooth/common/util" -I"/Users/nams/SimplicityStudio/v4_workspace/Lab 1/hardware/kit/common/bsp" -I"/Users/nams/SimplicityStudio/v4_workspace/Lab 1/platform/service/sleeptimer/inc" -I"/Users/nams/SimplicityStudio/v4_workspace/Lab 1/hardware/kit/common/drivers" -I"/Users/nams/SimplicityStudio/v4_workspace/Lab 1/platform/Device/SiliconLabs/EFR32BG22/Source" -I"/Users/nams/SimplicityStudio/v4_workspace/Lab 1/platform/radio/rail_lib/common" -I"/Users/nams/SimplicityStudio/v4_workspace/Lab 1/platform/emdrv/nvm3/inc" -I"/Users/nams/SimplicityStudio/v4_workspace/Lab 1/platform/service/sleeptimer/src" -I"/Users/nams/SimplicityStudio/v4_workspace/Lab 1/platform/radio/rail_lib/chip/efr32/efr32xg2x" -I"/Users/nams/SimplicityStudio/v4_workspace/Lab 1/platform/emdrv/nvm3/src" -I"/Users/nams/SimplicityStudio/v4_workspace/Lab 1/platform/emdrv/gpiointerrupt/inc" -I"/Users/nams/SimplicityStudio/v4_workspace/Lab 1/platform/halconfig/inc/hal-config" -I"/Users/nams/SimplicityStudio/v4_workspace/Lab 1/platform/service/sleeptimer/config" -I"/Users/nams/SimplicityStudio/v4_workspace/Lab 1/platform/radio/rail_lib/protocol/ble" -I"/Users/nams/SimplicityStudio/v4_workspace/Lab 1/platform/common/inc" -I"/Users/nams/SimplicityStudio/v4_workspace/Lab 1/platform/emdrv/gpiointerrupt/src" -I"/Users/nams/SimplicityStudio/v4_workspace/Lab 1/protocol/bluetooth/ble_stack/inc/common" -I"/Users/nams/SimplicityStudio/v4_workspace/Lab 1/hardware/kit/EFR32BG22_BRD4184A/config" -I"/Users/nams/SimplicityStudio/v4_workspace/Lab 1/platform/emdrv/common/inc" -I"/Users/nams/SimplicityStudio/v4_workspace/Lab 1/platform/emdrv/sleep/inc" -I"/Users/nams/SimplicityStudio/v4_workspace/Lab 1/platform/emdrv/uartdrv/inc" -I"/Users/nams/SimplicityStudio/v4_workspace/Lab 1/platform/bootloader/api" -I"/Users/nams/SimplicityStudio/v4_workspace/Lab 1/platform/emdrv/dmadrv/inc" -I"/Users/nams/SimplicityStudio/v4_workspace/Lab 1/platform/Device/SiliconLabs/EFR32BG22/Source/GCC" -I"/Users/nams/SimplicityStudio/v4_workspace/Lab 1/platform/emdrv/dmadrv/src" -I"/Users/nams/SimplicityStudio/v4_workspace/Lab 1" -I"/Users/nams/SimplicityStudio/v4_workspace/Lab 1/platform/bootloader" -O2 -Wall -c -fmessage-length=0 -ffunction-sections -fdata-sections -mfpu=fpv5-sp-d16 -mfloat-abi=hard -MMD -MP -MF"spi-driver/btl_driver_spi.d" -MT"spi-driver/btl_driver_spi.o" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


