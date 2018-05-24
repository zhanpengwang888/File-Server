# All protection mechanisms on, except stack protection
fs: server.c
	gcc -o fs server.c -g -fno-stack-protector
bugfree: server_bugfree.c multipart-parser-master/multipartparser.h
	gcc -Wall -pedantic -o bugfree server_bugfree.c http-parser/http_parser.c multipart-parser-master/multipartparser.c -g -lpthread

test: test.c multipart-parser-master/multipartparser.h
	gcc -o test test.c -fno-stack-protector multipart-parser-master/multipartparser.c -g -lpthread
