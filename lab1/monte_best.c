/*
 * intro to OpenMp in Russian
 * http://www.cyberforum.ru/blogs/18334/blog2965.html
 * 
 **/

#include <omp.h>

#include <stdio.h> // printf
#include <stdlib.h> 
#include <time.h>

#define MAX_THREADS 100

typedef struct particle_info 
{
    int b_absorbed;
    int lifetime;
} particle_info;

unsigned int thread_seeds[MAX_THREADS];

int gen_step(int *seed, double p)
{
    double x = rand_r(seed);
    if (1.0 * x / RAND_MAX > p)
        return -1;
    else
        return 1;
}

struct particle_info simulate_particle(int _start_pos, double _p, int _a, int _b)
{
    int current_pos = _start_pos, lifetime = 0;
    while (1)
    {
        if (current_pos <= _a)
            return (struct particle_info) { .b_absorbed = 0, .lifetime = lifetime };
        else if (current_pos >= _b)
            return (struct particle_info) { .b_absorbed = 1, .lifetime = lifetime };
        
        current_pos += gen_step(&thread_seeds[omp_get_thread_num()], _p);
        ++lifetime;
    }
}

// main ----------------------------------------------------------------

int main(int argc, char** argv)
{
    if (argc != 7)
    {
        printf("Not ehough arguments, try better\n");
        return 0;
    }
    
    int a = atoi(argv[1]);
    int b = atoi(argv[2]);
    int x = atoi(argv[3]);
    int N = atoi(argv[4]);
    double p = atof(argv[5]);
    int P = atoi(argv[6]);
    
    omp_set_dynamic(0);
    omp_set_num_threads(P);
    
    int j;
    srand(time(NULL));
    for (j = 0; j < MAX_THREADS; j++)
        thread_seeds[j] = rand();
    
    int b_absorbed = 0;
    long long int summary_lifetime = 0;
    
    
    double start = omp_get_wtime();
    int i;
    #pragma omp parallel for reduction(+: b_absorbed, summary_lifetime)
    for (i = 0; i < N; i++)
    {
        particle_info newresult = simulate_particle(x, p, a, b);
        b_absorbed       += newresult.b_absorbed;
        summary_lifetime += newresult.lifetime;
    }
    double end = omp_get_wtime();
    
    // writing to file
    
    FILE* file = fopen("stats.txt", "a");
    fseek(file, 0, SEEK_END);
    fprintf(file, "%.5f %.5f %.5fs %i %i %i %i %.5f %i\r\n", 
            b_absorbed*1.0 / N, summary_lifetime*1.0 / N, end - start, 
            a, b, x, N, p, P);
    fclose(file);
   
    return 0;
}
