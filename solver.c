/* Code for the equation solver. 
 * Author: Naga Kandasamy, 10/20/2012
 * Date last modified: 10/11/2015
 *
 * Compile as follows:
 * gcc -o solver solver.c solver_gold.c -std=c99 -lm -lpthread 
 */

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <pthread.h>
#include <math.h>
#include <semaphore.h>
#include "grid.h" 


extern int compute_gold(GRID_STRUCT *);
int compute_using_pthreads_jacobi(GRID_STRUCT *);
int compute_using_pthreads_red_black(GRID_STRUCT *);
void compute_grid_differences(GRID_STRUCT *, GRID_STRUCT *, GRID_STRUCT *);
void *func_a(void *);
void *func_b(void *);




float diff_jac =0;
float diff_rb =0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
GRID_STRUCT *grid_2_jacobi;

#define NUM_THREADS 2 

typedef struct barrier_struct_tag{
    sem_t counter_sem; /* Protects access to the counter. */
    sem_t barrier_sem; /* Signals that barrier is safe to cross. */
    int counter; /* The value itself. */
} barrier_t;

barrier_t barrier; 
void barrier_sync(barrier_t *, int);


/* This function prints the grid on the screen. */
void 
display_grid(GRID_STRUCT *my_grid)
{
    for(int i = 0; i < my_grid->dimension; i++)
        for(int j = 0; j < my_grid->dimension; j++)
            printf("%f \t", my_grid->element[i * my_grid->dimension + j]);
   		
    printf("\n");
}


/* Print out statistics for the converged values, including min, max, and average. */
void 
print_statistics(GRID_STRUCT *my_grid)
{
    float min = INFINITY;
    float max = 0.0;
    double sum = 0.0; 
    
    for(int i = 0; i < my_grid->dimension; i++){
        for(int j = 0; j < my_grid->dimension; j++){
            sum += my_grid->element[i * my_grid->dimension + j];
           
            if(my_grid->element[i * my_grid->dimension + j] > max) 
                max = my_grid->element[i * my_grid->dimension + j];

				if(my_grid->element[i * my_grid->dimension + j] < min) 
                    min = my_grid->element[i * my_grid->dimension + j];
				 
        }
    }

    printf("AVG: %f \n", sum/(float)my_grid->num_elements);
	printf("MIN: %f \n", min);
	printf("MAX: %f \n", max);

	printf("\n");
}

/* Calculate the differences between grid elements for the various implementations. */
void compute_grid_differences(GRID_STRUCT *grid_1, GRID_STRUCT *grid_2, GRID_STRUCT *grid_3)
{
    float diff_12, diff_13;
    int dimension = grid_1->dimension;
    int num_elements = dimension*dimension;

    diff_12 = 0.0;
    diff_13 = 0.0;
    for(int i = 0; i < grid_1->dimension; i++){
        for(int j = 0; j < grid_1->dimension; j++){
            diff_12 += fabsf(grid_1->element[i * dimension + j] - grid_2->element[i * dimension + j]);
            diff_13 += fabsf(grid_1->element[i * dimension + j] - grid_3->element[i * dimension + j]);
        }
    }
    printf("Average difference in grid elements for Gauss Seidel and Red-Black methods = %f. \n", \
            diff_12/num_elements);

    printf("Average difference in grid elements for Gauss Seidel and Jacobi methods = %f. \n", \
            diff_13/num_elements);


}

/* Create a grid of random floating point values bounded by UPPER_BOUND_ON_GRID_VALUE */
void 
create_grids(GRID_STRUCT *grid_1, GRID_STRUCT *grid_2, GRID_STRUCT *grid_3)
{
	printf("Creating a grid of dimension %d x %d. \n", grid_1->dimension, grid_1->dimension);
	grid_1->element = (float *)malloc(sizeof(float) * grid_1->num_elements);
	grid_2->element = (float *)malloc(sizeof(float) * grid_2->num_elements);
	grid_3->element = (float *)malloc(sizeof(float) * grid_3->num_elements);

	srand((unsigned)time(NULL));
	
	float val;
	for(int i = 0; i < grid_1->dimension; i++)
		for(int j = 0; j < grid_1->dimension; j++){
			val =  ((float)rand()/(float)RAND_MAX) * UPPER_BOUND_ON_GRID_VALUE;
			grid_1->element[i * grid_1->dimension + j] = val; 	
			grid_2->element[i * grid_2->dimension + j] = val; 
			grid_3->element[i * grid_3->dimension + j] = val; 
			
		}
}

