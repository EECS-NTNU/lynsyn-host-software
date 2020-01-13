lynsyn_xvc : main.o ../liblynsyn/lynsyn.o
	${LD} $^ ${LDFLAGS} -o $@

%.o : %.c
	${CC} -std=gnu99 ${CFLAGS} -c $< -o $@

%.o : %.cpp
	${CPP} ${CFLAGS} -c $< -o $@

%.o : %.s
	${AS} ${ASFLAGS} -c $< -o $@

%.o : %.S
	${AS} ${ASFLAGS} -c $< -o $@

.PHONY: clean
clean:
	rm -rf *.o lynsyn_sampler
