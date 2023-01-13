

BDIR=build
SDIR=src
TDIR=$(SDIR)/test
CFLAGS=-c -Wall -g -Isrc/include
CRFLAGS=-c -Wall -O3 -Isrc/include
DBG=debug
RLS=release

.PHONY: release
release: $(BDIR)/main.o $(BDIR)/arguments.o
	$(CC) $^ -O3 -o release/cintp
	@cp release/cintp ./cintp

.PHONY: debug
debug: $(BDIR)/main.o $(BDIR)/arguments.o
	$(CC) $^ -g -o debug/cintp
	@cp release/cintp ./cintp

test:
	@num_t=0
	@num_s=0
	@for i in $$(ls $(TDIR)); do						\
		echo "test $$i";						\
		$(CC) $(TDIR)/$$i $(CFLAGS) -o $(BDIR)/$${i%%.c}.o;		\
		$(CC) -g $(BDIR)/$${i%%.c}.o $^ -o $(DBG)/$${i%%.c}.test;	\
		./$(DBG)/$${i%%.c}.test;					\
		[ $? -eq 0 ] 							\
			&& { echo "success"; (( num_s += 1 )); }		\
			|| echo "failure";					\
		(( num_t += 1 ));						\
	done; printf "%i out of %i tests passed.\n" $$num_s $$num_t

$(BDIR)/%.o: $(SDIR)/%.c
	@if [[ $(MAKECMDGOALS) == 'release' ]]; then
		$(CC) $^ $(CRFLAGS) -o $@
	@else
		$(CC) $^ $(CFLAGS) -o $@
	@done

.PHONY: clean
clean:
	$(RM) $(BDIR)/*
	$(RM) $(RLS)/*
	$(RM) $(DBG)/*
	$(RM) cintp
