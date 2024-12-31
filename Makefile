BOARD=esp32s3
#VERBOSE=1::
CHIP=esp32

ifeq ($(BOARD),esp32s3)
	CDC_ON_BOOT = 1
	UPLOAD_PORT ?= /dev/ttyACM0
endif

GIT_VERSION := "$(shell git describe --abbrev=4 --dirty --always --tags)"
BUILD_EXTRA_FLAGS += -DGIT_VERSION=\"$(GIT_VERSION)\"


csim:       	
	g++ -x c++ -g espLogicAnalyzer.ino -o $@ -DESP32 -DCSIM -DUBUNTU -I./ -I${HOME}/Arduino/lib -I ${HOME}/Arduino/libraries/esp32jimlib/src/ 


include ${HOME}/Arduino/libraries/makeEspArduino/makeEspArduino.mk

fixtty:
	stty -F ${UPLOAD_PORT} -hupcl -crtscts -echo raw 115200 

cat:    fixtty
	cat ${UPLOAD_PORT}

uc:
	make upload && make cat


backtrace:
	tr ' ' '\n' | /home/jim/.arduino15/packages/esp32/tools/xtensa-esp32-elf-gcc/*/bin/xtensa-esp32-elf-addr2line -f -i -e /tmp/mkESP/winglevlr_ttgo-t1/*.elf
        


