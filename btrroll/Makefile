src = $(wildcard src/*.c) $(wildcard src/dialog/*.c)
obj = $(src:%.c=obj/%.o)

CFLAGS = -Iinclude -Werror=implicit-function-declaration
LDFLAGS = -lbtrfsutil

obj/%.o: %.c
	@mkdir -p '$(@D)'
	$(CC) -c $(CFLAGS) -o $@ $<

btrroll: $(obj)
	@mkdir -p 'bin'
	$(CC) $(LDFLAGS) -o bin/$@ $^

.PHONY: clean
clean:
	rm -f $(obj) btrroll
