#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

int m; // max chunk size
int P; // thread number

typedef struct thread_param_chunksort
{
    int *data;
    int num_chunks;
    int startpos;
} thread_param_chunksort;

typedef struct thread_param_merge
{
    int *data;
    int startpos;
    int seppos;
    int endpos;
} thread_param_merge;

void merge_neighbour_segments(int *data, int startpos, int seppos, int endpos)
{
    int n1 = seppos - startpos;
    int n2 = endpos - seppos;
    int *buffer1 = malloc(n1 * sizeof(int));
    int *buffer2 = malloc(n2 * sizeof(int));

    memcpy(buffer1, &data[startpos], n1 * sizeof(int));
    memcpy(buffer2, &data[seppos], n2 * sizeof(int));

    int i = 0;
    int j = 0;
    while (i < n1 && j < n2)
    {
        if (buffer1[i] < buffer2[j])
        {
            data[startpos + i + j] = buffer1[i];
            i++;
        }
        else
        {
            data[startpos + i + j] = buffer2[j];
            j++;
        }
    }

    while (i < n1)
    {
        data[startpos + i + j] = buffer1[i];
        i++;
    }
    while (j < n2)
    {
        data[startpos + i + j] = buffer2[j];
        j++;
    }

    free(buffer1);
    free(buffer2);
}

// for qsort
int compare(const void *a, const void *b)
{
    return ( *(int*)a - *(int*)b );
}

void * thread_chunksort_routine(void *th_params)
{
    thread_param_chunksort *params = (thread_param_chunksort*) th_params;
    int index = params->startpos;
    for (int i = 0; i < params->num_chunks; i++)
    {
        qsort(&params->data[index], m, sizeof(int), compare);
        index += m;
    }
    return 0;
}

void * thread_merge_routine(void *th_params)
{
    thread_param_merge *params = (thread_param_merge*) th_params;
    merge_neighbour_segments(params->data, params->startpos, params->seppos,
                             params->endpos);
    return 0;
}

void chunksort(int *data, int n)
{
    pthread_t* threads = (pthread_t*) malloc(P * sizeof(pthread_t));
    thread_param_chunksort* thread_params = malloc(P * sizeof(thread_param_chunksort));
    if (threads == NULL || thread_params == NULL)
    {
        printf("Alloc error\n");
        exit(1);
    }

    int total_chunks = n / m;
    int chunks_per_thread = total_chunks / P;

    // creating threads
    int index = 0;
    for (int i = 0; i < P; i++)
    {
        thread_params[i] = (thread_param_chunksort) {data, chunks_per_thread, index};
        pthread_create(&(threads[i]), NULL, thread_chunksort_routine, &thread_params[i]);
        index += m * chunks_per_thread;
    }
    // tail
    qsort(&data[index], n - index, sizeof(int), compare);
    for(int i = 0; i < P; i++)
        pthread_join(threads[i], NULL);

    free(threads);
    free(thread_params);
}

void th_mergesort(int *data, int n)
{
    chunksort(data, n);

    pthread_t* threads = malloc(P * sizeof(pthread_t));
    thread_param_merge* thread_params = malloc(P * sizeof(thread_param_merge));
    if (threads == NULL || thread_params == NULL)
    {
        printf("Alloc error\n");
        exit(1);
    }

    for (int subsize = m; subsize < n; subsize *= 2)
    {
        int index = 0;
        int num_threads = P;
        while (index + subsize + 1 < n)
        {
            for (int i = 0; i < P; i++)
            {
                if (index + subsize - 1 >= n)
                {
                    num_threads = i;
                    break;
                }
                int r2 = index + 2 * subsize;
                if (r2 > n)
                    r2 = n;

                thread_params[i] = (thread_param_merge) {data, index, index + subsize, r2};
                index = r2;
                pthread_create(&(threads[i]), NULL, thread_merge_routine, &thread_params[i]);
            }

            for (int i = 0; i < num_threads; ++i)
                pthread_join(threads[i], NULL);
        }
    }

    free(threads);
    free(thread_params);
}

void generate_2_arrays(int **first, int **second, int size)
{
    *first  = (int *) malloc(sizeof(int) * size);
    *second = (int *) malloc(sizeof(int) * size);
    if (*first == NULL || *second == NULL)
    {
        printf("Alloc error\n");
        exit(1);
    }

    srand(time(NULL));
    for (int i = 0; i < size; i++)
        (*first)[i] = (*second)[i] = rand();
}

long timetick()
{
    struct timeval timecheck;
    gettimeofday(&timecheck, NULL);
    return (long)timecheck.tv_sec * 1000 + (long)timecheck.tv_usec / 1000;
}

int main(int argc, char** argv)
{
    int n = atoi(argv[1]); // data size
    m = atoi(argv[2]); // max chunk size
    P = atoi(argv[3]); // thread number

    // GENERATING DATA ---------------------------------------------------------

    int *data_testqs, *data_testms;
    generate_2_arrays(&data_testqs, &data_testms, n);

    // PROCESSING MERGESORT ----------------------------------------------------

    FILE* file = fopen("data.txt", "w");
    for (int i = 0; i < n; i++)
        fprintf(file, "%d ", data_testms[i]);
    fprintf(file, "\n");

    long start = timetick();
    th_mergesort(data_testms, n);
    long time_ms = timetick() - start;
    printf("MS time: %ld ms\n", time_ms);

    for (int i = 0; i < n; i++)
        fprintf(file, "%d ", data_testms[i]);
    fprintf(file, "\n\n");
    fclose(file);

    // PROCESSING QUICKSORT ----------------------------------------------------

    start = timetick();
    qsort(&data_testqs[0], n, sizeof(int), compare);
    long time_qs = timetick() - start;
    printf("QS time: %ld ms\n", time_qs);

    // VERIFYING ---------------------------------------------------------------

    if (memcmp((char *)data_testms, (char *)data_testqs, n) != 0)
    {
        printf("Incorrect reuslts of sorting, exiting...\n");
        exit(2);
    }

    file = fopen("stats.txt", "a");
    fseek(file, 0, SEEK_END);
    fprintf(file, "%ld %ld %d %d %d\n", time_ms, time_qs, n, m, P);
    fclose(file);

    free(data_testqs);
    free(data_testms);

    return 0;
}
