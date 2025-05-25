all:
	gcc main.c indexer.c hashmap.c -o main

.PHONY: clean
clean:
	rm -f main
