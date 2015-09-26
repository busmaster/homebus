SUBDIRS = addchecksum firmwareupdate modulservice monitor portserver eventmonitor

all:
	for d in $(SUBDIRS); do \
		(cd $$d; make all)  \
	done

.PHONY: clean
clean:
	for d in $(SUBDIRS); do \
		(cd $$d; make clean)  \
	done


