#ifndef INDEXER_H
#define INDEXER_H

#include "hashmap.h"

typedef struct {
    char* filename;
    char* full_content;
} IndexedFile;

typedef struct {
    char* filename;
    HashMap* map;
    HashMap* censored_map;
    IndexedFile* indexed_file;
} ThreadArgs;

void* thread_index_file(void* arg);
void process_sentence(HashMap* map, const char* sentence, HashMap* censored_map, const char* filename);
int index_censored_file(const char* filename);

#endif
