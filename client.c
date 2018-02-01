#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/mman.h>

//Struct to house data in queue (shared memory)
struct queuemem{
	int currsize;
	char path[1024];
	char keyword[256];
};




int main(int argc, char *argv[])
{
	char p[1024];
	char k[256];
	int queue = atoi(argv[1]);
	//Intialize data for shared memory/semaphores
	const int SIZE = queue * sizeof(struct queuemem);
	char *name = "Project3";
	int shm;
	//Open semaphore/shared memory
	sem_t *sem = sem_open("/OSSem",O_CREAT,0644);
	shm = shm_open(name,O_RDWR,0666);
	//Check for errors
	if(shm == -1)
	{
		printf("ERROR\n");
		return -1;
	}
	
	if(sem == SEM_FAILED)
	{
		printf("Sem failed\n");
		return -1;
	}
	//Point to data in shared memory
	struct queuemem *arr = mmap(0,SIZE,PROT_READ | PROT_WRITE, MAP_SHARED, shm,0);
	if(arr == MAP_FAILED)
	{
		printf("ERROR\n");
		return 0; 
	}
	FILE *file = fopen(argv[2],"r");
	if(file == NULL)
	{
		printf("Error\n");
		return 0;
	}
	int flag = 1;
	//Add files so long as queue is less than certain size
	while(flag == 1)
	{
		sem_wait(sem);
		if(arr[0].currsize < queue)
		{
//			sem_wait(sem);
			if(fscanf(file,"%s %s",p,k) >= 1)
			{	
			//	sem_wait(sem);
				if(strcmp(p,"exit") == 0)
				{
					strcpy(arr[0].path,p);
					arr[0].currsize = 1;
				}
				else
				{
					int csize = arr[0].currsize;
					strcpy(arr[csize].path,p);
					strcpy(arr[csize].keyword,k);
					arr[csize].currsize = csize + 1;
					for(int i = 0; i < csize;i++)
					{
						arr[i].currsize += 1;
					}
				}
			//	sem_post(sem);
			}
			else{
				flag = 0;
			}
		}
		sem_post(sem);
	}
	sem_close(sem);
	fclose(file);
	return 0; 
}
