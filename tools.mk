.PHONY: flash
flash: all
	openocd -f board/stm32f4discovery.cfg -c "program build/ch.elf verify reset"

.PHONY: r
r: reset
.PHONY: reset
reset:
	openocd -f board/stm32f4discovery.cfg -c "init" -c "reset" -c "shutdown"
