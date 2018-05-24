/*
 * 2017 Kristopher Micinski for CMSC 311 at Haverford.
 * 
 * Huge parts of code snippets in this project have been taken from:
 * 
 * http://www6.uniovi.es/cscene/CS5/CS5-05.html
 * http://www.cs.cmu.edu/afs/cs/academic/class/15213-s00/www/class28/tiny.c
 */

// Standard C libraries
#include <stdio.h>
#include <stdlib.h>

// Various POSIX libraries
#include <unistd.h>

// Various string utilities
#include <string.h>

// Operations on files
#include <fcntl.h>

// Gives us access to the C99 "bool" type
#include <stdbool.h>

// Includes for socket programming
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

// Memory management stuff
#include <sys/mman.h>

#define perror(err) fprintf(stderr, "%s\n", err);

#define BUFLEN 1024

// 
// Global variables
// 
int server_fd = -1;
char *root;         // Root directory from which the server serves files
char buffer[1024];
char secret_key[100];

int LOG_ENABLED = 1;

void logmsg(char *message) {
  printf(message);
  printf("\n");
  fflush(stdout);
}

/*
 * Returns true if string `pre` is a prefix of `str`
 */
bool prefix(const char *pre, const char *str)
{
    return strncmp(pre, str, strlen(pre)) == 0;
}

void hello_world() {
  printf("Hello, world!\n");
  fflush(stdout);
}

/*
 * cerror - returns an error message to the client
 */
void cerror(FILE *stream, char *cause, char *errno, 
      char *shortmsg, char *longmsg) {
  fprintf(stream, "HTTP/1.1 %s %s\n", errno, shortmsg);
  fprintf(stream, "Content-type: text/html\n");
  fprintf(stream, "\n");
  fprintf(stream, "<html><title>Tiny Error</title>");
  fprintf(stream, "<body bgcolor=""ffffff"">\n");
  fprintf(stream, "%s: %s\n", errno, shortmsg);
  fprintf(stream, "<p>%s: %s\n", longmsg, cause);
  fprintf(stream, "<hr><em>The Tiny Web server</em>\n");
}

struct fatFilePointer {
  int length;
  char *data;
};

#define CHUNK_SIZE 1000

struct fatFilePointer read_file(char *name)
{
  FILE *file;
  char *buffer;
  unsigned long fileLen;
  struct fatFilePointer ret;
  ret.length = 0;
  ret.data = NULL;

  //Open file
  file = fopen(name, "rb");
  if (!file)
  {
    fprintf(stderr, "Unable to open file %s", name);
    return ret;
  }

  fileLen = 0;
  buffer = malloc(CHUNK_SIZE);
  char temp[CHUNK_SIZE];
  unsigned long bytesRead;

  do {
    bytesRead = fread(temp,1,CHUNK_SIZE,file);
    char *newbuffer = malloc(fileLen + CHUNK_SIZE);
    for (unsigned long i = 0; i < fileLen; i++) {
      newbuffer[i] = buffer[i];
    }
    for (unsigned long i = 0; i < bytesRead; i++) {
      newbuffer[fileLen + i] = temp[i];
    }
    fileLen += bytesRead;
    char *oldbuf = buffer;
    buffer = newbuffer;
    free(oldbuf);
  } while (bytesRead == CHUNK_SIZE);

  ret.length = fileLen;
  ret.data = buffer;
  return ret;
}

/*
 * Responsd to an HTTP request
 */
void serve_http(int socket, char *buffer) {
  char method[100];
  char filename[100];
  char filetype[30];
  char version[100];
  char cgiargs[100];
  char uri[200];
  char *p;
  FILE *stream = fdopen(socket, "r+");
  struct stat sbuf;
  int fd = -1;

  bzero(method,100);
  bzero(uri,100);
  bzero(version,100);

  sscanf(buffer, "%s %s %s", method, uri, version);
  
  /* tiny only supports the GET method */
  if (strcasecmp(method, "GET") != 0) {
    cerror(stream, method, "501", "Not Implemented", 
           "Tiny does not implement this method");
    fclose(stream);
    close(socket);
    return;
  }
  
  /* read (and ignore) the HTTP headers */
  logmsg("HTTP request:\n");
  logmsg(buffer);

  /* parse the uri [crufty] */
  strcpy(cgiargs, "");
  strcpy(filename, root);
  strcat(filename, uri);
  if (uri[strlen(uri)-1] == '/') {
    strcat(filename, "index.html");
  }
  logmsg(filename);

  /* make sure the file exists */
  if (stat(filename, &sbuf) < 0) {
    cerror(stream, filename, "404", "Not found", 
           "Tiny couldn't find this file");
    fclose(stream);
    close(socket);
    return;
  }

  /* serve static content */
  if (strstr(filename, ".html")) {
    strcpy(filetype, "text/html");
  } else if (strstr(filename, ".gif")) {
    strcpy(filetype, "image/gif");
  } else if (strstr(filename, ".jpg")) {
    strcpy(filetype, "image/jpg");
  } else {
    strcpy(filetype, "text/plain");
  }
    
  struct fatFilePointer contents = read_file(filename);

  /* print response header */
  fprintf(stream, "HTTP/1.1 200 OK\n");
  fprintf(stream, "Server: Tiny Web Server\n");
  printf("Writing file with length: %d\n", (int)sbuf.st_size);
  fprintf(stream, "Content-length: %d\n", contents.length);
  fprintf(stream, "Content-type: %s\n", filetype);
  fprintf(stream, "\r\n");
    
  // Use mmap to return arbitrary-sized response body 
  fwrite(contents.data, 1, contents.length, stream);
  free(contents.data);
  fflush(stream);
}

