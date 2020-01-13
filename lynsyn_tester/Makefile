lynsyn_tester : main.o ../liblynsyn/lynsyn.o
	${LD} $^ ${LDFLAGS} -o $@

%.o : %.c
	${CC} ${CFLAGS} -c $< -o $@

%.o : %.cpp
	${CPP} ${CXXFLAGS} -c $< -o $@

%.o : %.s
	${AS} ${ASFLAGS} -c $< -o $@

%.o : %.S
	${AS} ${ASFLAGS} -c $< -o $@

.PHONY: clean
clean:
	rm -rf *.o lynsyn_tester
