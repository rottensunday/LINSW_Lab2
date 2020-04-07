CC=$(CROSS_COMPILE)gcc
OBJS := main.o
prog: $(OBJS)
	$(CC) -o prog $(CFLAGS) $(LDFLAGS) $(OBJS) -l gpiod
$(OBJS) : %.o : %.c
	$(CC) -c $(CFLAGS) $< -o $@
