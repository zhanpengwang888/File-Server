/* server.c inspired by 
	socket turtorial: http://www.cs.rpi.edu/~moorthy/Courses/os98/Pgms/socket.html
	tinyhttpd program. link: http://sourceforge.net/projects/tinyhttpd/
	and Kris hint code for 311: https://gist.github.com/kmicinski/afa3c1dec7ef612292fdeb27be8f625a
*/
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
#include "multipart-parser-master/multipartparser.h"
struct my_http_header_line
{
  char *field;
  size_t field_len;
  char *value;
  size_t value_len;
};
//predefined constant
#define OFFSET 0
#define SERVER_STRING "Server: bugfree/0.1.0\r\n"
#define BUF_SIZE 1024
#define CURRENT_LINE (&header[nlines - 1])
#define MAX_HEADER_LINES 2000
#define P_LEN 128 //each path assume to be at most 128 bytes
enum
{
  FALSE,
  TRUE
};
//Global variables
int server_fd = -1;
char *root; // Root directory from which the server serves files
char buffer[BUF_SIZE];
unsigned short port; //port used as globla
typedef struct
{
  int sock;
  char *url;
  int method;
  int buf_len;
} parsed_data_t;

struct fatFilePointer
{
  int length;
  int fd;
};

//The function prototypes
struct fatFilePointer read_file_s(char *name);
void *accept_request(void *); //accept and send request to appropriate function
void serve_file(FILE *stream, int client, char *filename, char *filetype);
void success(FILE *, int, char *);
void success_dir(FILE *, int, char *); // for reporting succesfully list the files in a directory
void permission_deny(int);
void unimplemented(int);
void not_found(FILE*, int, char *);
int startup(u_short *);                      // set up the socket for sever
void processing_delete_request(FILE *, int, char *); // delete the selected file
int url_callback(http_parser *parser, const char *at, size_t len);
void bad_request(int client);
void list_file(char *path, char **r_message); //added Fri 23
int existance_permission_checking(FILE *stream, int socket, char *filename);
//added Fri night added feature -- for post
int post_fd; //for storing file descrpitor used in post
int current;
int on_data(multipartparser *parser, const char *at, size_t length);
//modified  Sat 5 pm
void get_file(FILE *stream, int socket, char *header, char *filename);
void file_type(char *filename, char **filetype);
void put_file(FILE *stream, int socket, char *header, char *filename);
char *result;

struct fatFilePointer read_file_s(char *name)
{
  int fd;
  struct stat stat_buf;
  //char* buffer;
  struct fatFilePointer ret;
  ret.fd = -1;
  ret.length = 0;

  fd = open(name, O_RDONLY);
  if (fd == -1)
  {
    perror("unable to open file");
  }
  ret.fd = fd;
  fstat(fd, &stat_buf);
  ret.length = (int)stat_buf.st_size;
  return ret;
}

// this function returns the number of delim
int find_char(char *filename, char delim)
{
  int i;
  int count = 0;
  for (i = 0; i < strlen(filename); i++)
  {
    if (filename[i] == delim)
      count++;
  }
  return count;
}

