SUBDIRS = addchecksum firmwareupdate modulservice monitor

all:
	for d in $(SUBDIRS); do \
		(cd $$d; make all)  \
	done

.PHONY: clean
clean:
	for d in $(SUBDIRS); do \
		(cd $$d; make clean)  \
	done


