#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "hashmap.h"

static unsigned long hash_function(const char* str) {
    unsigned long hash = 5381;
    while (*str) {
        hash = ((hash << 5) + hash) + (unsigned char)(*str);
        str++;
    }
    return hash;
}

HashMap* create_hash_map(int capacity) {
    HashMap* map = (HashMap*)malloc(sizeof(HashMap));
    if (!map) return NULL;

    map->capacity = capacity;
    map->size = 0;
    map->entries = (HashEntry**)calloc(capacity, sizeof(HashEntry*));
    return map;
}

static HashEntry* find_entry(HashMap* map, const char* word) {
    unsigned long base_hash = hash_function(word) % map->capacity;
    int i = 0;
    while (i < map->capacity) {
        int idx = (base_hash + i) % map->capacity;
        HashEntry* entry = map->entries[idx];
        if (!entry) break;
        if (strcmp(entry->word, word) == 0) return entry;
        i++;
    }
    return NULL;
}

static void resize_word_occurrences(WordOccurrence* wo) {
    wo->context_capacity *= 2;
    wo->context = realloc(wo->context, wo->context_capacity * sizeof(char*));
}

static void add_occurrence_to_entry(HashEntry* entry, const char* filename, const char* context) {
    pthread_mutex_lock(&entry->entry_mutex);

    int i = 0;
    while (i < entry->occurrence_count) {
        if (strcmp(entry->occurrences[i].filename, filename) == 0) {
            WordOccurrence* wo = &entry->occurrences[i];
            if (wo->occurrence_count == wo->context_capacity) {
                resize_word_occurrences(wo);
            }
            wo->context[wo->occurrence_count++] = strdup(context);
            pthread_mutex_unlock(&entry->entry_mutex);
            return;
        }
        i++;
    }

    if (entry->occurrence_count == entry->occurrence_capacity) {
        entry->occurrence_capacity = (entry->occurrence_capacity == 0) ? 2 : entry->occurrence_capacity * 2;
        entry->occurrences = realloc(entry->occurrences, entry->occurrence_capacity * sizeof(WordOccurrence));
    }

    WordOccurrence new_occurrence;
    new_occurrence.filename = strdup(filename);
    new_occurrence.occurrence_count = 1;
    new_occurrence.context_capacity = 2;
    new_occurrence.context = (char**)malloc(2 * sizeof(char*));
    new_occurrence.context[0] = strdup(context);

    entry->occurrences[entry->occurrence_count++] = new_occurrence;

    pthread_mutex_unlock(&entry->entry_mutex);
}

void hashmap_put(HashMap* map, const char* key, const char* filename, const char* context) {
    char key_lower[MAX_WORD_LEN];
    int i = 0;
    while (key[i] && i < MAX_WORD_LEN - 1) {
        key_lower[i] = tolower(key[i]);
        i++;
    }
    key_lower[i] = '\0';

    HashEntry* entry = find_entry(map, key_lower);
    if (entry) {
        add_occurrence_to_entry(entry, filename, context);
        return;
    }

    unsigned long base_hash = hash_function(key_lower) % map->capacity;
    int probe = 0;
    while (probe < map->capacity) {
        int idx = (base_hash + probe) % map->capacity;
        if (!map->entries[idx]) {
            HashEntry* new_entry = (HashEntry*)malloc(sizeof(HashEntry));
            new_entry->word = strdup(key_lower);
            new_entry->occurrence_count = 0;
            new_entry->occurrence_capacity = 2;
            new_entry->occurrences = (WordOccurrence*)malloc(2 * sizeof(WordOccurrence));
            pthread_mutex_init(&new_entry->entry_mutex, NULL);

            map->entries[idx] = new_entry;
            map->size++;
            add_occurrence_to_entry(new_entry, filename, context);
            return;
        }
        probe++;
    }
}

WordOccurrence* get_word_occurrences(HashMap* map, const char* word, int* out_count) {
    HashEntry* entry = find_entry(map, word);
    if (!entry) {
        *out_count = 0;
        return NULL;
    }

    pthread_mutex_lock(&entry->entry_mutex);
    *out_count = entry->occurrence_count;
    WordOccurrence* result = entry->occurrences;
    pthread_mutex_unlock(&entry->entry_mutex);

    return result;
}

int hashmap_contains(HashMap* map, const char* key) {
    return find_entry(map, key) != NULL;
}

void hashmap_merge(HashMap* dest, HashMap* src) {
    int i = 0;
    while (i < src->capacity) {
        HashEntry* entry = src->entries[i];
        if (entry) {
            pthread_mutex_lock(&entry->entry_mutex);
            int j = 0;
            while (j < entry->occurrence_count) {
                WordOccurrence* wo = &entry->occurrences[j];
                int k = 0;
                while (k < wo->occurrence_count) {
                    hashmap_put(dest, entry->word, wo->filename, wo->context[k]);
                    k++;
                }
                j++;
            }
            pthread_mutex_unlock(&entry->entry_mutex);
        }
        i++;
    }
}

void free_hash_map(HashMap* map) {
    int i = 0;
    while (i < map->capacity) {
        HashEntry* entry = map->entries[i];
        if (entry) {
            pthread_mutex_destroy(&entry->entry_mutex);
            free(entry->word);

            int j = 0;
            while (j < entry->occurrence_count) {
                WordOccurrence* wo = &entry->occurrences[j];
                free(wo->filename);

                int k = 0;
                while (k < wo->occurrence_count) {
                    free(wo->context[k]);
                    k++;
                }

                free(wo->context);
                j++;
            }

            free(entry->occurrences);
            free(entry);
        }
        i++;
    }
    free(map->entries);
    free(map);
}