// checking file existance and permission for POST request.
int existance_permission_checking_post(int socket, char *filename)
{
  struct stat sb;
  char *token1;
  int num = 1;
  int num_of_slash = find_char(filename, '/');
  char buff[1024];
  filename[0] = '/';
  strncpy(buff, filename, sizeof(buff));
  char slash[1024];
  char base[1024];
  char temp[1024];
  bzero(temp, 1024);
  if (filename[strlen(filename) - 1] == '/')
    num_of_slash--;

  strncpy(slash, "/", sizeof(slash)); // slash = "/"
  strncpy(base, "./", sizeof(base));  // base = "."

  token1 = strtok(buff, "/");
  strcat(base, token1);
  strncpy(temp, base, sizeof(temp));
  char *ck = calloc(strlen(temp) + 1, sizeof(char));
  bzero(ck, strlen(temp));
  strncpy(ck, temp, strlen(temp));
  char *d = calloc(strlen(ck), sizeof(char));
  bzero(d, strlen(ck));
  strncpy(d, ck + 1, strlen(ck));
  // check if the file or folder exists
  if (stat(ck, &sb) < 0)
  {
    mkdir(d, S_IWOTH | S_IXOTH); // create a new directory with world writable and world executable permissions.
    //close(socket);
    free(ck);
    free(d);
  }
  else if ((sb.st_mode & S_IXOTH) != S_IXOTH)
  {
    // check if the file has world executable permission
    permission_deny(socket);
    free(ck);
    free(d);
    close(socket);
    return FALSE;
  }
  else if ((sb.st_mode & S_IROTH) != S_IROTH && (sb.st_mode & S_IWOTH) != S_IWOTH)
  {
    // check if the file has world readable or writable permission
    permission_deny(socket);
    free(ck);
    free(d);
    close(socket);
    return FALSE;
  }
  free(ck);
  free(d);
  while ((token1 = strtok(NULL, "/")) != NULL && num < num_of_slash - 1)
  {
    strcat(base, slash);
    strcat(base, token1);
    bzero(temp, 1024);
    strncpy(temp, base, strlen(base));
    char *check = calloc(strlen(temp) + 1, sizeof(char));
    bzero(check, strlen(temp));
    strncpy(check, temp, strlen(temp));
    char *dir = calloc(strlen(check), sizeof(char));
    bzero(dir, strlen(check));
    strncpy(dir, check + 1, strlen(check));
    // check if the file or folder exists
    if (stat(check, &sb) < 0)
    {
      mkdir(dir, S_IWOTH | S_IXOTH); // create a new directory with world writable and world executable permissions.
      //close(socket);
      free(check);
      free(dir);
      //return FALSE;
    }
    else if ((sb.st_mode & S_IXOTH) != S_IXOTH)
    {
      // check if the file has world executable permission
      permission_deny(socket);
      free(check);
      free(dir);
      close(socket);
      return FALSE;
    }
    else if ((sb.st_mode & S_IROTH) != S_IROTH && (sb.st_mode & S_IWOTH) != S_IWOTH)
    {
      // check if the file has world readable or writable permission
      permission_deny(socket);
      free(check);
      free(dir);
      close(socket);
      return FALSE;
    }
    free(dir);
    free(check);
    num++;
  }
  return TRUE;
}

// checking file existance and permission for PUT requests.
int existance_permission_checking_put(int socket, char *filename)
{
  struct stat sb;
  char *token1;
  int num = 1;
  int num_of_slash = find_char(filename, '/');
  char buff[1024];
  filename[0] = '/';
  strncpy(buff, filename, sizeof(buff));
  char slash[1024];
  char base[1024];
  char temp[1024];
  bzero(temp, 1024);
  if (filename[strlen(filename) - 1] == '/')
    num_of_slash--;

  strncpy(slash, "/", sizeof(slash)); // slash = "/"
  strncpy(base, "./", sizeof(base));  // base = "."

  token1 = strtok(buff, "/");
  strcat(base, token1);
  strncpy(temp, base, sizeof(temp));
  char *ck = calloc(strlen(temp) + 1, sizeof(char));
  bzero(ck, strlen(temp));
  strncpy(ck, temp, strlen(temp));
  char *d = calloc(strlen(ck), sizeof(char));
  bzero(d, strlen(ck));
  strncpy(d, ck + 1, strlen(ck));
  // check if the file or folder exists
  if (stat(ck, &sb) < 0)
  {
    mkdir(d, S_IWOTH | S_IXOTH); // create a new directory with world writable and world executable permissions.
    //close(socket);
    free(ck);
    free(d);
  }
  else if ((sb.st_mode & S_IXOTH) != S_IXOTH)
  {
    // check if the file has world executable permission
    permission_deny(socket);
    free(ck);
    free(d);
    close(socket);
    return FALSE;
  }
  else if ((sb.st_mode & S_IROTH) != S_IROTH && (sb.st_mode & S_IWOTH) != S_IWOTH)
  {
    // check if the file has world readable or writable permission
    permission_deny(socket);
    free(ck);
    free(d);
    close(socket);
    return FALSE;
  }
  free(ck);
  free(d);
  while ((token1 = strtok(NULL, "/")) != NULL && num < num_of_slash - 1)
  {
    strcat(base, slash);
    strcat(base, token1);
    bzero(temp, 1024);
    strncpy(temp, base, strlen(base));
    char *check = calloc(strlen(temp) + 1, sizeof(char));
    bzero(check, strlen(temp));
    strncpy(check, temp, strlen(temp));
    char *dir = calloc(strlen(check), sizeof(char));
    bzero(dir, strlen(check));
    strncpy(dir, check + 1, strlen(check));
    // check if the file or folder exists
    if (stat(check, &sb) < 0)
    {
      mkdir(dir, S_IWOTH | S_IXOTH); // create a new directory with world writable and world executable permissions.
      //close(socket);
      free(check);
      free(dir);
      //return FALSE;
    }
    else if ((sb.st_mode & S_IXOTH) != S_IXOTH)
    {
      // check if the file has world executable permission
      permission_deny(socket);
      free(check);
      free(dir);
      close(socket);
      return FALSE;
    }
    else if ((sb.st_mode & S_IROTH) != S_IROTH && (sb.st_mode & S_IWOTH) != S_IWOTH)
    {
      // check if the file has world readable or writable permission
      permission_deny(socket);
      free(check);
      free(dir);
      close(socket);
      return FALSE;
    }
    free(dir);
    free(check);
    num++;
  }

  filename[0] = '.';
  // check the file.
  if (stat(filename, &sb) < 0)
  {
    bad_request(socket);
    return FALSE;
  }
  else if ((sb.st_mode & S_IROTH) != S_IROTH && (sb.st_mode & S_IWOTH) != S_IWOTH)
  {
    // check if the file has world readable or writable permission
    permission_deny(socket);
    //close(socket);
    return FALSE;
  }

  return TRUE;
}

