DIRS = simple call vdso strchr float argv dlopen time hellocpp thread params uint demo

define runall
	@for d in $(DIRS); do \
	  echo -e "\n==================================================="; \
	  echo "[*] $1 $$d"; \
	  $(MAKE) --no-print-directory -C $$d $2; \
	done
endef

all:
	$(call runall,"Building")

run:
	$(call runall,"Testing",run)

clean:
	$(call runall,"Cleaning",clean)

.PHONY: all test clean