/* Edit this function to use the jacobi method of solving the equation. The final result should 
 * be placed in the grid_2 data structure */
int 
compute_using_pthreads_jacobi(GRID_STRUCT *grid_2)
{
	int num_iter_jac = 0;
	int done_jac = 0;
	pthread_t worker_thread[NUM_THREADS];
	GRID_STRUCT *grid_2_thread;
	//GRID_STRUCT *grid_3 = (GRID_STRUCT *)grid_3;
	barrier.counter = 0;
    	sem_init(&barrier.counter_sem, 0, 1); 
    	sem_init(&barrier.barrier_sem, 0, 0); 
	
	grid_2_jacobi = (GRID_STRUCT *)malloc(sizeof(GRID_STRUCT));
	grid_2_jacobi->dimension = grid_2->dimension;
	grid_2_jacobi->element = grid_2->element;
	int i;
	//printf("manik");
	while(!done_jac)
	{
	 
	 for(i=0;i<NUM_THREADS;i++)
	 {
		
	 	grid_2_thread = (GRID_STRUCT *)malloc(sizeof(GRID_STRUCT));
		grid_2_thread->dimension = grid_2->dimension;
		grid_2_thread->dimension_end = ((grid_2->dimension)*(i+1)/NUM_THREADS);
		grid_2_thread->dimension_start = ((grid_2->dimension)*(i))/NUM_THREADS;
		grid_2_thread->element = grid_2->element;
		grid_2_thread->thread_id = i;
		grid_2_thread->diff = 0;
		grid_2_thread->temp = 0;
		if((pthread_create(&worker_thread[i], NULL, func_b, (void *)grid_2_thread)) != 0)
		{
            		printf("Cannot create thread \n");
            		exit(0);
		}
		
	 }	
	
		for(i = 0; i < NUM_THREADS; i++)
		{
			pthread_join(worker_thread[i], NULL);
		}
		
		num_iter_jac++;
		printf("Iteration %d. Diff: %f. \n", num_iter_jac, diff_jac);
		 if((float)diff_jac/((float)(grid_2->dimension*grid_2->dimension)) < (float)TOLERANCE) 
            	done_jac = 1;
		
	}

	return num_iter_jac;
			
}

void *func_b(void *arg)
{
	GRID_STRUCT *my_grid = (GRID_STRUCT *)arg;
	int i,j;
	
	  //diff_jac = 0;
	 for(i = my_grid->dimension_start + 1; i < (my_grid->dimension_end-1); i++)
	 {
            for(j = 1; j < (my_grid->dimension-1); j++)
	    {
                my_grid->temp = grid_2_jacobi->element[i * my_grid->dimension + j];
               	
                grid_2_jacobi->element[i * my_grid->dimension + j] = \
                               0.20*(my_grid->element[i * my_grid->dimension + j] + \
                                       my_grid->element[(i - 1) * my_grid->dimension + j] +\
                                       my_grid->element[(i + 1) * my_grid->dimension + j] +\
                                       my_grid->element[i * my_grid->dimension + (j + 1)] +\
                                       my_grid->element[i * my_grid->dimension + (j - 1)]);
		
                my_grid->diff += fabs(grid_2_jacobi->element[i * my_grid->dimension + j] - my_grid->temp); 
		
	    }
         }

	barrier_sync(&barrier, my_grid->thread_id);

	for(i = my_grid->dimension_start + 1; i < (my_grid->dimension_end-1); i++)
	{
		for(j = 1; j < (my_grid->dimension-1); j++)
		{
			my_grid->element[i * my_grid->dimension + j] = grid_2_jacobi->element[i * my_grid->dimension + j];
		}
	}

	//barrier_sync(&barrier, my_grid->thread_id);

	pthread_mutex_lock(&mutex);
	diff_jac = my_grid->diff;
	pthread_mutex_unlock(&mutex);
	
        pthread_exit(NULL);
	//display_grid(my_grid);		  
       
		
	
}