// checking file existance and permission for GET and DELETE requests.
int existance_permission_checking(FILE *stream, int socket, char *filename)
{
  struct stat sb;
  char *token1;
  int num = 1;
  int num_of_slash = find_char(filename, '/');
  char buff[1024];
  filename[0] = '/';
  strncpy(buff, filename, sizeof(buff));
  char slash[1024];
  char base[1024];
  char temp[1024];
  bzero(temp, 1024);
  if (filename[strlen(filename) - 1] == '/')
    num_of_slash--;

  strncpy(slash, "/", sizeof(slash));
  strncpy(base, "./", sizeof(base));

  token1 = strtok(buff, "/");
  strcat(base, token1);
  strncpy(temp, base, sizeof(temp));
  char *ck = calloc(strlen(temp) + 1, sizeof(char));
  bzero(ck, strlen(temp));
  strncpy(ck, temp, strlen(temp));
  // check if the file or folder exists
  if (stat(ck, &sb) < 0)
  {
    not_found(stream, socket, filename);
    //close(socket);
    free(ck);
    return FALSE;
  }
  else if ((sb.st_mode & S_IXOTH) != S_IXOTH)
  {
    // check if the file has world executable permission
    permission_deny(socket);
    free(ck);
    //close(socket);
    return FALSE;
  }
  else if ((sb.st_mode & S_IROTH) != S_IROTH && (sb.st_mode & S_IWOTH) != S_IWOTH)
  {
    // check if the file has world readable or writable permission
    permission_deny(socket);
    free(ck);
    //close(socket);
    return FALSE;
  }
  free(ck);
  while ((token1 = strtok(NULL, "/")) != NULL && num < num_of_slash - 1)
  {
    strcat(base, slash);
    strcat(base, token1);
    bzero(temp, 1024);
    strncpy(temp, base, strlen(base));
    char *check = calloc(strlen(temp) + 1, sizeof(char));
    bzero(check, strlen(temp));
    strncpy(check, temp, strlen(temp));

    // check if the file or folder exists
    if (stat(check, &sb) < 0)
    {
      not_found(stream, socket, filename);
      close(socket);
      free(check);
      return FALSE;
    }
    else if ((sb.st_mode & S_IXOTH) != S_IXOTH)
    {
      // check if the file has world executable permission
      permission_deny(socket);
      free(check);
      close(socket);
      return FALSE;
    }
    else if ((sb.st_mode & S_IROTH) != S_IROTH && (sb.st_mode & S_IWOTH) != S_IWOTH)
    {
      // check if the file has world readable or writable permission
      permission_deny(socket);
      free(check);
      close(socket);
      return FALSE;
    }

    free(check);
    num++;
  }
  filename[0] = '.';
  // check the file.
  if (stat(filename, &sb) < 0)
  {
    not_found(stream, socket, filename);
    return FALSE;
  }
  else if ((sb.st_mode & S_IROTH) != S_IROTH && (sb.st_mode & S_IWOTH) != S_IWOTH)
  {
    // check if the file has world readable or writable permission
    permission_deny(socket);
    //close(socket);
    return FALSE;
  }
  return TRUE;
}

