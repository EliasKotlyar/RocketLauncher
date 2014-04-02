
ctlmissile: ctlmissile.o
	$(CC) -o ctlmissile ctlmissile.o -lusb-1.0

clean:
	rm -f ctlmissile ctlmissile.o
