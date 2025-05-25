#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>
#include "hashmap.h"
#include "indexer.h"


#define MAX_INDEXED_FILES 256
#define MAX_FILENAME_LEN 256


HashMap* global_map;
HashMap* censored_map;
pthread_mutex_t merge_mutex;


int index_file_interactive(const char* filename);
void index_censored();
void search_word(const char* word);
IndexedFile indexed_files_data[MAX_INDEXED_FILES];
char indexed_files[MAX_INDEXED_FILES][MAX_FILENAME_LEN];
int indexed_files_count = 0;


int is_censored_context(const char* context) {
    if (!censored_map) return 0;

    char* copy = strdup(context);
    char* token = strtok(copy, " ,.-!?;:\n\t");
    int found = 0;

    while (token) {
        for (char* p = token; *p; p++) *p = tolower(*p);
        if (hashmap_contains(censored_map, token)) {
            found = 1;
            break;
        }
        token = strtok(NULL, " ,.-!?;:\n\t");
    }

    free(copy);
    return found;
}

int compare_occurrences(const void* a, const void* b) {
    const WordOccurrence* wa = (const WordOccurrence*)a;
    const WordOccurrence* wb = (const WordOccurrence*)b;
    return wb->occurrence_count - wa->occurrence_count;
}

int is_file_indexed(const char* filename) {
    for (int i = 0; i < indexed_files_count; i++) {
        if (strcmp(indexed_files[i], filename) == 0) {
            return 1;
        }
    }
    return 0;
}

void add_indexed_file(const char* filename) {
    if (indexed_files_count < MAX_INDEXED_FILES) {
        strncpy(indexed_files[indexed_files_count], filename, MAX_FILENAME_LEN - 1);
        indexed_files[indexed_files_count][MAX_FILENAME_LEN - 1] = '\0';
        indexed_files_count++;
    } else {
        printf("Greska: dostignut maksimalan broj indeksiranih fajlova.\n");
    }
}

void* thread_worker(void* arg) {
    ThreadArgs* args = (ThreadArgs*)arg;
    thread_index_file(arg);

    pthread_mutex_lock(&merge_mutex);
    hashmap_merge(global_map, args->map);
    pthread_mutex_unlock(&merge_mutex);

    free_hash_map(args->map);
    free(args);
    return NULL;
}

int main(int argc, char* argv[]) {
    char line[1024];

    global_map = create_hash_map(32768);
    censored_map = create_hash_map(512);
    pthread_mutex_init(&merge_mutex, NULL);

    if (argc > 1) {
        if (index_censored_file(argv[1]) != 0) {
            fprintf(stderr, "Greska pri ucitavanju cenzurisanih reci iz %s\n", argv[1]);
        } else {
            printf("Cenzurisane reci ucitane iz '%s'.\n", argv[1]);
        }
    }

    printf("Dobrodosli u pretrazivac.\n");
    printf("Komande:\n");
    printf("  index <fajl>       - indeksiranje\n");
    printf("  search <rec>       - pretraga\n");
    printf("  stop               - izlaz\n");

    while (1) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\r\n")] = 0;

        if (strncmp(line, "index ", 6) == 0) {
            const char* filename = line + 6;
            if (is_file_indexed(filename)) {
                printf("Fajl je vec indeksiran.\n");
                continue;
            }

            pthread_t tid;
            ThreadArgs* args = malloc(sizeof(ThreadArgs));
            args->filename = strdup(filename);
            args->map = create_hash_map(8192);
            args->censored_map = censored_map;
            args->indexed_file = &indexed_files_data[indexed_files_count];

            if (pthread_create(&tid, NULL, thread_worker, args) != 0) {
                perror("Greska pri kreiranju niti");
                free(args);
                continue;
            }

            pthread_join(tid, NULL);
            add_indexed_file(filename);
            printf("Indeksiranje zavrseno za '%s'\n", filename);
        }else if (strncmp(line, "search ", 7) == 0) {
            search_word(line + 7);
        } else if (strcmp(line, "stop") == 0) {
            break;
        } else {
            printf("Nepoznata komanda.\n");
        }
    }

    free_hash_map(global_map);
    free_hash_map(censored_map);
    pthread_mutex_destroy(&merge_mutex);
    return 0;
}

void remove_censored_contexts() {
    for (int i = 0; i < global_map->capacity; i++) {
        HashEntry* entry = global_map->entries[i];
        if (!entry) continue;

        for (int j = 0; j < entry->occurrence_count; j++) {
            WordOccurrence* wo = &entry->occurrences[j];
            int write_index = 0;
            for (int k = 0; k < wo->occurrence_count; k++) {
                if (!is_censored_context(wo->context[k])) {
                    wo->context[write_index++] = wo->context[k];
                } else {
                    free(wo->context[k]);
                }
            }
            wo->occurrence_count = write_index;
        }
    }
}

void search_word(const char* word) {
    if (hashmap_contains(censored_map, word)) {
        printf("Rec %s je cenzurisana\n", word);
        return;
    }

    int count = 0;
    WordOccurrence* occurrences = get_word_occurrences(global_map, word, &count);
    if (count == 0 || occurrences == NULL) {
        printf("Rec '%s' nije pronadjena u indeksu.\n", word);
    } else {
        qsort(occurrences, count, sizeof(WordOccurrence), compare_occurrences);
        for (int i = 0; i < count; i++) {
            printf("  U fajlu: %s, %d puta\n", occurrences[i].filename, occurrences[i].occurrence_count);
            for (int j = 0; j < occurrences[i].occurrence_count; j++) {
                printf("    -%s\n", occurrences[i].context[j]);
            }
        }
    }
}


