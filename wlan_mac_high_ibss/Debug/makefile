################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include subdir.mk
-include wlan_platform_high/subdir.mk
-include wlan_platform_common/subdir.mk
-include wlan_mac_high_framework/subdir.mk
-include wlan_mac_high_framework/wlan_exp_ip_udp/subdir.mk
-include wlan_mac_common_framework/subdir.mk
-include src/subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 
ELFSIZE += \
wlan_mac_high_ibss.elf.size \

ELFCHECK += \
wlan_mac_high_ibss.elf.elfcheck \


# All Target
all: wlan_mac_high_ibss.elf secondary-outputs

# Tool invocations
wlan_mac_high_ibss.elf: $(OBJS) ../../svn_src/wlan_w3_high/lscript.ld $(USER_OBJS)
	@echo Building target: $@
	@echo Invoking: MicroBlaze gcc linker
	mb-gcc -Wl,--no-relax -Wl,-Map=executable.map,--gc-sections -Wl,-T -Wl,../../svn_src/wlan_w3_high/lscript.ld -L../../wlan_bsp_cpu_high/mb_high/lib -mlittle-endian -mxl-barrel-shift -mxl-pattern-compare -mcpu=v8.40.b -mno-xl-soft-mul -o"wlan_mac_high_ibss.elf" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo Finished building target: $@
	@echo ' '

wlan_mac_high_ibss.elf.size: wlan_mac_high_ibss.elf
	@echo Invoking: MicroBlaze Print Size
	mb-size wlan_mac_high_ibss.elf  |tee "wlan_mac_high_ibss.elf.size"
	@echo Finished building: $@
	@echo ' '

wlan_mac_high_ibss.elf.elfcheck: wlan_mac_high_ibss.elf
	@echo Invoking: Xilinx ELF Check
	elfcheck wlan_mac_high_ibss.elf -hw ../../Mango_802.11_RefDes_v1.7.1_hw_platform/system.xml -pe mb_high  |tee "wlan_mac_high_ibss.elf.elfcheck"
	@echo Finished building: $@
	@echo ' '

# Other Targets
clean:
	-$(RM) $(OBJS)$(C_DEPS)$(EXECUTABLES)$(ELFSIZE)$(ELFCHECK)$(S_UPPER_DEPS) wlan_mac_high_ibss.elf
	-@echo ' '

secondary-outputs: $(ELFSIZE) $(ELFCHECK)

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets
