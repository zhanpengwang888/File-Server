#include <stdio.h>  
#include <sys/socket.h>  
#include <sys/types.h>  
#include <netinet/in.h>  
#include <arpa/inet.h>  
#include <unistd.h>  
#include <ctype.h>  
#include <string.h>  
#include <sys/stat.h>  
#include <pthread.h>  
#include <sys/wait.h>  
#include <stdlib.h>  
#include <stdbool.h>
#include <sys/mman.h>
#include "http-parser/http_parser.h"
#include <linux/tcp.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/sendfile.h> //--uncomment if run in linux
#include <dirent.h>

enum {FALSE, TRUE};

// this function returns the number of delim
int find_char(char* filename, char delim) {
	int i;
	int count = 0;
	for (i = 0; i < strlen(filename); i++) {
		if (filename[i] == delim)
			count ++;
	}
	return count;
}


// checking file permission.
int permission_checking(int socket, char* filename ) {
  	struct stat sb;
  	char *token1;
  	int num = 1;
  	int num_of_slash = find_char(filename, '/');
	char buff[1024];
	strncpy(buff, filename, sizeof(buff));
	char slash[1024];
	char base[1024];
	char temp[1024];
	bzero(temp, 1024);
  	if (filename[strlen(filename) - 1] == '/')
  		num_of_slash --;

	strncpy(slash, "/", sizeof(slash));
	strncpy(base, "/", sizeof(base));
  		
  	token1 = strtok(buff, "/");
        strcat(base, token1);
	strncpy(temp, base, sizeof(temp));
	char *ck = malloc(strlen(temp)+1);
	bzero(ck, strlen(temp));
	strncpy(ck, temp, strlen(temp));
	// check if the file or folder exists
	if(stat(ck, &sb) < 0) {
            //not_found(socket, filename);
            close(socket);
            free(ck);
            return 0;
          } else if ((sb.st_mode & S_IXOTH) != S_IXOTH) {
            // check if the file has world executable permission
            //permission_deny(socket);
            free(ck);
            close(socket);
            return 0;
          } else if ((sb.st_mode & S_IROTH) != S_IROTH && (sb.st_mode & S_IWOTH) != S_IWOTH) {
            // check if the file has world readable or writable permission                       
            //permission_deny(socket);
            free(ck);
            close(socket);
            return 0;
          }
	while ((token1 = strtok(NULL, "/")) != NULL && num < num_of_slash) {
	  strcat(base, slash);
	  strcat(base, token1);
	  bzero(temp, 1024);
	  strncpy(temp, base, strlen(base));
	  char *check = malloc(strlen(temp)+1);
	  bzero(check, strlen(temp));
	  strncpy(check, temp, strlen(temp));

	  // check if the file or folder exists
	  if(stat(check, &sb) < 0) {
	    //not_found(socket, filename);
	    close(socket);
	    free(check);
	    return 0;
	  } else if ((sb.st_mode & S_IXOTH) != S_IXOTH) {
	    // check if the file has world executable permission
	    //permission_deny(socket);
	    free(check);
	    close(socket);
	    return 0;
	  } else if ((sb.st_mode & S_IROTH) != S_IROTH && (sb.st_mode & S_IWOTH) != S_IWOTH) {
	    // check if the file has world readable or writable permission
	    //permission_deny(socket);
	    free(check);
            close(socket);
            return 0;
	  }
	  
	  //filename_tokens[num] = temp;
	  free(check);
	  num ++;
	  
  	}
  	return 0;
}

int main(void) {
	char* filename = "/files/foo/foo.txt";
	permission_checking(1, filename);
	return FALSE;

}
