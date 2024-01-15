#!/bin/bash
mysh:	mysh.c
	gcc	-o	mysh.out	mysh.c	-lreadline	-lhistory	
	
run:	mysh
	./mysh.out
all:	run

.PHONY:	run