// skeleton for processing delete request. Working but just need correct messages for the cases.
void processing_delete_request(FILE *stream, int socket, char *filename)
{
  FILE *fp;
  int ret;

  if (!existance_permission_checking(stream, socket, filename))
  {
    return;
  }

  fp = fopen(filename, "w"); // open the file with write permission
  fclose(fp);
  // then execute the remove() to permanently remove the file
  ret = remove(filename);
  if (ret == 0)
  {
    // remove successfully 200 OK; Need 200 OK message
    success(stream, socket, filename);
    close(socket);
    return;
  }
}

void *accept_request(void *client)
{
  long client_socket_tmp = (long)client;
  int client_socket = (int)client_socket_tmp;
  //printf("client socket is %d",client_socket);
  //parse from header from http
  char method[BUF_SIZE] = "GET";
  char url[BUF_SIZE];
  //char version[BUF_SIZE]= "HTTP/1.0";
  //variable for convenient
  char filename[BUF_SIZE];
  char *filetype = malloc(BUF_SIZE);

  //char buf[BUF_SIZE];
  char *buf = malloc(BUF_SIZE + 1);
  buf[BUF_SIZE] = '\0';
  ssize_t recved;
  int nparsed = 0;
  //printf("%d\n", client_socket);
  recved = recv(client_socket, buf, BUF_SIZE, 0);
  //printf("%d\n", recved);
  if (recved < 0)
  {
    printf("Cannot receive from client!\n");
    perror("Receive error");
    exit(1);
  }

  // manunally parse the method name, since I have no idea how to use on_headers_complete
  int first_space;
  for (first_space = 0; first_space < BUF_SIZE; first_space++)
  {
    if (isspace(buf[first_space]))
    {
      break;
    }
  }
  bzero(method, BUF_SIZE);
  strncpy(method, buf, first_space);
  //added 11pm Friday night, we get into send file if the method is POST  -- all the functions need to be organized
  parsed_data_t *my_data = malloc(sizeof(parsed_data_t));
  my_data->sock = client_socket;
  my_data->url = malloc(sizeof(char) * recved + 1);
  //printf("I got here.\n");

  bzero(my_data->url, sizeof(char) * recved + 1);
  http_parser_settings settings;
  http_parser_settings_init(&settings);

  //settings.on_header_field = on_header_field;
  //settings.on_header_value = on_value_field;
  settings.on_url = url_callback;
  //settings.on_body = url_callback;

  http_parser *parser = malloc(sizeof(http_parser));
  http_parser_init(parser, HTTP_REQUEST);

  parser->data = my_data;
  nparsed = http_parser_execute(parser, &settings, buf, recved); // get url from mydata->url
  my_data->url[recved] = '\0';
  if (nparsed != recved)
  {
    perror("Parse has error!\n");
    //exit(1);
  }
  free(parser);

  FILE *stream = fdopen(client_socket, "r+");

  //organize filename and type
  strcpy(filename, root);
  strcpy(url, my_data->url);
  strcat(filename, url);
  file_type(filename, &filetype);
  printf("file name is now %s\n", filename);
  printf("file type is now %s\n", filetype);
  printf("method is now %s\n", method);
  //parse done, start to serve
  if (strcasecmp(method, "GET") == 0)
  {
    //get the complete path to file
    /* serve static content */
    //probably need a file type function
    serve_file(stream, client_socket, filename, filetype);
    close(client_socket);
  }
  else if (strcasecmp(method, "DELETE") == 0)
  {
    processing_delete_request(stream, client_socket, filename);
    //close(client_socket);
  } //added 11pm Friday night, we get into send file if the method is POST  -- all the functions need to be organized
  else if (strcasecmp(method, "POST") == 0)
  {
    get_file(stream, client_socket, buf, filename);
    close(client_socket);
    pthread_detach(pthread_self());
    pthread_exit(NULL);
  }
  else if (strcasecmp(method, "PUT") == 0)
  {
    put_file(stream, client_socket, buf, filename);
    close(client_socket);
    pthread_detach(pthread_self());
    pthread_exit(NULL);
  }
  else
  {
    unimplemented(client_socket);
    close(client_socket);
  }
  free(buf);
  free(filetype);
  free(my_data->url);
  free(my_data);
  free(result);
  fclose(stream);
  pthread_detach(pthread_self());
  pthread_exit(NULL);
}

