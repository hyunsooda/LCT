#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>

#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )
#define NAME_LENGTH 5000
#define STACK_LEN 1000
#define N 100
#define TRUE 1
#define FALSE 0
#define MAX_LINE 150

#define ANSI_COLOR_RED "\x1b[31m"   // change commit, deleted
#define ANSI_COLOR_GREEN "\x1b[32m"  // change commit
#define ANSI_COLOR_BLUE "\x1b[34m"   // created
#define ANSI_COLOR_YELLOW "\x1b[33m" // modified


typedef struct _Contents {
	char* names[NAME_LENGTH];
	int len;
}Contents;

typedef struct _arrayStack {
	char* stackArr[STACK_LEN];
	int topIndex;
}ArrayStack;
typedef ArrayStack Stack;

void make_f(char* p, int* f);
int KMP(char*, char*);

void getContents(char [], Contents**);
int isExisted(char [], char []);
void copyAllFiles(char []);
int compareTwoFile();

void StackInit(Stack* pstack);
int SIsEmpty(Stack* pstack);
void SPush(Stack* pstack, char* data);
char* SPop(Stack* pstack);
char* SPeek(Stack* pstack);
void SReverseSearch(Stack* pstack);
void log_print(Stack* pstack);
void StackFree(Stack* pstack);
int check(Stack* pstack, char []);


int NOT_MODIFIED = 0;
int INITIAL_CREATE = 0;

int main( int argc, char **argv ) {
	char target[20]; /* monitoring directory name */
	int fd;
	int wd; /* watch desc */
	int flag = 0;
	char* ignores[] = {".goutputstream-", "swo", "swx", "4913", "~", ".swpx", ".swp", ".save" };
	char buffer[BUF_LEN];
	Contents* contents = NULL;
	struct timeval timeout;
	fd_set reads, cpy_reads;
	Stack stack;

	fd = inotify_init();
	
	if (fd < 0) {
		perror("inotify_init");
	}
	if (argc < 2) {
		fprintf (stderr, "Watching the current directory\n");
		strcpy (target, ".");
	} else {
		fprintf (stderr, "Watching '%s' directory\n", argv[1]);
		strcpy (target, argv[1]);
	}
	
	StackInit(&stack);	
	copyAllFiles(target);
	wd = inotify_add_watch(fd, target, IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_TO );
	FD_ZERO(&reads);
	FD_SET(fd, &reads);

  	while(1) {
		int length, i = 0;
		int is_detected = 0;
		char msg[50], FROM_MSG[50];
		int FROM_FLAG = 0;
		
		cpy_reads = reads;
		timeout.tv_sec = 10;
		timeout.tv_usec = 10000;

		if(contents) {
			for(int m=0; m<contents->len; m++) 
				free(contents->names[m]);
			free(contents);
		}
	
		//getContents(target, &contents);
		StackFree(&stack);
		StackInit(&stack);
		is_detected = select(fd+1,&cpy_reads,0,0,&timeout);
		if(is_detected == -1)
			perror("select() error");
		else if(!is_detected) {
			fflush(stdout);
			continue;
		} 
		
		length = read(fd, buffer, BUF_LEN);
		if (length < 0) 
			perror("read");
		
		while (i < length) {
			struct inotify_event *event = (struct inotify_event *) &buffer[i];
			
			flag = 0;

			if (event->len) {
				for(int k=0; k<8; k++) {
					if(KMP(event->name, ignores[k]) != -1) {
						flag = 1;
						break;
					}
				}
				if(!flag) {
					memset(&msg, 0, 50);
					if ((event->mask & IN_CREATE)) {
						if (event->mask & IN_ISDIR) 
							sprintf(msg, "The directory %s was created.", event->name);
							//printf("The directory %s was created.\n", event->name);      
						else 
							sprintf(msg, ANSI_COLOR_BLUE "The file %s was created.", event->name);
							//printf("The file %s was created.\n", event->name); 
						SPush(&stack, msg);
					}
					else if (event->mask & IN_DELETE) {
						if (event->mask & IN_ISDIR) 
							sprintf(msg, "the directory %s was deleted.", event->name);
							//printf("The directory %s was deleted.\n", event->name);      
						else 
							sprintf(msg, ANSI_COLOR_RED "The file %s was deleted.", event->name); //printf("The file %s was deleted.\n", event->name);
						SPush(&stack, msg);
					} else if (event->mask & IN_MODIFY) {
						if (event->mask & IN_ISDIR) 
							sprintf(msg, "The directory %s was modified.", event->name);
						else 
							sprintf(msg, ANSI_COLOR_YELLOW "The file %s was modified.", event->name); 
						if(!compareTwoFile(event->name)) { 
							if(!INITIAL_CREATE)
								NOT_MODIFIED = 1;
						}
						SPush(&stack,msg);
					} else if((event->mask & IN_MOVED_TO)) {
						printf("%s was moved from some place.\n", event->name);
					} 
				}
			}
			i += EVENT_SIZE + event->len;
		}
		log_print(&stack);
		copyAllFiles(target);
	}
	
	inotify_rm_watch(fd, wd);
	close(fd);
	return 0;
}

