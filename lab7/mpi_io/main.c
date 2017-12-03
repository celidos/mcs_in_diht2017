#include <stdlib.h>
#include <time.h>
#include <mpi.h>
#include <string.h>
#include <stdio.h>

typedef struct
{
    int rank;
    int size;

    int l;
    int a;
    int b;
    int N;
} mpi_routine_params;

typedef struct
{
    int x;
    int y;
    int r;
} ParticleInfo;

void solve_task(mpi_routine_params task_params)
{
    // SEED DISTRIBUTION -------------------------------------------------------

    ParticleInfo *particles = (ParticleInfo*)malloc(sizeof(ParticleInfo) * task_params.N);
    int seed;
    int *seeds = (int*)malloc(task_params.size * sizeof(int));
    if (task_params.rank == 0)
    {
        srand(time(NULL));
        for (int i = 0; i < task_params.size; ++i)
            seeds[i] = rand();
    }

    // sending seeds to all over the world
    MPI_Scatter(seeds, 1, MPI_UNSIGNED, &seed, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
    free(seeds);

    int l = task_params.l;
    for (int i = 0; i < task_params.N; ++i)
    {
        particles[i].r = rand_r(&seed) % (task_params.a * task_params.b);
        particles[i].x = rand_r(&seed) % l;
        particles[i].y = rand_r(&seed) % l;
    }

    // FIELD OF VECTORS --------------------------------------------------------

    int size = l * l * task_params.size;
    int *cast = (int *)malloc(size * sizeof(int));
    memset(cast, (char)0, size * sizeof(int));

    for (int i = 0; i < task_params.N; ++i)
        ++cast[particles[i].y * l * task_params.size + particles[i].x * task_params.size + particles[i].r];

    MPI_Datatype view;
    MPI_Type_vector(l, l * task_params.size, l * task_params.a * task_params.size, MPI_INT, &view);
    MPI_Type_commit(&view);

    //printf("seed %d: (%d, %d)\n", task_params.seed,
    //    (task_params.rank % task_params.a), (task_params.rank / task_params.a));

    MPI_File file;
    MPI_File_delete("data.bin", MPI_INFO_NULL);
    MPI_File_open(MPI_COMM_WORLD, "data.bin", MPI_MODE_CREATE | MPI_MODE_WRONLY,
                  MPI_INFO_NULL, &file);

    // starting with offset
    MPI_File_set_view(file, task_params.size *
                      ((task_params.rank % task_params.a) * task_params.l +
                       (task_params.rank / task_params.a) * task_params.a
                       * task_params.l * task_params.l) * sizeof(int),
                      MPI_INT, view, "native", MPI_INFO_NULL);

    //printf("seed %d WE ARE HERE fuck not working!\n", task_params.rank);
    MPI_File_write(file, cast, size, MPI_INT, MPI_STATUS_IGNORE);
    MPI_Type_free(&view);
    MPI_File_close(&file);

    free(cast);
    free(particles);
}

long timetick() // in usec
{
    struct timeval timecheck;
    gettimeofday(&timecheck, NULL);
    return (long)timecheck.tv_sec * 1000000 + (long)timecheck.tv_usec;
}

int main(int argc, char * argv[])
{
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // printf("ok! mpi inited");
    mpi_routine_params setting;

    setting.rank = rank;
    setting.size = size;
    setting.l = atoi(argv[1]);
    setting.a = atoi(argv[2]);
    setting.b = atoi(argv[3]);
    setting.N = atoi(argv[4]);

    long start = timetick();
    solve_task(setting);
    long full_time = timetick() - start;

    // WRITING TO FILE ---------------------------------------------------------
    // if we are in master
    if (setting.rank == 0)
    {
        FILE *file = fopen("stats.txt", "w");
        fprintf(file, "%d %d %d %d %ld\n",
                setting.l,
                setting.a,
                setting.b,
                setting.N,
                full_time);
        fclose(file);
    }

    MPI_Finalize();
    return EXIT_SUCCESS;
}
