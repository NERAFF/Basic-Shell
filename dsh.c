/*  dsh.c */
//LAUURIA LUCA 900326
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_LINE 4096
#define MAX_ARGS 256
#define MAX_PATH 512
#define MAX_PROMPT 32

char _path[MAX_PATH] = "/bin/:/usr/bin";

//Function to print an error message and exit 
void panic(const char* msg){
	if(errno){
		fprintf(stderr, "PANIC: %s: %s\n\n", msg, strerror(errno));
	}else{
		fprintf(stderr, "PANIC: %s\n\n", msg);
	}
	exit(EXIT_FAILURE);
}
//Function to prompt the user and read input
int prompt(char* buf, size_t buf_size, const char* prompt_string){
	printf("%s", prompt_string);
	if(fgets(buf, buf_size, stdin) == NULL){
		return EOF;//-1
	}
	size_t cur = -1;//delete \n in W
	do{
		cur++;
		if(buf[cur] == '\n'){
			buf[cur] = '\0';
			break;
		}
	}while (buf[cur] != '\0');
	return cur;
}
//Function to set the PATH evironment variable
void set_path(const char* new_path) {
	if(new_path != NULL){
		//change path
#if USE_DEBUG_PRINTF
		printf("DEBUG: new_path: %s\n",new_path);			
#endif	
		char temp_path[MAX_PATH];
        strncpy(temp_path, new_path, MAX_PATH - 1);
        temp_path[MAX_PATH - 1] = '\0';
		int cur_pos = 0;
		while(new_path[cur_pos] != '\0'){	
			cur_pos++;
			if(cur_pos >= MAX_PATH - 1 && new_path[cur_pos]!='\0'){
				fprintf(stderr, "Error: PATH string too long\n");
				return;	
			}
		}
		if(cur_pos >0)
			memcpy(_path, new_path, cur_pos + 1);
	}
	printf("%s\n", _path);
}
//Function to lookup the absolute path of a command
void path_lookup(char* abs_path, const char* rel_path){
	char* prefix;
	char buf[MAX_PATH];
	if(abs_path == NULL || rel_path == NULL)
		panic("get_abs_path: parameter error");
	prefix = strtok(_path,":");
	while(prefix != NULL){
		strcpy(buf, prefix);
		strcat(buf, rel_path);
		if(access(buf, X_OK) == 0){
			strcpy(abs_path, buf);
			return;
		}
		prefix= strtok(NULL,":");
	}
	strcpy(abs_path, rel_path);
}

//Function to execute a command with a relative or absolute path
void exec_rel2abs(char** arg_list){
	if (arg_list[0][0]=='/'){
		//absolute path
		execv(arg_list[0] , arg_list);	
	}
	else{
		//relative path
		char abs_path[MAX_PATH];
		path_lookup(abs_path, arg_list[0]);		
		execv(abs_path, arg_list);
	}
}
//Function to handle redirection of output
void do_redir(const char* out_path, char** arg_list, const char* mode){
	if(out_path == NULL)
		panic("do_redir: no path");	
	int pid = fork();
	if(pid > 0){
		int wpid = wait(NULL);
		if(wpid < 0) panic("do_redir: wait");
	}else if (pid == 0){
		//begin child node
		FILE* out = fopen(out_path, mode);
		if(out == NULL){
			perror(out_path);
			exit(EXIT_FAILURE);
		}
		dup2(fileno(out), 1);// 1 = fileno(stdout)
		exec_rel2abs(arg_list);		
		perror(arg_list[0]);
		exit(EXIT_FAILURE);
		//end child node
	}else{
		panic("fork");
	}
}
//Function for pipe
void do_pipe(size_t pipe_pos, char** arg_list){
	int pipefd[2];
	int pid;
	if(pipe(pipefd) < 0) panic("do_pipe: pipe");
	//left side of the pipe
	pid = fork();
	if(pid > 0){
		int wpid = wait(NULL);
		if(wpid < 0) panic("do_pipe: wait");
	}else if (pid == 0){
		//child
		close(pipefd[0]);
		dup2(pipefd[1],1);
		close(pipefd[1]);
		exec_rel2abs(arg_list);			
		perror(arg_list[0]);
		exit(EXIT_FAILURE);
	}else{
		panic("do_pipe: fork");
	}
	//right side of the pipe
	pid = fork();
	if(pid > 0){
		close(pipefd[0]);
		close(pipefd[1]);
		int wpid = wait(NULL);
		if(wpid < 0) panic("do_pipe: wait");
	}else if (pid == 0){
		//child
		close(pipefd[1]);
		dup2(pipefd[0],0);
		close(pipefd[0]);
		exec_rel2abs(arg_list + pipe_pos + 1);			
		perror(arg_list[pipe_pos + 1]);
		exit(EXIT_FAILURE);
	}else{
		panic("do_pipe: fork");
	}
}
//Function to execute a command
void do_exec(char** arg_list, int background){
	int pid = fork();
	if(pid > 0){
		if(!background){
            // Wait for the child process only if the command is not in the background
            int wpid = wait(NULL);
            if(wpid < 0) panic("do_exec: wait");
        }
        else{
            printf("PID[%d] background process\n", pid);
        }
	}else if (pid == 0){
        if(background){
        // If the command is in the background, detach from the terminal       	
			freopen("/dev/null", "r", stdin);
            freopen("/dev/null", "w", stdout);
            freopen("/dev/null", "w", stderr);  
        }
        exec_rel2abs(arg_list);
        perror(arg_list[0]);
        exit(EXIT_FAILURE);
       
	}else{
		panic("do_exec: fork");
	}
}

int main(void){
	char input_buffer[MAX_LINE];
	size_t arg_count;
	char* arg_list[MAX_ARGS];
	char prompt_string[MAX_PROMPT]="\0";
	if(isatty(0)){
		//we're in interactive session
		strcpy(prompt_string, "dsh$ \0");
	}
	while(prompt(input_buffer, MAX_LINE, prompt_string) >= 0){
		//tokenize input
		arg_count = 0;
		arg_list[arg_count] = strtok(input_buffer, " ");
		//1 param-->comando 
		//2...n-->parametri comando
		if(arg_list[arg_count] == NULL){
			continue;//non ho token torno al ciclo
		}else{
			do{
				arg_count++;
				if (arg_count > MAX_ARGS) break;
				arg_list[arg_count] = strtok(NULL, " ");
			}while(arg_list[arg_count] != NULL);// arg_count è num_arg+1
		}
#if USE_DEBUG_PRINTF
		printf("DEBUG: tokens:");
		for(size_t i=0; i < arg_count; i++){
			printf(" %s[%zu]", arg_list[i],i);
		}
		puts("");
#endif
		//builtins
		if(strcmp(arg_list[0],"exit") == 0){
			break;
		}
		if(strcmp(arg_list[0],"setpath") == 0){
			set_path(arg_list[1]);
			continue;
		}
		{
			//check for special characters
			size_t redir_pos=0;
			size_t append_pos=0;
			size_t pipe_pos=0;
			size_t background_pos=0;
			
			for(size_t i=0; i<arg_count; i++){//loop controllo caratteri speciali
				if(strcmp(arg_list[i],">")== 0){
					redir_pos=i;
					break;
				}
				if(strcmp(arg_list[i],">>")== 0){
					append_pos=i;
					break;
				}
				if(strcmp(arg_list[i],"|")== 0){
					pipe_pos=i;
					break;
				}
				if(strcmp(arg_list[i],"&")== 0){
					background_pos=i;
					break;
				}
			}
			//do shell ops
			if(redir_pos != 0){
				arg_list[redir_pos] = NULL;
				//effettuare la riderection
				do_redir(arg_list[redir_pos + 1], arg_list, "w+");
			}
			else if(append_pos !=0){
				arg_list[append_pos] = NULL;
				do_redir(arg_list[append_pos + 1], arg_list, "a+");
			}
			else if(pipe_pos !=0){
				arg_list[pipe_pos] = NULL;
				do_pipe(pipe_pos, arg_list);
			}
            else if (background_pos!=0) {
                arg_list[background_pos] = NULL;
  				do_exec(arg_list,1);//exec in background
            }
			else{
				do_exec(arg_list,0);
			}
 		}
	}
	exit(EXIT_SUCCESS);
}
