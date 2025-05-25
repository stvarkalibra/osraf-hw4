#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "indexer.h"

extern HashMap* censored_map;

int sentence_contains_censored(const char* sentence, HashMap* censored_map) {
    if (!sentence || !censored_map) return 0;

    char* copy = strdup(sentence);
    if (!copy) return 0;

    char* token = strtok(copy, " ,.-!?;:\n\t");
    while (token != NULL) {
        char* p = token;
        while (*p) {
            *p = tolower(*p);
            p++;
        }

        if (hashmap_contains(censored_map, token)) {
            free(copy);
            return 1;
        }

        token = strtok(NULL, " ,.-!?;:\n\t");
    }

    free(copy);
    return 0;
}

void process_sentence(HashMap* map, const char* sentence, HashMap* censored_map, const char* filename) {
    if (!sentence || !map || !filename) return;

    if (censored_map && sentence_contains_censored(sentence, censored_map)) return;

    char* copy = strdup(sentence);
    if (!copy) return;

    char* token = strtok(copy, " ,.-!?;:\n\t");
    while (token != NULL) {
        char* p = token;
        while (*p) {
            *p = tolower(*p);
            p++;
        }

        hashmap_put(map, token, filename, sentence);
        token = strtok(NULL, " ,.-!?;:\n\t");
    }

    free(copy);
}

void *thread_index_file(void *arg) {
    if (!arg) return NULL;

    ThreadArgs *args = (ThreadArgs *)arg;
    FILE *fp = fopen(args->filename, "r");
    if (!fp) {
        perror("fopen");
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long length = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    args->indexed_file->full_content = (char*)malloc(length + 1);
    if (!args->indexed_file->full_content) {
        fclose(fp);
        return NULL;
    }

    fread(args->indexed_file->full_content, 1, length, fp);
    args->indexed_file->full_content[length] = '\0';
    fclose(fp);

    args->indexed_file->filename = strdup(args->filename);

    char *saveptr = NULL;
    char *sentence = strtok_r(args->indexed_file->full_content, ".!?\n", &saveptr);
    while (sentence != NULL) {
        process_sentence(args->map, sentence, args->censored_map, args->filename);
        sentence = strtok_r(NULL, ".!?\n", &saveptr);
    }

    return NULL;
}

int index_censored_file(const char* filename) {
    if (!filename) return -1;

    IndexedFile indexed_file_local;
    ThreadArgs args;

    args.filename = (char*)filename;
    args.map = create_hash_map(1024);
    args.censored_map = censored_map;
    args.indexed_file = &indexed_file_local;

    thread_index_file(&args);

    hashmap_merge(censored_map, args.map);
    free_hash_map(args.map);

    free(indexed_file_local.filename);
    free(indexed_file_local.full_content);

    return 0;
}