void getContents(char target[], Contents** Content) {
	DIR* dir_ptr;
	struct dirent* direntp;

	if((dir_ptr = opendir(target)) == NULL)
		perror("opendir() error");
	else {
		*Content = (Contents*)malloc(sizeof(Contents));
		while((direntp = readdir(dir_ptr)) != NULL) {
			(*Content)->names[(*Content)->len] = (char*)malloc(strlen(direntp->d_name)+1);
			strcpy((*Content)->names[(*Content)->len], direntp->d_name);
			(*Content)->len++;
		}		
	}
	closedir(dir_ptr);
}

int isExisted(char target[],char name[]) {
	DIR* dir_ptr;
	struct dirent* direntp;

	if((dir_ptr = opendir(target)) == NULL)
		perror("opendir() error");
	else {
		while((direntp = readdir(dir_ptr)) != NULL) {
			if(!strcmp(direntp->d_name, name))
				return TRUE;
		}
	}
	return FALSE;
}

void StackFree(Stack* pstack) {
	if(pstack->topIndex < 0) return;
	while(SIsEmpty(pstack)) 
		free(SPop(pstack));
}

void StackInit(Stack* pstack) {
	pstack->topIndex = -1;
}

int SIsEmpty(Stack* pstack) {
	if(pstack->topIndex == -1)
		return TRUE;
	else 
		return FALSE;
}

void SPush(Stack* pstack, char* str) {
	pstack->topIndex += 1;
	pstack->stackArr[pstack->topIndex] = (char*)malloc(200);
	strcpy(pstack->stackArr[pstack->topIndex],str);
}

char*  SPop(Stack* pstack) {
	int rIdx;
	if(SIsEmpty(pstack)) {
		printf("Stack Pop  error");
		exit(1);
	}

	rIdx = pstack->topIndex;
	pstack->topIndex -= 1;
	return pstack->stackArr[rIdx];
}
void SReverseSearch(Stack* pstack) {
	int backup = pstack->topIndex;
	int i = 0;

	if(SIsEmpty(pstack)) {
		printf("Stack Reverse Search error");
		exit(1);
	}

	while(pstack->topIndex >= 0) {
		printf("%s\n",pstack->stackArr[i]);
		pstack->topIndex--;
		i++;
	}

	pstack->topIndex = backup;
}

int check(Stack* pstack, char msg[]) {
	int i=0;
	int backup = pstack->topIndex;

	while(pstack->topIndex >= 0) {
		if(!strcmp(pstack->stackArr[i], msg)) {
			pstack->topIndex = backup;
			return FALSE;
		}
		pstack->topIndex--;
		i++;
	}
	pstack->topIndex = backup;
	return TRUE;
}


char* SPeek(Stack* pstack) {
	if(SIsEmpty(pstack)) {
		printf("Stack memory error");
		exit(1);
	}
	return pstack->stackArr[pstack->topIndex];
}

void make_f(char *p, int *f)
{
    int pl;
 	int q, k;
   
	pl = strlen(p);
    f[0] = -1;
    k = -1;

    for( q=1; q<pl; ++q)
    {
        while( k>=0 && p[k+1] != p[q] ) k = f[k];
        if(p[k+1] == p[q]) k++;
        f[q] = k;
    }
}

int KMP(char *s, char *p) 
{
    int f[ N ];
    int sl, pl;
    int i, q;

    pl = strlen(p);
    sl = strlen(s);
    make_f(p, f);

    q = -1;

    for( i=0; i<sl; ++i)
    {
        while( q >= 0 && p[q+1] != s[i] ) q = f[q];
        if(p[q+1] == s[i]) q++;
        if(q == pl-1) return i-pl+1;
    }
    return -1;
}