void serve_file(FILE *stream, int client, char *filename, char *filetype)
{
  long int offset = OFFSET;
  //if the destination is a directory, default to send index.html -- need to revise to send a list
  if (filename[strlen(filename) - 1] == '/')
  {
    char *send_message;
    if (!existance_permission_checking(stream, client, filename))
      return;
    list_file(filename, &send_message);
    printf("%s\n", send_message);
    success_dir(stream, client, send_message);
    return;
  }
  if (!existance_permission_checking(stream, client, filename))
    return;
  struct fatFilePointer source = read_file_s(filename);
  ; // the file we want to send to client
  if (source.fd == -1)
  {
    not_found(stream, client, filename);
    return; //quit if we can't find corresponding file
  }
  else
  {
    success(stream, client, filename);
  }
  int num_byte = sendfile(client, source.fd, &offset, source.length);
  if (num_byte == -1)
  {
    perror("sendfile fails");
    bad_request(client);
  }
  if (num_byte != source.length)
  {
    perror("incomplete transfer from sendfile");
    bad_request(client);
  }
  //close file descirptor
  close(source.fd);
  //use sendfile later
}

//used to repsond request if client want a directory -- return the necessary html string
//vulnerable need to fix in the future
void list_file(char *path, char **r_message)
{
  DIR *d;
  struct dirent *dir;
  //char* result;
  char **tmp; // how many element should I malloc? ---wait
  int i = 0;
  tmp = malloc(sizeof(char *) * BUF_SIZE);
  char *target = malloc(strlen(path));
  strncpy(target, path + 2, strlen(path) - 1); //we need to get rid of the first point
  printf("target is %s\n", target);
  d = opendir(target);

  if (d)
  {
    while ((dir = readdir(d)) != NULL)
    {
      char *tmp_c = malloc(strlen(dir->d_name) + 1);
      tmp_c[strlen(dir->d_name)] = '\0';
      bzero(tmp_c, strlen(dir->d_name));
      strncpy(tmp_c, dir->d_name, strlen(dir->d_name));
      tmp[i] = tmp_c;
      printf("%s      %d\n", tmp[i], i);
      i++;
    }
    closedir(d);
  }
  result = malloc((i * P_LEN + BUF_SIZE) * sizeof(char));
  //result[i*P_LEN+BUF_SIZE] = '\0';
  bzero(result, i * P_LEN + BUF_SIZE);
  char *trans = malloc(BUF_SIZE);
  sprintf(trans, "<HTML>\n<head><title>%s</title></head>\n", path);
  //printf("%s",trans);
  strncpy(result, trans, strlen(trans));
  sprintf(trans, "<body><h1>%s</h1>\n<ul>\n", path);
  strncat(result, trans, strlen(trans));
  printf("%s this is tmp[0]\n", tmp[0]);
  for (int j = 0; j < i; j++)
  {
    if ((strcmp(tmp[j], ".") == 0) || (strcmp(tmp[j], "..") == 0))
      continue;
    else
    {
      sprintf(trans, "<li><a href=\"localhost:%d%s%s\">%s</a></li>\n", port, path, tmp[j], tmp[j]);
      strcat(result, trans);
    }
  }
  sprintf(trans, "</ul>\n</body>\n</html>\n");
  strcat(result, trans);
  printf("%s", result);
  //free all allocated memory
  for (int j = 0; j < i; j++)
    free(tmp[j]);
  free(target);
  free(tmp);
  free(trans);
  *r_message = result;
  //free(result);
}
//added Fri 23 night -- for post
int on_data(multipartparser *parser, const char *at, size_t length)
{
  printf("Multipart parsed Data!\n");
  printf("Length: %lu\n", length);

  write(post_fd, at, length);
  current += length;
  printf("Total %d\n", current);
  return 0;
}

