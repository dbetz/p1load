p1load:	p1load.c ploader.c osint_linux.c
	cc -Wall -DMACOSX -o $@ p1load.c ploader.c osint_linux.c

run:	p1load
	./p1load blink.binary -r -t

clean:
	rm -rf p1load
