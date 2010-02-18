
ARM			:= arm
PPC			:= ppc
DATA		:= data

ARMPREFIX	:= $(DEVKITARM)/bin/arm-eabi
ARMOBJCOPY	:= $(ARMPREFIX)-objcopy

ARMELF		:= $(CURDIR)/$(ARM)/$(ARM).elf
ARMBIN		:= $(CURDIR)/$(PPC)/$(DATA)/$(ARM).bin
PPCELF		:= $(CURDIR)/$(PPC)/$(PPC).elf

TARGET	:= iostester


TARGETELF := $(TARGET).elf

.PHONY:		$(ARMELF) $(PPCELF)

all: $(TARGETELF)
clean:
	@echo clean...
	@rm -rf *.elf
	@$(MAKE) --no-print-directory -C $(ARM) clean
	@$(MAKE) --no-print-directory -C $(PPC) clean

$(TARGETELF): $(PPCELF)
	@echo creating $(notdir $@)
	@cp $< $@
		
$(ARMELF): 
	@echo creating  $(notdir $@)
	@$(MAKE) --no-print-directory -C $(ARM)
	
$(PPCELF): $(ARMBIN)
	@echo creating  $(notdir $@)
	@$(MAKE) --no-print-directory -C $(PPC)

$(ARMBIN): $(ARMELF)
	@echo creating  $(notdir $@)
	@$(ARMOBJCOPY) -O binary $< $@
