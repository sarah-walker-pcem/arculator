#
# $Id: Makefile,v 1.2 2006/03/07 15:17:10 mhowkins Exp $
#

AS = armas
LD = armld
OBJCOPY = armobjcopy




all: hostfs,ffa hostfsfiler,ffa

%,ffa: %.elf
	$(OBJCOPY) -O binary $< $@

%.elf: %.o
	$(LD) --section-start .text=0 -o $@ $<

%.o: %.s
	$(AS) -o $@ $<

clean:
	rm -f *.o *.elf *,ffa
