################################################################################
# MRS Version: 2.4.0
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
S_UPPER_SRCS += \
c:/Arturo/UNI/ULB/Embedded/Lab4/SoundEffectGenerator/Common/Startup/startup_ch32h417_v3f.S 

S_UPPER_DEPS += \
./Startup/startup_ch32h417_v3f.d 

OBJS += \
./Startup/startup_ch32h417_v3f.o 

DIR_OBJS += \
./Startup/*.o \

DIR_DEPS += \
./Startup/*.d \

DIR_EXPANDS += \
./Startup/*.253r.expand \


# Each subdirectory must supply rules for building sources it contributes
Startup/startup_ch32h417_v3f.o: c:/Arturo/UNI/ULB/Embedded/Lab4/SoundEffectGenerator/Common/Startup/startup_ch32h417_v3f.S
	@	riscv-wch-elf-gcc -march=rv32imac_zba_zbb_zbc_zbs_xw -mabi=ilp32 -msmall-data-limit=8 -msave-restore -fmax-errors=20 -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized -g -x assembler-with-cpp -I"c:/Arturo/UNI/ULB/Embedded/Lab4/SoundEffectGenerator/Common/Startup" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

