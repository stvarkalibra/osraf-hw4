#ifndef HASHMAP_H
#include <pthread.h>

#define HASHMAP_H
#define MAX_WORD_LEN 64

typedef struct {
    int context_capacity;
    char* filename;
    char** context;
    int occurrence_count;
} WordOccurrence;
typedef struct {
    pthread_mutex_t entry_mutex;
    char* word;
    WordOccurrence* occurrences;
    int occurrence_count;
    int occurrence_capacity;
} HashEntry;
typedef struct {
    int capacity;
    HashEntry** entries;
    int size;
} HashMap;
HashMap* create_hash_map(int capacity);
void free_hash_map(HashMap* map);
int hashmap_contains(HashMap* map, const char* key);
void hashmap_merge(HashMap* dest, HashMap* src);
void hashmap_put(HashMap* map, const char* key, const char* filename, const char* context);
WordOccurrence* get_word_occurrences(HashMap* map, const char* word, int* out_count);
#endif
