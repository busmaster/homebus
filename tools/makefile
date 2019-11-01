SUBDIRS = addchecksum firmwareupdate modulservice monitor portserver eventmonitor eventmonitorweb

all:
	for d in $(SUBDIRS); do \
		(cd $$d; make all)  \
	done

.PHONY: clean
clean:
	for d in $(SUBDIRS); do \
		(cd $$d; make clean)  \
	done


