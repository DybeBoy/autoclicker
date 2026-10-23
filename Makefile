PREFIX ?= $(HOME)/.local

autoclicker: autoclicker.c
	$(CC) -O2 -Wall -Wextra -o $@ $<

install: autoclicker
	install -Dm755 autoclicker $(DESTDIR)$(PREFIX)/bin/autoclicker
	@echo "Permissions setup (run once, as root):"
	@echo '  echo '"'"'KERNEL=="uinput", GROUP="input", MODE="0660", OPTIONS+="static_node=uinput"'"'"' > /etc/udev/rules.d/99-autoclicker.rules'
	@echo "  usermod -aG input $$USER && modprobe uinput && udevadm control --reload && udevadm trigger"
	@echo "Then log in again."

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/autoclicker

clean:
	rm -f autoclicker

.PHONY: install uninstall clean
