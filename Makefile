
# This is convoluted but keeps the 
# overall scheme in place, which is that dom-ws has
# the main makefile for all epxa10 projects.
all:
	cd ../dom-ws; make domapp

clean:
	cd ../dom-ws; make clean