void log_print(Stack* pstack) {
	char* str;
	int count = 0;
	short appear_modify_log = 0;
	Stack logs;
	StackInit(&logs);

	fflush(stdout);

	while(!SIsEmpty(pstack)) {
		if(count == 1) 
			appear_modify_log = count = 0;	
		str = SPop(pstack);
	//	printf("123%s\n",str);
		if(appear_modify_log) {
			count++;
			if((KMP(str, "created") != -1) || (KMP(str,"moved") != -1)) {
				if(NOT_MODIFIED) {
					SPop(&logs);
					NOT_MODIFIED = 0;
				}
				continue;
			}
		}
		if(KMP(str, "modified") != -1)  
			appear_modify_log = 1;
		SPush(&logs,str);
	}
	if(INITIAL_CREATE) {
		printf("(new) %s\n", str);
		INITIAL_CREATE = 0;
	}
	else if(logs.topIndex > -1)
		SReverseSearch(&logs);
	StackFree(&logs);
}

void copyAllFiles(char* target) {
	DIR* dir_ptr;
	struct dirent* direntp;
	int file, output;
	int reads;
	struct stat sb,gitsb;
	char buf[1000];
	char filename[100];

	// remove
	chdir(".git");
	if((dir_ptr = opendir(target)) == NULL) {
		perror("opendir() error");
		printf("%d\n",__LINE__);
	}

	while((direntp = readdir(dir_ptr)) != NULL) {
		if(stat(direntp->d_name, &gitsb) == -1) {
			perror("stat");
			printf("%d\n",__LINE__);
		}
		
		if(S_ISREG(gitsb.st_mode)) {
			if(remove(direntp->d_name) == -1) {
				perror("remove() error");
				printf("%d\n",__LINE__);
			}
		}
	}

    chdir("..");	
	// copy
	FILE* in;
	FILE* out;
	if((dir_ptr = opendir(target)) == NULL) {
		perror("opendir() error");
		printf("%d\n", __LINE__);
		exit(1);
	}
	while((direntp = readdir(dir_ptr)) != NULL) {
		if(stat(direntp->d_name, &sb) == -1) {
			// no file or directory
			// temporary file which is made by vim
			//perror("stat");
			//printf("%d\n",__LINE__);
			continue;
		}

		if(S_ISREG(sb.st_mode)) {
			//printf("%s\n", direntp->d_name);
			in = fopen(direntp->d_name, "rb");
			chdir(".git");
			out = fopen(direntp->d_name, "wb");

			while(1) {
				reads = fread((void*)buf,1,1000,in);
			//	printf("%d\n",reads);
				if(reads < 1000) {
					fwrite((void*)buf,1,reads,out);
					break;
				}
				fwrite((void*)buf,1,reads,out);
			}

			fclose(out);
			fclose(in);
			chdir("..");
		}
	}
	
}


int compareTwoFile(char file[]) {
	//char git[100] = ".git/";
	//execl("/bin/bash", "bash", "diff.sh", file, strcat(git,file), NULL);
	FILE* origin_file;
	FILE* git_file;
	char origin_buf[MAX_LINE], git_buf[MAX_LINE];
	Stack stack;
	int var, flag = 0, p_flag = 0, count = 0;

	StackInit(&stack);
	origin_file = fopen(file,"r");
	chdir(".git");
	if((git_file = fopen(file,"r")) == NULL) {
		if((git_file = fopen(strcat(file,"~"), "r")) == NULL) {
			INITIAL_CREATE = 1;
			return TRUE; // when creating a file
		}
	}
	chdir("..");
	
	while(fgets(origin_buf, MAX_LINE, origin_file) != NULL) {
		fgets(git_buf, MAX_LINE, git_file);

		var = strcmp(origin_buf, git_buf);
		count++;

		if ((var < 0) || (var > 0)) {
			if(check(&stack, origin_buf) && !SIsEmpty(&stack)) {
				printf("this is where the files do not match:\n");
				printf(ANSI_COLOR_GREEN"current(LINE:%d)%s", count, origin_buf);
				printf(ANSI_COLOR_RED"before(LINE:%d):%s\n",count, git_buf);
				flag = 1;

				while(!SIsEmpty(&stack)) 
					SPop(&stack);
			//	for(int k=0; k<MAX_LINE; k++) {
			//		if(git_buf[k] != 0) 
			//			p_flag = 1;
			//	}
			//	if(!p_flag) {
			//		SPush(&stack, git_buf);
			//		p_flag = 0;
				//}
				//continue;
			}
		}
		SPush(&stack, git_buf);
		memset(&origin_buf, 0, sizeof(origin_buf));
		memset(&git_buf, 0, sizeof(git_buf));
	}
	StackFree(&stack);
	if(flag) return TRUE;
	else return FALSE;
}




