//actually post file into our own directory
void get_file(FILE *stream, int socket, char *header, char *filename)
{
  //get the correct path to store the file                                      
  char* path_to_file = malloc(BUF_SIZE + 1);
  strcpy(path_to_file, "files/");
  char raw_name [BUF_SIZE] = "";
  strncpy(raw_name, filename+8,BUF_SIZE-7);
  //printf("tmp is %s\n",raw_name);
  strcat(path_to_file, raw_name);
  //FILE *stream = fdopen(socket, "r+");
  char *partial_body = malloc(BUF_SIZE + 1); //store the header part information
  partial_body[BUF_SIZE] = '\0';
  //int used = 0; //count the total num of char in partial body
  int length = 0;          //used to count the length of main body of request
  char boundary[BUF_SIZE]; //store the boundary information
  //char trans[BUF_SIZE]; // used to work as transition buffer
  // Parse each header in sequence
  printf("header is now %s\n\n\n", header);
  strncpy(partial_body, header, strlen(header));
  if (filename[strlen(root)] == '.' || filename[strlen(filename) - 1] == '/')
  {
    bad_request(socket);
    return;
  }
  //need to find the length of body and boundary
  char *tmp = NULL;
  tmp = strstr(partial_body, "Content-Length:");
  if (tmp == NULL)
  {
    bad_request(socket);
    return;
  }
  sscanf(tmp, "Content-Length: %d", &length);
  printf("Length is %d\n", length);
  char *tmp2 = NULL;
  tmp2 = strstr(partial_body, "Content-Type: multipart/form-data;");
  if (tmp2 == NULL)
  {
    bad_request(socket);
    return;
  }
  sscanf(tmp2, "Content-Type: multipart/form-data; boundary=%s", boundary);
  printf("Boundary is %s\n", boundary);
  //need to find the length of body and boundary

  //parse the header part --no need here

  //get the main body to request
  char *body = malloc(length);
  int read = fread(body, 1, length, stream);
  printf("Read %d out of %d bytes\n", read, length);

  char *total = malloc(strlen(partial_body) + length);
  memcpy(total, partial_body, strlen(partial_body));
  memcpy(total + strlen(partial_body), body, length);

  if (!existance_permission_checking_post(socket, filename))
    return;

  multipartparser_callbacks callbacks;
  multipartparser parser;
  multipartparser_callbacks_init(&callbacks); // It only sets all callbacks to NULL.
  //update Wed noon
  post_fd = open(path_to_file, O_WRONLY | O_TRUNC | O_CREAT, 0666);
  callbacks.on_data = &on_data;
  multipartparser_init(&parser, boundary);
  multipartparser_execute(&parser, &callbacks, total, strlen(partial_body) + length);
  printf("post_fd is now %d\n", post_fd);

  fprintf(stream, "HTTP/1.1 200 OK\n");
  fprintf(stream, SERVER_STRING);
  fprintf(stream, "Content-length: %d\n", length);
  fprintf(stream, "Content-type: multipart/form-data\n");
  fprintf(stream, "\r\n");
  fflush(stream);
  free(partial_body);
  free(body);
  free(total);
  return;
}

//actually put file into our own directory
void put_file(FILE *stream, int socket, char *header, char *filename)
{
  //get the correct path to store the file                                      
  char* path_to_file = malloc(BUF_SIZE + 1);
  strcpy(path_to_file, "files/");
  char raw_name [BUF_SIZE] = "";
  strncpy(raw_name, filename+8,BUF_SIZE-7);
  //printf("tmp is %s\n",raw_name);
  strcat(path_to_file, raw_name);
  //printf("the path to file is now %s\n",path_to_file);
  //FILE *stream = fdopen(socket, "r+");
  char *partial_body = malloc(BUF_SIZE + 1); //store the header part information
  partial_body[BUF_SIZE] = '\0';
  //int used = 0; //count the total num of char in partial body
  int length = 0;          //used to count the length of main body of request
  char boundary[BUF_SIZE]; //store the boundary information
  //char trans[BUF_SIZE]; // used to work as transition buffer
  // Parse each header in sequence
  printf("header is now %s\n\n\n", header);
  strncpy(partial_body, header, strlen(header));
  if (filename[strlen(root)] == '.' || filename[strlen(filename) - 1] == '/')
  {
    bad_request(socket);
    return;
  }
  //need to find the length of body and boundary
  char *tmp = NULL;
  tmp = strstr(partial_body, "Content-Length:");
  if (tmp == NULL)
  {
    bad_request(socket);
    return;
  }
  sscanf(tmp, "Content-Length: %d", &length);
  printf("Length is %d\n", length);
  char *tmp2 = NULL;
  tmp2 = strstr(partial_body, "Content-Type: multipart/form-data;");
  if (tmp2 == NULL)
  {
    bad_request(socket);
    return;
  }
  sscanf(tmp, "Content-Type: multipart/form-data; boundary=%s", boundary);
  printf("Boundary is %s\n", boundary);
  //need to find the length of body and boundary

  //parse the header part --no need here

  //get the main body to request
  char *body = malloc(length);
  int read = fread(body, 1, length, stream);
  printf("Read %d out of %d bytes\n", read, length);

  char *total = malloc(strlen(partial_body) + length);
  memcpy(total, partial_body, strlen(partial_body));
  memcpy(total + strlen(partial_body), body, length);

  if (!existance_permission_checking_put(socket, filename))
    return;

  multipartparser_callbacks callbacks;
  multipartparser parser;
  multipartparser_callbacks_init(&callbacks); // It only sets all callbacks to NULL.
  post_fd = open(path_to_file, O_WRONLY | O_TRUNC | O_CREAT, 0666);
  callbacks.on_data = &on_data;
  multipartparser_init(&parser, boundary);
  multipartparser_execute(&parser, &callbacks, total, strlen(partial_body) + length);
  printf("post_fd is now %d\n", post_fd);

  fprintf(stream, "HTTP/1.1 200 OK\n");
  fprintf(stream, SERVER_STRING);
  fprintf(stream, "Content-length: %d\n", length);
  fprintf(stream, "Content-type: multipart/form-data\n");
  fprintf(stream, "\r\n");
  fflush(stream);
  free(body);
  free(partial_body);
  free(total);
  return;
}