int handle_connection(int socket)
{
  char string[100];
  bzero(buffer,1024);
  while (true) {
    int length = recv(socket, buffer, 1024, 0);
    if (length < 1) {
      // The connection has been closed
      break;
    }
    logmsg("Received some data!");
    logmsg(buffer);
    if (prefix("goodbye",buffer)) {
      send(socket,"goodbye\n",8,0);
      return 0;
    } else if (prefix("hello",buffer)) {
      const char *response = "Hello!\n";
      send(socket,response, strlen(response), 0);
    } else if (prefix("echo ",buffer)) {
      // "echo NNN data..."
      char numbuf[5];
      int len;
      char *position = buffer+5;
      int i = 0;
      // Read until you see the empty space after the number
      while (*position != ' ') {
        numbuf[i] = *position;
        position++;
        i++;
      }
      // Now at "echo NNN^ data", so move up one
      position++;
      numbuf[i] = 0;
      len = atoi(numbuf);
      memcpy(string,position,len);
      send(socket,"Server is echoing: ", strlen("Server is echoing: "),0);
      char socbuf[12];
      sprintf(socbuf,"%d",socket);
      send(socket,socbuf,strlen(socbuf),0);
      send(socket," ", 1, 0);
      send(socket,string,strlen(string),0);
    } else {
      serve_http(socket, buffer);
      break;
    }
  }
  return 0;
}

// Run this at  cleanup, closes server file descriptor
void cleanup() {
  if (server_fd != -1) {
    close(server_fd);
  }
}

// Main entry point for program
int main(int argc, char** argv)
{
  int socket_id;
  int client;
  socklen_t addrlen = sizeof(struct sockaddr_in);
  struct sockaddr_in this_addr;
  struct sockaddr_in peer_addr;
  unsigned short port = 8080; /* Port to listen on */

  if(argc != 4) {
    printf("Usage: %s <port-number> <root-directory> <secret-key>\n", argv[0]);
    exit(1);
  }

  port = atoi(argv[1]);
  root = argv[2];
  
  strcpy(secret_key,argv[3]);

  // We've stack allocated this_addr and peer_addr, so zero them
  // (since we wouldn't know what was there otherwise).
  memset(&this_addr, 0, addrlen );
  memset(&peer_addr, 0, addrlen );

  // Set input port
  this_addr.sin_port        = htons(port);
  // Say that we want internet traffic
  this_addr.sin_family      = AF_INET;
  // Accept connections to all IP addresses assigned to this machine
  this_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  // Actually get us a socket that will listen for internet
  // connections
  socket_id = socket( AF_INET, SOCK_STREAM, IPPROTO_IP);
  if (setsockopt(socket_id, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0) {
    logmsg("setsockopt(SO_REUSEADDR) failed");
    exit(1);
  }

  // Set that socket up using the configuration we specified
  if (bind(socket_id, (const struct sockaddr *) &this_addr, addrlen) != 0) {
    logmsg("bind failed!");
    exit(1);
  }
  
  // Listen for connections on this socket
  if (listen(socket_id, 5) != 0) {
    logmsg("listen failed!");
    exit(1);
  }

  printf("There's a server running on port %d.\n", port);
  
  // Loop forever while there is a connection
  while((client = accept(socket_id, (struct sockaddr *) &peer_addr,
                         &addrlen)) != -1) {
    printf("Got a connection on port %d, handling now.", port);
    handle_connection(client);
    logmsg("Connectee hung up connection.");
    close(client);
  }
  
  return 0;
}