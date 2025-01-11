# Variables
EXE=GB-emulator

# Special rules and targets
.PHONY: all build clean help

# Rules and targets
all: build

build:
	@cd src && $(MAKE) -f MakefileLinux.mk
	@cp -f src/$(EXE) ./

# test: build
# 	@cd test && $(MAKE)

clean:
	@cd src && $(MAKE) -f MakefileLinux.mk clean
# @cd test && $(MAKE) -f MakefileLinux.mk clean
	@rm -f $(EXE)

help:
	@echo "Usage:"
	@echo "  make [all]\t\tBuild"
	@echo "  make build\t\tBuild the software"
# @echo "  make test\t\tRun all the tests"
	@echo "  make clean\t\tRemove all files generated by make"
	@echo "  make help\t\tDisplay this help"

# report:
# 	@cd report && $(MAKE)
# 	@cp -f report/$(DOCNAME).pdf ./