/* Edit this function to use the red-black method of solving the equation. The final result 
 * should be placed in the grid_3 data structure */
int 
compute_using_pthreads_red_black(GRID_STRUCT *grid_3)
{
	int num_iter_th = 0;
	int done_rb = 0;
	pthread_t worker_thread[NUM_THREADS];
	GRID_STRUCT *grid_3_thread;
	
	barrier.counter = 0;
    	sem_init(&barrier.counter_sem, 0, 1); 
    	sem_init(&barrier.barrier_sem, 0, 0); 

	
	int i;
	while(!done_rb){
	 for(i=0;i<NUM_THREADS;i++)
	 {
		grid_3_thread = (GRID_STRUCT *)malloc(sizeof(GRID_STRUCT));
		grid_3_thread->dimension = grid_3->dimension;
		grid_3_thread->dimension_end = ((grid_3->dimension)*(i+1)/NUM_THREADS);
		grid_3_thread->dimension_start = ((grid_3->dimension)*(i))/NUM_THREADS;
		grid_3_thread->element = grid_3->element;
		grid_3_thread->thread_id = i;
		grid_3_thread->diff = 0;
		grid_3_thread->temp = 0;
		if((pthread_create(&worker_thread[i], NULL, func_a, (void *)grid_3_thread)) != 0)
		{
            		printf("Cannot create thread \n");
            		exit(0);
		}
		
	 }	

		for(i = 0; i < NUM_THREADS; i++)
		{
			pthread_join(worker_thread[i], NULL);
		}
		num_iter_th++;
		printf("Iteration %d. Diff: %f. \n", num_iter_th, diff_rb);
		if((float)diff_rb/((float)(grid_3->dimension*grid_3->dimension)) < (float)TOLERANCE) 
            	done_rb = 1;
	}
	
	
	
	//}	

	 return num_iter_th;
}

void *func_a(void *arg)
{
	GRID_STRUCT *my_grid = (GRID_STRUCT *)arg;
	int i,j;
	float temp;
	//diff=0;
	
       
	
	for(i = my_grid->dimension_start + 1; i < (my_grid->dimension_end-1); i++)
	{
            for(int j = 1; j < (my_grid->dimension-1); j+=2)
	    {
		
		
                my_grid->temp = my_grid->element[i * my_grid->dimension + j];
                /* Apply the update rule. */	
                my_grid->element[i * my_grid->dimension + j] = \
                               0.20*(my_grid->element[i * my_grid->dimension + j] + \
                                       my_grid->element[(i - 1) * my_grid->dimension + j] +\
                                       my_grid->element[(i + 1) * my_grid->dimension + j] +\
                                       my_grid->element[i * my_grid->dimension + (j + 1)] +\
                                       my_grid->element[i * my_grid->dimension + (j - 1)]);
		
                my_grid->diff += fabs(my_grid->element[i * my_grid->dimension + j] - my_grid->temp);
		
	    }  
	}

	pthread_mutex_lock(&mutex);
	diff_rb = my_grid->diff;
	pthread_mutex_unlock(&mutex);
	barrier_sync(&barrier, my_grid->thread_id); 
	
	
	for(i = my_grid->dimension_start + 1; i < (my_grid->dimension_end-1); i++)
	{
            for(int j = 2; j < (my_grid->dimension-1); j+=2)
	    {
                my_grid->temp = my_grid->element[i * my_grid->dimension + j];
                /* Apply the update rule. */	
                my_grid->element[i * my_grid->dimension + j] = \
                               0.20*(my_grid->element[i * my_grid->dimension + j] + \
                                       my_grid->element[(i - 1) * my_grid->dimension + j] +\
                                       my_grid->element[(i + 1) * my_grid->dimension + j] +\
                                       my_grid->element[i * my_grid->dimension + (j + 1)] +\
                                       my_grid->element[i * my_grid->dimension + (j - 1)]);

                my_grid->diff += fabs(my_grid->element[i * my_grid->dimension + j] - my_grid->temp);
	    }  
	}

	//barrier_sync(&barrier, my_grid->thread_id);
	pthread_mutex_lock(&mutex);
	diff_rb = my_grid->diff;
	pthread_mutex_unlock(&mutex);
	
	
        
	pthread_exit(NULL);
}