void success_dir(FILE *stream, int client, char *list_message)
{
  //FILE *stream = fdopen(client, "r+");
  fprintf(stream, "HTTP/1.1 200 OK\n");
  fprintf(stream, SERVER_STRING);
  fprintf(stream, "You are requiring a directory, here are all files in it\n");
  fprintf(stream, "%s", list_message);
  fprintf(stream, "\r\n");
  fflush(stream);
}

void success(FILE *stream, int client, char *filename)
{
  char *filetype = malloc(BUF_SIZE + 1);
  filetype[BUF_SIZE] = '\0';
  struct stat stat_buf;
  //FILE *stream = fdopen(client, "r+");
  int fd = open(filename, O_RDONLY);
  file_type(filename, &filetype);
  fstat(fd, &stat_buf);
  /*successful HTTP header */
  fprintf(stream, "HTTP/1.1 200 OK\n");
  fprintf(stream, SERVER_STRING);
  fprintf(stream, "Content-length: %d\n", (int)stat_buf.st_size);
  fprintf(stream, "Content-type: %s\n", filetype);
  fprintf(stream, "\r\n");
  fflush(stream);
  free(filetype);
  //close(fd);
  //fclose(stream);
}

void unimplemented(int client)
{
  char *buf = malloc(BUF_SIZE + 1);
  buf[BUF_SIZE] = '\0';
  /* reponse server can not run this request, feature not be implemented*/
  sprintf(buf, "HTTP/1.0 501 Unimplemented Error\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "Content-type: text/html\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "The request you sent has not been implemented\r\n");
  send(client, buf, strlen(buf), 0);
  free(buf);
}

void permission_deny(int client)
{
  char *buf = malloc(BUF_SIZE + 1);
  buf[BUF_SIZE] = '\0';
  /* reponse server can not run this request, feature not be implemented*/
  sprintf(buf, "HTTP/1.0 403 Permission Deny Error\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "Content-type: text/html\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "Don't have permission to open directory in the path or write to target file\r\n");
  send(client, buf, strlen(buf), 0);
  free(buf);
}
void not_found(FILE *stream, int client, char *filename)
{
  char *buf = malloc(BUF_SIZE + 1);
  buf[BUF_SIZE] = '\0';
  //FILE *stream = fdopen(client, "r+");
  /* 404 page */
  /*server information*/
  fprintf(stream, "HTTP/1.1 404 NOT fOUND \n");
  fprintf(stream, SERVER_STRING);
  fprintf(stream, "Content-name: %s\n", filename);
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "your request because the resource specified\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "is unavailable or nonexistent.\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "</BODY></HTML>\r\n");
  send(client, buf, strlen(buf), 0);
  free(buf);
}

void bad_request(int client)
{
  char *buf = malloc(BUF_SIZE + 1);
  buf[BUF_SIZE] = '\0';

  /*回应客户端错误的 HTTP 请求 */
  sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "Content-type: text/html\r\n");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "\r\n");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "<P>Your browser sent a bad request, ");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "such as a POST without a Content-Length.\r\n");
  send(client, buf, sizeof(buf), 0);
  free(buf);
}

