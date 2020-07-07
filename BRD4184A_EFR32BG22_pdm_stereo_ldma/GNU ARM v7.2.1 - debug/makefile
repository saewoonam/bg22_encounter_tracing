################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include src/subdir.mk
-include emlib/subdir.mk
-include DSP_FFT/subdir.mk
-include CMSIS/EFR32BG22/subdir.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: BRD4184A_EFR32BG22_pdm_stereo_ldma.axf

# Tool invocations
BRD4184A_EFR32BG22_pdm_stereo_ldma.axf: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: GNU ARM C Linker'
	arm-none-eabi-gcc -g3 -gdwarf-2 -mcpu=cortex-m33 -mthumb -T "BRD4184A_EFR32BG22_pdm_stereo_ldma.ld" -Xlinker --gc-sections -Xlinker -Map="BRD4184A_EFR32BG22_pdm_stereo_ldma.map" -mfpu=fpv5-sp-d16 -mfloat-abi=softfp --specs=nano.specs -o BRD4184A_EFR32BG22_pdm_stereo_ldma.axf "./CMSIS/EFR32BG22/startup_efr32bg22.o" "./CMSIS/EFR32BG22/system_efr32bg22.o" "./DSP_FFT/arm_bitreversal2.o" "./DSP_FFT/arm_cfft_f32.o" "./DSP_FFT/arm_cfft_q15.o" "./DSP_FFT/arm_cfft_q31.o" "./DSP_FFT/arm_cfft_radix4_init_q15.o" "./DSP_FFT/arm_cfft_radix4_q15.o" "./DSP_FFT/arm_cfft_radix4_q31.o" "./DSP_FFT/arm_cfft_radix8_f32.o" "./DSP_FFT/arm_cmplx_mag_f32.o" "./DSP_FFT/arm_cmplx_mag_q15.o" "./DSP_FFT/arm_cmplx_mag_q31.o" "./DSP_FFT/arm_common_tables.o" "./DSP_FFT/arm_const_structs.o" "./DSP_FFT/arm_max_f32.o" "./DSP_FFT/arm_sqrt_q15.o" "./DSP_FFT/arm_sqrt_q31.o" "./emlib/em_cmu.o" "./emlib/em_core.o" "./emlib/em_emu.o" "./emlib/em_gpio.o" "./emlib/em_int.o" "./emlib/em_ldma.o" "./emlib/em_pdm.o" "./emlib/em_rtcc.o" "./emlib/em_system.o" "./emlib/em_usart.o" "./retargetio.o" "./src/arm_fft_bin_data.o" "./src/arm_fft_bin_example_f32.o" "./src/blackman_window_256.o" "./src/main_pdm_stereo_ldma.o" "./src/retargetserial.o" -lm -Wl,--start-group -lgcc -lc -lnosys -Wl,--end-group
	@echo 'Finished building target: $@'
	@echo ' '

	@echo 'Building hex file: BRD4184A_EFR32BG22_pdm_stereo_ldma.hex'
	arm-none-eabi-objcopy -O ihex "BRD4184A_EFR32BG22_pdm_stereo_ldma.axf" "BRD4184A_EFR32BG22_pdm_stereo_ldma.hex"
	@echo ' '

	@echo 'Building bin file: BRD4184A_EFR32BG22_pdm_stereo_ldma.bin'
	arm-none-eabi-objcopy -O binary "BRD4184A_EFR32BG22_pdm_stereo_ldma.axf" "BRD4184A_EFR32BG22_pdm_stereo_ldma.bin"
	@echo ' '

	@echo 'Building s37 file: BRD4184A_EFR32BG22_pdm_stereo_ldma.s37'
	arm-none-eabi-objcopy -O srec "BRD4184A_EFR32BG22_pdm_stereo_ldma.axf" "BRD4184A_EFR32BG22_pdm_stereo_ldma.s37"
	@echo ' '

	@echo 'Running size tool'
	arm-none-eabi-size "BRD4184A_EFR32BG22_pdm_stereo_ldma.axf" -A
	@echo ' '

# Other Targets
clean:
	-$(RM) $(EXECUTABLES)$(OBJS)$(C_DEPS) BRD4184A_EFR32BG22_pdm_stereo_ldma.axf
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets
