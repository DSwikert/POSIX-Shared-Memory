#include <string.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>
#include <stddef.h>
#include <sys/wait.h>
#include <dirent.h>
#include <pthread.h>


#define PATHSIZE 1024
#define KEYSIZE 256
#define LINESIZE 1024
#define OUTSIZE 2048

//structure to contain keep info for print
struct printinfo{
	struct DLList *list;
	char dir[1024];

};
//Node structure to contain data
struct Node{
	char filename[1000];
	int linenum;
	char line[1024];
	struct Node *Next;
	struct Node *Prev;
};
//Doubly linked list structure
struct DLList{

	struct Node *Head;
	struct Node *Tail;
	int count;

};

//Create doubly linked list
struct DLList *createlist()
{
	struct DLList *list = (struct DLList*)malloc(sizeof(struct DLList));
	list->Head = NULL;
	list->Tail = NULL;
	list->count = 0;
	return list;
}

//Insert node into DLL List
struct DLList *insert(struct DLList *list,char *filename,int linenum, char *line)
{
	//Allocate Memory
	struct Node *node = (struct Node*)malloc(sizeof(struct Node));
	//Copy data
	strcpy(node->filename,filename);
	strcpy(node->line,line);
	node->linenum = linenum;
	node->Next = NULL;
	node->Prev = NULL;
	if(list->Head == NULL)
	{
		list->Head = node;
		list->Tail = node;
	}
	else
	{
		node->Prev = list->Tail;
		list->Tail->Next = node;
		list->Tail = node;
	}
	list->count = list->count + 1;
}

//Free list memory
void destroy(struct DLList *list)					//Frees up memory
{
	int flag = 0;
	if(list->Head == NULL)
	{
		free(list);
		return;
	}
	struct Node *node = list->Head;
	while(flag == 0)
	{
		if(list->Tail == node)
		{
			flag = 1;
			free(node);
		}
		else
		{
			node = node->Next;
			free(node->Prev);
		}
	}
	free(list);
}

//Write to output file
void *print(void *var)
{

	struct DLList *list = var;
	//Create file lock
	struct flock fl = {F_WRLCK, SEEK_CUR,   0,      0,     0 };
   	int fd;

    	fl.l_pid = getpid();

	FILE *file = fopen("output.txt","a");
        if(file == NULL)
        {
                printf("ERROR: File couldn't be opened\n");
        }

    	if ((fd = open("output.txt", O_RDWR)) == -1) {
       	perror("open");
        exit(1);
    	}
	if(fcntl(fd,F_SETLKW,&fl) == -1)
	{
		perror("fcntl");
		exit(1);
	}
	//Write to output.txt
	while(list->count > 0)
	{
		struct Node *node = list->Head;
        	if(node->Next != NULL)
        	{
        	        list->Head = node->Next;
        	        node->Next->Prev = NULL;
        	        node->Next = NULL;
        	}
        	else
        	{
        	        list->Head = NULL;
        	        list->Tail = NULL;
        	}
        	list->count = list->count - 1;
		fprintf(file,"%s:%d:%s",node->filename,node->linenum,node->line);
		free(node);
	}
	fl.l_type = F_UNLCK;
	fcntl(fd,F_SETLK,&fl);
	close(fd);
	fclose(file);
	pthread_exit(NULL);
}

//Structure to hold arguments for pthread function
struct args{
	char filename[1000];
	char keyword[256];
	struct DLList *list;
};


//Pthread function
void *pts(void *var)
{
	//Semaphore
	sem_t *sm = sem_open("/TSem",O_CREAT,0644);

	//Grab function arguments
	struct args *arg = var;
	FILE *file = fopen(arg->filename,"r");
	if(file == NULL)
	{
		printf("ERROR in file\n");
		pthread_exit(NULL);

	}
	char k[256];
        strcpy(k,arg->keyword);
	struct DLList *list = arg->list;
	char in[2048];
	char word[1000];

	int linecount = 1;
	char *token;
	char hold[2048];
	//Read file
	while(fgets(in,2048,file) != NULL)
	{
		//Check the line for the keyword
		int flg = 1;
		strcpy(hold,in);
		token = strtok(in," ");
		while(token != NULL && flg == 1)
		{
			if(strcmp(k,token) == 0)
			{
				sem_wait(sm);
				insert(list,arg->filename,linecount,hold);
				sem_post(sm);
				flg = 0;
			}
			token = strtok(NULL," ");
		}
		
		linecount++;
	}
	
	fclose(file);
	sem_close(sm);
	pthread_exit(NULL);
}

void threading(char *p,char *k)
{
	//Create semaphore for forks
	sem_t *sm = sem_open("/TSem",O_CREAT,0644,1);

	//Initialize path, key, and directory variables
	char path[1024];
	char key[256];
	char cd[1024];
	getcwd(cd,sizeof(cd));
        strcpy(path,p);
	strcpy(key,k);
        DIR *dp;
        pthread_t pid[50];
        int threadcount = 1;
	//directory structures
        struct dirent *entry;
        struct stat pstat;
	struct DLList *list = createlist();
        if((dp = opendir(path)) != NULL)
        {
                chdir(path);

		//While there is still something to be read in directory
                while((entry = readdir(dp)) != NULL)
                {
	                stat(entry->d_name,&pstat);
			//Only read files
                        if(!S_ISDIR(pstat.st_mode))
                    	{
				//Use structure to pass arguements to pthread function
				struct args *arg = malloc(sizeof(struct args));
				strcpy(arg->filename,entry->d_name);
				strcpy(arg->keyword,key);
				arg->list = list;
				//Create a pthread for every file to be searched
                                pthread_create(&pid[threadcount],NULL,pts,arg);
                                pthread_join(pid[threadcount],NULL);
				threadcount++;
				free(arg);
                        }
                }
        }
        else
        {
                printf("Problem\n");
        }
	//Close directory/semaphores, join threads
        closedir(dp);
	sem_close(sm);
	chdir(cd);
	pthread_create(&pid[0],NULL,print,list);
	pthread_join(pid[0],NULL);
	destroy(list);

}


//Structure to hold data in shared memory
struct queuemem{
	
	int currsize;
	char path[PATHSIZE];
	char key[KEYSIZE];
};


int main(int argc, char *argv[])
{
	//Get Queue Size
	int queue = atoi(argv[1]);
	
	int child_count = 0;
	
	//Create 100 paths/keys to search for
	char locpath[100][PATHSIZE];
	char lockey[100][KEYSIZE];
	pid_t n;
	
	//Create Shared Memory	
	const int SIZE = queue * sizeof(struct queuemem);;
	int shm;
	char *name = "Project3";
	shm = shm_open(name,O_CREAT | O_RDWR, 0666);
	ftruncate(shm,SIZE);	

	//Pointer to the first member of the queue
	struct queuemem *ptr = mmap(0,SIZE,PROT_READ | PROT_WRITE,MAP_SHARED,shm,0);

	sem_t *sem = sem_open("/OSSem",O_CREAT,0644,1);

	//Create an exit flag
	int flag = 0;
	if(ptr == MAP_FAILED)
	{
		printf("ERROR\n");
		return 0;
	}
	ptr[0].currsize = 0;
	while(flag == 0)
	{
		//Semaphore for shared memory access
		sem_wait(sem);
		int csize = ptr[0].currsize;	
		//If queue isn't empty
		if(csize > 0)
		{
			//Check for exit
			if(strcmp(ptr[0].path,"exit") == 0)
			{
				flag = 1;
				printf("Done\n");
			}
			else
			{
				//Grab path and key
				strcpy(locpath[child_count],ptr[0].path);
                                strcpy(lockey[child_count],ptr[0].key);
                                child_count += 1;
				//Create a fork for each search request
				n = fork();
				if(n < 0)
				{
					fprintf(stderr,"Fork Failed"); 
				}
				else if(n == 0)
				{
					
					threading(locpath[child_count -1],lockey[child_count - 1]);
					return 0;
				}
				else
				{
					if(csize == 1)
					{
						ptr[0].currsize -= 1;
					}
					else
					{
						for(int i = 0; i < csize - 1;i++)
						{
							strcpy(ptr[i].path,ptr[i+1].path);
							strcpy(ptr[i].key,ptr[i+1].key);
							ptr[i].currsize -= 1;
						}
					}
				}
				
			}
		}
		sem_post(sem);
	}//Wait for chold proccesses to finish
	for(int j = 0; j < child_count;j++)
	{
		wait(NULL);
	}
	printf("Children done\n");
	if(sem_close(sem) == -1)
	{		
		printf("Error destroying semaphore\n");
	}
	if(shm_unlink(name) == -1)
	{
		printf("Error closing memory\n");
	}	
	return 0;
}