void 
barrier_sync(barrier_t *barrier, int thread_number)
{
    sem_wait(&(barrier->counter_sem));

    /* Check if all threads before us, that is NUM_THREADS-1 threads have reached this point. */	  
    if(barrier->counter == (NUM_THREADS - 1)){
        barrier->counter = 0; /* Reset the counter. */
        sem_post(&(barrier->counter_sem)); 
					 
        /* Signal the blocked threads that it is now safe to cross the barrier. */
        //printf("Thread number %d is signalling other threads to proceed. \n", thread_number); 
        for(int i = 0; i < (NUM_THREADS - 1); i++)
            sem_post(&(barrier->barrier_sem));
    } 
    else{
        barrier->counter++;
        sem_post(&(barrier->counter_sem));
        sem_wait(&(barrier->barrier_sem)); /* Block on the barrier semaphore. */
    }
}


		
/* The main function */
int 
main(int argc, char **argv)
{	
	/* Generate the grids and populate them with the same set of random values. */
	GRID_STRUCT *grid_1 = (GRID_STRUCT *)malloc(sizeof(GRID_STRUCT)); 
	GRID_STRUCT *grid_2 = (GRID_STRUCT *)malloc(sizeof(GRID_STRUCT)); 
	GRID_STRUCT *grid_3 = (GRID_STRUCT *)malloc(sizeof(GRID_STRUCT)); 

	grid_1->dimension = GRID_DIMENSION;
	grid_1->num_elements = grid_1->dimension * grid_1->dimension;
	grid_2->dimension = GRID_DIMENSION;
	grid_2->num_elements = grid_2->dimension * grid_2->dimension;
	grid_3->dimension = GRID_DIMENSION;
	grid_3->num_elements = grid_3->dimension * grid_3->dimension;

 	create_grids(grid_1, grid_2, grid_3);
	struct timeval start, stop;
	
	/*Compute the reference solution using the single-threaded version. */
	/*printf("Using the single threaded version to solve the grid. \n");
	gettimeofday(&start, NULL);
	int num_iter = compute_gold(grid_1);
	gettimeofday(&stop, NULL);
	printf("CPU run time = %0.8f s. \n", (float)(stop.tv_sec - start.tv_sec + (stop.tv_usec - start.tv_usec)/(float)1000000));
	printf("Convergence achieved after %d iterations. \n", num_iter);*/

	/* Use pthreads to solve the equation uisng the red-black parallelization technique. */
	
	printf("Using pthreads to solve the grid using the red-black parallelization method. \n");
	gettimeofday(&start, NULL);
	int num_iter = compute_using_pthreads_red_black(grid_2);
	gettimeofday(&stop, NULL);
	printf("CPU run time pthreads red black = %0.8f s. \n", (float)(stop.tv_sec - start.tv_sec + (stop.tv_usec - start.tv_usec)/(float)1000000));
	printf("Convergence achieved after %d iterations. \n", num_iter);

	
	/*Use pthreads to solve the equation using the jacobi method in parallel. */
	printf("Using pthreads to solve the grid using the jacobi method. \n");
	gettimeofday(&start, NULL);
	num_iter = compute_using_pthreads_jacobi(grid_3);
	gettimeofday(&stop, NULL);
	printf("CPU run time using pthreads jacobi = %0.8f s. \n", (float)(stop.tv_sec - start.tv_sec + (stop.tv_usec - start.tv_usec)/(float)1000000));
	printf("Convergence achieved after %d iterations. \n", num_iter);

	/* Print key statistics for the converged values. */
	printf("\n");
	printf("Reference: \n");
	print_statistics(grid_1);

	printf("Red-black: \n");
	print_statistics(grid_2);
		
	printf("Jacobi: \n");
	print_statistics(grid_3);

    /* Compute grid differences. */
    compute_grid_differences(grid_1, grid_2, grid_3);

	/* Free up the grid data structures. */
	free((void *)grid_1->element);	
	free((void *)grid_1); 
	
	free((void *)grid_2->element);	
	free((void *)grid_2);

	free((void *)grid_3->element);	
	free((void *)grid_3);

	exit(0);
}
