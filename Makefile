

BDIR=build
SDIR=src
TDIR=$(SDIR)/test
IDIR=$(SDIR)/include
RFLAGS=-c -Wall -O3 -I$(IDIR)
DFLAGS=-c -Wall -g -I$(IDIR)
CFLAGS=$(DFLAGS)
DBG=debug
RLS=release
PROG=cintp
PROG2=text-editor
# add deps here
DEPS=$(BDIR)/main.o $(BDIR)/arguments.o
DEPS2=$(BDIR)/text-editor.o

.PHONY: all
all: clean debug test release
	@echo 'done.'

.PHONY: dirs
.PHONY: release
release: CFLAGS=$(RFLAGS)
release: MODE=release
release: dirs $(DEPS) $(DEPS2)
	@echo 'linking release build...'
# 	you could make this a for loop
	@$(CC) $(DEPS) -O3 -o $(RLS)/$(PROG)
	@cp $(RLS)/$(PROG) ./$(PROG)
	@$(CC) $(DEPS2) -O3 -o $(RLS)/$(PROG2)
	@cp $(RLS)/$(PROG2) ./$(PROG2)

.PHONY: debug
debug: CFLAGS=$(DFLAGS)
debug: MODE=debug
debug: dirs $(DEPS) $(DEPS2)
	@echo 'linking debug build...'
	@$(CC) $(DEPS) -g -o $(DBG)/$(PROG)
	@cp $(DBG)/$(PROG) ./$(PROG)
	@$(CC) $(DEPS2) -g -o $(DBG)/$(PROG2)
	@cp $(DBG)/$(PROG2) ./$(PROG2)

.PHONY: test
test: CFLAGS=$(DFLAGS)
test: MODE=debug
test: dirs
	@echo 'testing...';										\
	rm test.log;											\
	num_t=0;											\
	num_s=0;											\
	for i in $$(ls $(TDIR)); do									\
		if [ "$$i" != "skip" ]; then								\
			echo "====test $$i" | tee -a test.log;						\
			num_t=$$(( num_t + 1 ));							\
			$(CC) $(TDIR)/$$i $(CFLAGS) -o $(BDIR)/$${i%%.c}.o 2>&1 | tee -a test.log;	\
			$(CC) -g $(BDIR)/$${i%%.c}.o -o $(DBG)/$${i%%.c}.test 2>&1 | tee -a test.log;	\
			[ -f "$(DBG)/$${i%%.c}.test" ] || 						\
				{ echo "Compilation failed--file doesn't exist" | tee -a test.log; 	\
				sh -c 'exit 1'; };							\
			[ -f "$(DBG)/$${i%%.c}.test" ] && 						\
				{ ./$(DBG)/$${i%%.c}.test 2>&1 >> test.log; ./$(DBG)/$${i%%.c}.test; };	\
			[ "$$?" -eq 0 ] 								\
				&& { echo "Test succeeded" | tee -a test.log; 				\
				num_s=$$(( num_s + 1 )); }						\
				|| echo "Test failed" | tee -a test.log;				\
		fi											\
	done; echo "====finished" | tee -a test.log; 							\
	printf "%i out of %i tests passed.\n" $$num_s $$num_t | tee -a test.log;

$(BDIR)/%.o: $(SDIR)/%.c
	@echo 'building: '$^' mode: $(MODE)'
	@$(CC) $^ $(CFLAGS) -o $@

.PHONY: clean
clean: dirs
	@echo "cleaning..."
	@$(RM) $(BDIR)/*
	@$(RM) $(RLS)/*
	@$(RM) $(DBG)/*
	@$(RM) test.log
	@$(RM) cintp

# make sure all dirs exist before
dirs:
	@mkdir -p $(SDIR) &> /dev/null
	@mkdir -p $(BDIR) &> /dev/null
	@mkdir -p $(TDIR) &> /dev/null
	@mkdir -p $(IDIR) &> /dev/null
	@mkdir -p $(DBG) &> /dev/null
	@mkdir -p $(RLS) &> /dev/null
