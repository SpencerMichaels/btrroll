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

install: btrroll
	install -Dm0755 bin/btrroll "${DESTDIR}/usr/bin/btrroll"
	install -Dm0755 etc/btrroll.hook "${DESTDIR}/usr/lib/initcpio/hooks/btrroll"
	install -Dm0755 etc/btrroll.install "${DESTDIR}/usr/lib/initcpio/install/btrroll"
	install -Dm0644 etc/btrroll.service "${DESTDIR}/usr/lib/systemd/system/btrroll.service"
