PIN=/opt/pin/pin

test:
	for test in 1 2 3; do \
		gcc test$$test.c -o test$$test; \
		${PIN} -ifeellucky -t ../pintool/obj-intel64/pintool.so -o test$$test.log -- ./test$$test; \
		python ../HeapView.py test$$test.log test$$test.svg; \
	done

all: test
