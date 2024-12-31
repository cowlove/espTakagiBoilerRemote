BOARD=esp32s3
#VERBOSE=1::
CHIP=esp32

ifeq ($(BOARD),esp32s3)
	CDC_ON_BOOT = 1
	UPLOAD_PORT ?= /dev/ttyACM0
endif

GIT_VERSION := "$(shell git describe --abbrev=4 --dirty --always --tags)"
BUILD_EXTRA_FLAGS += -DGIT_VERSION=\"$(GIT_VERSION)\"


include ${HOME}/Arduino/libraries/makeEspArduino/makeEspArduino.mk