int startup(unsigned short *port)
{
  int http = 0;
  struct sockaddr_in server;

  http = socket(PF_INET, SOCK_STREAM, 0);
  if (http == -1)
  {
    perror("socket can not be created\n");
    exit(1);
  }
  memset(&server, 0, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_port = htons(*port);
  server.sin_addr.s_addr = htonl(INADDR_ANY);

  //sets options associated with a socket --don't quite understand
  if (setsockopt(http, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
  {
    perror("setsockopt(SO_REUSEADDR) failed");
    exit(1);
  }

  if (bind(http, (struct sockaddr *)&server, sizeof(server)) < 0)
  {
    perror("bind failed!");
    exit(1);
  }

  // Listen for connections on this socket
  if (listen(http, 5) != 0)
  {
    perror("listen failed!");
    exit(1);
  }
  return http;
}

//helper function to get file type from an url
void file_type(char *filename, char **filetype)
{
  if (strstr(filename, ".html"))
  {
    strcpy(*filetype, "text/html");
  }
  else if (strstr(filename, ".gif"))
  {
    strcpy(*filetype, "image/gif");
  }
  else if (strstr(filename, ".jpg"))
  {
    strcpy(*filetype, "image/jpg");
  }
  else if (strstr(filename, ".pdf"))
  {
    strcpy(*filetype, "application/pdf");
  }
  else if (strstr(filename, ".css"))
  {
    strcpy(*filetype, "text/css");
  }
  else if (strstr(filename, ".js"))
  {
    strcpy(*filetype, "application/javascript");
  }
  else if (strstr(filename, ".json"))
  {
    strcpy(*filetype, "application/json");
  }
  else if (strstr(filename, ".png"))
  {
    strcpy(*filetype, "image/png");
  }
  else if (strstr(filename, ".zip"))
  {
    strcpy(*filetype, "application/zip");
  }
  else if (strstr(filename, ".tar"))
  {
    strcpy(*filetype, "application/x-tar");
  }
  else if (strstr(filename, ".xml"))
  {
    strcpy(*filetype, "application/xml");
  }
  else if (strstr(filename, ".rtf"))
  {
    strcpy(*filetype, "application/rtf");
  }
  else if (strstr(filename, ".doc"))
  {
    strcpy(*filetype, "application/msword");
  }
  else if (strstr(filename, ".xls"))
  {
    strcpy(*filetype, "application/vnd.ms-excel");
  }
  else if (strstr(filename, ".csv"))
  {
    strcpy(*filetype, "text/csv");
  }
  else
  {
    strcpy(*filetype, "text/plain");
  }
}

int url_callback(http_parser *parser, const char *at, size_t len)
{
  strncpy(((parsed_data_t *)(parser->data))->url, at, len);
  return 0;
}

int main(int argc, char **argv)
{
  int server_socket = -1;
  int client_socket = -1;
  port = 8080; /* default port is 8080 */
  struct sockaddr_in client_name;
  unsigned int client_name_len = sizeof(client_name);
  pthread_t newthread; //used to deal with multiple requests
  //int n; //test for socket

  if (argc != 3)
  {
    printf("Usage: %s <port-number> <root-directory>\n", argv[0]);
    exit(1);
  }

  port = atoi(argv[1]);
  root = argv[2];

  memset(&client_name, 0, sizeof(client_name)); //clean allocated client_name
  //setup the http sever in stratup
  server_socket = startup(&port);
  printf("Team ZTJ sever is running on port %d\n", port);

  while (1)
  {
    //socket receive the linking request by client
    client_socket = accept(server_socket, (struct sockaddr *)&client_name, &client_name_len);
    if (client_socket == -1)
    {
      perror("problem of accepting client request, quit\n");
      exit(1);
    }
    bzero(buffer, 256);
    /*n = read(client_socket,buffer,255);
     	if (n < 0) perror("ERROR reading from socket");
     	printf("Here is the message: %s\n",buffer);
     	n = write(client_socket,"I got your message.\n",20);
    	if (n < 0) perror("ERROR writing to socket"); */

    //use created thread to handle request
    //multiple clients will need to wait all clients send its message -- problem
    long client_socket_tmp = (long)client_socket;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&newthread, NULL, accept_request, (void *)client_socket_tmp) != 0)
      perror("pthread_create failed");
    //pthread_join(newthread, NULL);
    //close(client_socket);
  }
  pthread_exit(NULL);
  close(server_socket);

  return (0);
}
