#
# Build Cachelot alpine-based devbox and release Docker container
#

DEVBOX_IMAGE = cachelot/alpine-devbox
DEVBOX_TAG := latest
RELEASE_NAME ?= cachelot/cachelot
RELEASE_TAG ?= latest
PWD := $(shell pwd)
VOL_PATH := "$(PWD)/artifacts"

all: alpine-devbox cachelotd


alpine-devbox:
	docker build -t "$(DEVBOX_IMAGE):$(DEVBOX_TAG)" "$(PWD)"

enter-devbox:
	docker run -i -t $(DEVBOX_IMAGE) /bin/bash

DEVBOX_ID := $(shell docker ps -l | grep 'cachelot/alpine-devbox' | cut -d' ' -f1)
dumb-init:
	docker run $(DEVBOX_IMAGE) /bin/true
	docker cp "$(DEVBOX_ID):/usr/bin/dumb-init" "dumb-init"

cachelotd:
	rm -rf $(VOL_PATH); mkdir $(VOL_PATH)
	docker run -i -v $(VOL_PATH):/cachelot "$(DEVBOX_IMAGE)" < build_cachelotd.sh
	cp -f -- "$(VOL_PATH)/cachelotd" cachelotd

TEMPDIR := $(shell mktemp -d -u -t cachelot-docker-XXXXXXX)
ALL_TEMPDIRS := "$(dir $(TEMPDIR))cachelot-docker-*"
release: cachelotd dumb-init
	mkdir "$(TEMPDIR)"
	cp -- release.Dockerfile "$(TEMPDIR)/Dockerfile"
	cp -- cachelotd "$(TEMPDIR)/cachelotd"
	cp -- dumb-init "$(TEMPDIR)/dumb-init"
	docker build --no-cache=true -t "$(RELEASE_NAME):$(RELEASE_TAG)" "$(TEMPDIR)"
	rm -rf -- "$(TEMPDIR)"

clean:
	rm -f cachelotd
	rm -f dumb-init
	rm -rf "$(VOL_PATH)"
	rm -rf "$(ALL_TEMPDIRS)"

.PHONY: all alpine-devbox enter-devbox release
