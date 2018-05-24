---
layout: project
title:  "Project 1G: Implementing a File Server"
date:   2018-2-10
due: 2018-2-25
categories: project assignment
permalink: /project/2
---

This is a snapshot of the assignment description on 2/10/18. The
course webpage **will likely change**. Refer to the course webpage for
updated project specs.

**Caution!** This code will be graded on the virtual machine for the
course. If your code does not run on that virtual machine, you will
not be counted as receiving credit.

In this project, you'll implement a web application that serves and
retrieves files. Part of this server was implemented in the previous
project, but it didn't actually do the work of storing files on disk
and properly responding to HTTP requests.

This portion of the project is group work. During the course of this
project you are allowed access to all online resources, but you are
required to cite any resource that gave you any significant insight
into the project, including conversations you had with those outside
of your group. You will put these sources in `SOURCES.md` within this
folder.

This project consists of two broad parts: attacks and
implementation. The attacks section will have you use ROP to get
around the NX bit and defeat modern defenses. The implementation
section will have you use defensive programming to implement the file
server. You can choose to divvy up these tasks in whatever way makes
sense for your group: you can all work on different parts, or work on
each part together. But by the end of the project, every team member
is responsible for understanding each part of the project (under
penalty of not understanding well and then getting a bad grade on the
exam). I recommend not getting *stuck* on one part, though, since they
are logically independent.

Please respect the department's collaboration policy. Specifically,
you are not allowed to look at any other group's *code* (or do
anything equivalent, such as talking through your code on a
line-by-line basis). You may discuss pseudo-code on the board, but
afterwards you must erase it (so as to not let anyone else see it) and
then cite your conversation with the other person in a comment in your
code (and in the sources file). **Within** your group you may
collaborate in whatever way you want.

**If you get stuck, look at the hints / advice at the bottom of this
guide.**

# Project Overview

The goal of this project is to teach you the following:
- Launching a ROP-based exploit that defeats ASLR, NX, and stack canaries
- Defensive Programming
- Writing an HTTP server in a native language

You should first obtain the starter files for this project, perform a
`git clone` of this project's repository:

    git clone github.com/security-course/file-server-group

The starter code is contained in a file `server.c`. But in the later
part of the project, you'll ditch that starter code and instead write
your own server using whatever libraries you want.

### Collaboration Policy

Specific examples that extend the collaboration policy for this lab:
Same as on Project 1I, except replace ever occurrence of "other
students" with "other groups." This is to say, you may say whatever
you want to your groupmembers, but shouldn't talk about low-level
details and addresses with other groups. I was quite satisfied with
the level of collaboration that happened on the last project, so keep
it up.

## Attacks

In this part, we'll be launching attacks against a security-hardened
version of the server. Our server still contains vulnerabilities, but
for this project ASLR will be turned **on**. I'll also be turning on
the NX bit, meaning you can't inject code into the program and execute
it.

### Part 0: Exploiting ASLR

In this part, you should look carefully at the implementation of
`server.c` and think about how you could get the server to return to
you the contents of the file `/proc/self/maps`. This will allow you to
defeat ASLR since you'll be able to learn the base address of certain
values.

**Deliverable**: The function `dump_proc_self_maps` in
`client.py`. This function--when run--prints the contents of the
server's `/proc/self/maps` to the screen of the *client*. (I.e., if I
run it in python I should see the address on the server.)

### Part 1: Mounting a Ret-to-libc attack

In this part, you're just going to cause the server to shut down. This
is a [denial of service
(DOS)](https://en.wikipedia.org/wiki/Denial-of-service_attack)
attack. You can't inject code into the program to make it run `exit`,
so instead you'll have to reuse the function `exit`, which is already
a part of `glibc`. Find it and run that.

**Deliverable**: The function `server_exit` in `client.py`. This
function--when run--causes the server to call `exit` via a
return-to-libc attack.

Note that this will be run with ASLR turned **on**. This means that
you'll have to parse the contents of `/proc/self/maps` you get from
the previous part.

**Note**: In this version of `server.c`, the `echo` command has
changed slightly. Instead of the command being

```
echo hello
```

The new command is:

```
echo 5 hello
```

The format is `echo NNNNN data`, where `NNNNN` is a number of at most
five characters in length, followed by a space, followed by the
data. The number `NNNNN` specifies how much data to grab from `data`
and put into `string`. This makes it so that you can input binary data
into the buffer `string` containing zeros.

Also note that the server prints out the socket being used, which will
be helpful in subsequent parts.

### Part 2: Finding ROP gadgets

In this part of the assignment, you're going to use
return-oriented-programming to write the secret key out to the socket
so that the attacker gets it on the other end of the
wire. Essentially, you want to effect this sequence

- `%rax` contains 1
- `%rdi` contains the socket number (which you get from echo)
- `%rsi` contains the base of the secret key (which you get from
  `/proc/self/maps`)
- `%rdx` contains the length of the secret key (which doesn't really
  matter, just set it at something like 100)
- Finally, call `syscall`

To find gadgets, use the tool "Gadget Finder" in the EDB debugging
toolkit. I have included a copy of EDB on the virtual machine for the
class. For this part of the project, I recommend you either run the VM
with its GUI, or use X forwarding to forward EDB to XQuartz.

The gadget finder is available in EDB under the graphical menu
"Plugins -> ROPTool." It has a number of configurations that allow you
to select which parts of the code you want to search and which
operations you want to find.

Remember that it might be the case that there is no such gadget that
simply moves 1 into `%rax`. Instead, you might have to use a
combination of things (e.g., one that pops the top of the stack into
%rbx and then exchanges `%bx` and `%ax`).

**Deliverable**: the file `rop_gadgets.txt` that lists the various
gadgets you will use for your attack and lists how you will weave them
together.

### Part 3: Executing the gadgets

Now actually execute the attack. This attack must work even with ASLR
turned on, meaning that you'll have to construct the payload on the
fly after reading `/proc/self/maps`.

**Deliverable**: the function `rop` in `client.py` that performs a
ROP-based attack on the server to force it to dump the secret key.

### Part 4: ROP with Stack Protection

The version of the server I gave you for this assignment still has
stack canaries turned off. This makes it possible to exploit the
stack-based buffer overflows in the server. However, assuming the
stack protector were turned on, it would be harder to launch this kind
of an attack. Change the server so that it is susceptible to a
ROP-based attack but also with the stack protector turned on. Clearly
(since the stack protector is on) you won't be able to use a
stack-based buffer overflow (at least one of the form
`strcpy(buffer)`).

**Deliverable**: a file `server_exploitable_canaries.c` that is
exploitable with canaries turned on, and a description of how it would
be exploited (either as an attack via a Python function like the
previous parts, or a text-based description). Do your vulnerability
writeup in a file `how_to_exploit_canaries.txt` and possibly link to
any functions that launch the exploit.

## Defenses / Implementation

The second part of your project will be to implement an HTTP-based web
app that acts as a file server and accepts HTTP verbs. Your file
server will serve and store files on disc. In the next assignment, you
will use this file server to store archive files (or you can throw it
away and reimplement it in Python, using this project as a learning
experience). During this part, you'll also document different
security-related problems you thought about in code and how you
designed to confront them.

Along with your group, I recommend you read [HTTP Made Really
  Easy](https://www.jmarshall.com/easy/http/). It doesn't cover
  everything, but it covers enough to give you the gist.

For this project, you'll be writing in any native language. I've
provided starter code in C. But you can choose to reimplement it in
C++ if you'd like (though the easiest thing to do would be to write in
C). If you'd rather, you can also write in Rust, Go, etc... as long as
it works with the tests scripts (which will call `make`). I expect
most teams will write in C (I did while completing this project), and
if you choose to use another language I am not obligated to support
your learning it. If you want to be adventurous and choose another
(native) language, don't let me stop you. Just make sure it works with
all of the tests.

**Careful**: don't just start extending `server.c` like you might be
tempted to. There is some advice at the end of this document that
details picking an HTTP library.

### Server setup

Your server must run as a binary file named `fs`. It must accept
**two** command-line arguments: the first is the port on which it will
server data. The second is the root directory from which it will serve
files. An example run of the server--and the one I will use in my
tests--looks like this:

    ./fs 8080 ./files

This will tell the server to start serving files from the `files`
directory on port 8080. Subsequently, I will refer to this
command-line argument as the "root directory."

### Requirements

The goal of this project is to write a web server which serves files
on a given port. Your server must accept four "verbs" in the HTTP
protocol and behave as described in the following subparts. Note that
in each of these subparts, the headline is only the verb, not the
headers or body that follows it. If you're stuck here, read about how
HTTP works: it will be useful for you to know about anyway.

#### `GET /path/to/file HTTP/1.1`

The server must respond to HTTP `GET` requests and return files in a
number of MIME types. To see an example of this, start the server like
this:

`./fs 8080 ./files secret-key`

Now open a web browser and point it at `localhost:8080`. You should
see a web page and a gif of a cat. Your job is to make sure that you
implement this HTTP request correctly (i.e., fixing all of the bugs
present in my implementation). You must also support more file types
than I am currently supporting: the MIME types need to be lined up
appropriately.

An example GET request looks like this:

```
GET /sample.pdf HTTP/1.1
Host: localhost:8080
Connection: keep-alive
Accept-Encoding: gzip, deflate
Accept: */*
User-Agent: python-requests/2.18.4
```

The constraints for `GET` are as follows:

- [5 points] When the client (either a `telnet` user, a web browser, a
  Python script, etc..) issues a `GET /path/to/file` command to your
  server, your server should respond by looking up the file
  `<root-directory>/path/to/file`.

- [2 points] If the file exists, it must *serve* that file in a
  response and responsd with a `200 OK`.

- [2 points] If the file does *not* exist, it must respond with a `404
  Not Found` and list the file which was not found.

- [2 points] If the file exists, but is *not* world readable, your
  server **must** return a `403 Forbidden`.

- [2 points] If--in looking up the file--you encounter a file which is
  not world-executable, you must return a `403 Forbidden`. For
  example, say that there is a file `files/myfolder/foo.txt`, and that
  `myfolder`'s permissions are `770` (i.e., readable, writable, and
  executable by user and group, but not readable, writable, or
  executable by anyone else) then you must return `403`, even if
  `foo.txt` is world readable.

- [5 points] Your server **must never** allow any request for any file
  that is in any subdirectory of the root directory. For example, it
  had better not leak `../../../../proc/self/maps` (or something like
  that).

- [5 points] In the case that the user attempts to `GET` a folder, the
  server must do something a bit special. It must return an HTML page
  that returns a list of links to files within the folder. For
  example, if the folder `/path` contains the files `foo` and `bar`,
  it should return a page something like this

```
<html>
<head><title>/path</title></head>
<body><h1>/path</h1>
<ul>
    <li><a href="localhost:8080/path/foo">foo</a></li>
    <li><a href="localhost:8080/path/bar">bar</a></li>
</ul>
</body>
</html>
```

#### `POST /path/to/file HTTP/1.1`

To allow the user to upload files, your server will respond to a POST
request. A POST request is an instruction that tells the server that
the client wants to create something on the server. In this case, that
will be a file.

Here's an example of what a POST to our server looks like:

```
POST /examplepdf.pdf HTTP/1.1;
Host: localhost:8080
Connection: keep-alive
Accept-Encoding: gzip, deflate
Accept: */*
User-Agent: python-requests/2.18.4
Content-Length: 54982
Content-Type: multipart/form-data; boundary=d6e135d1e0f243bf8a931baf7e3ae35e

--d6e135d1e0f243bf8a931baf7e3ae35e
Content-Disposition: form-data; name="file"; filename="sample.pdf"

PDF-1.2
6 0 obj
<< /S /GoTo /D (chapter.1) >>
endobj
8 0 obj
(Template)
endobj
10 0 obj
<< /S /GoTo /D (section.1.1) >>
endobj
12 0 obj
(How to compile a texttt {.tex} file to a texttt {.pdf} file)
endobj
14 0 obj
...
```

- [10 points] `POST /path/to/file` should accept multipart-encoded
  data.

- `POST` is a fairly flexiblt HTTP verb, allowing a very general
  interaction with the server. You don't need to know everything about
  it to accomplish the tasks in this project, but reading about it
  might help make it more concrete.

- [2 points] Similarly to GET, POST must return a `403` if the request
  traverses into a folder that is not world-executable or tries to
  write a file that is not world writable.

- [2 points] POST should return `400` (invalid request) if the
  filename that the user tries to write is invalid.

- [5 points] In the case that the `POST` tries to put the file in a
  folder that does not yet exist, the folder should be created. So,
  for example, if the client `POST`s data to `/path/to/file` and only
  the folder `path` exists, the folder `to` should be created (as long
  as `path` is executable and writable).

#### `PUT /path/to/file HTTP/1.1`

[2 points] This should do the same thing as POST, except that in the
case the file does not yet exist it should return a `400`.

#### `DELETE /path/to/file`

[5 points] As long as the permissions are correct (i.e., the file does
not lie on a path wherein you traverse into a folder that isn't
world-executable, and the ultimate file and folder are world
writable), the file will be deleted. Return `404` in the case the file
does not exist, or `403` in the case of a permissions violation.

#### Summary of commands

- `GET /path/to/file` Fetches a file, ensuring permissions are
  correct. Also note that for directories, GET will return an HTML
  page that gives links to the files contained within the directory.

- `POST /path/to/file` Uploads a file, ensuring that permissions are
  correct and creating necessary subdirectories. Multipart-encoded
  form data must be supported. See the test in `test_post_file.py`.

- `PUT /path/to/file` Updates an alread-existing file.

- `DELETE /path/to/file` Deletes an existing file

### Preventing External Traffic 

This part is worth 5 points.

Your server is very vulnerable. You've now made something that allows
anyone that can talk to your computer to be able to store and read
files on it. This is certainly a bad idea. So you should turn it off.

Read this guide and figure out how to block incoming and outgoing
traffic to port `8080` (or whatever port you decide to use). (Note
that you might need to read other guides too, generally: google around
for how to do this.)

https://www.thegeekdiary.com/centos-rhel-how-to-block-incoming-and-outgoing-ports-using-iptables/

When you figure it out, write a shell script `block_ports.sh` that do
this.

The reason this is important is that when we move on to the next part
of the assignment you'll write a chat server. Your chat server will
eventually hook up to this server to store the files.

### Denial-Of-Service (DOS) Prevention

This part is worth 6 points.

Right now, if I open up a `telnet` connection to the server, the
server will be "tied up" in the sense that I can't open up another
connection to the server in another terminal. This is bad: it means
that an attacker could take the server offline simply by taking too
long to complete their request.

You must figure out how to solve this problem. To do so, you should
use either processes or threads to handle each incoming connection
into the server. This will make your server a bit better. Of course,
industrial-strength server setups also include a number of other
network-based protection mechanisms (such as blocking whole swaths of
IP ranges when they flood the server with too many requests).

## Server tests and Example Implementation in Python

There are a few tests you can execute against your server. Be careful
to modify the tests so that they point at the proper port.

- `test_get.py`
- `test_get_harder.py`
- `test_post_file.py`
- `test_put_file.py`

Note that I am **not** giving you every single test I will use. This
is by design. I have given you a specification I will use, and will
test your submission using various combinations of this and the tool
[Postman](https://www.getpostman.com/).

You are required to write your own tests for this project, possibly in
Python, shell scripts, etc.. While I will not explicitly grade these,
I reserve the right to lower your grade if you don't have at least a
few tests for big features.

To give you an idea of how the assignment should work, I have written
an example implementation in Python. It is in the file `server.py`. I
recommend you look it over and try to see what it's doing. Your
implementation should have a similar flavor. But it will probably be
quite a bit more work, since you'll need to figure out which libraries
to use, and C is just going to be more verbose than Python no matter
what.

Note that you'll have to install a few python packages to use that
server. You'll have to do a `sudo pip install` to get them. I have a
file `setup_tests.sh` that will do this.

## Notes on Ethics and "realness" 

You would not want to use this server in practice, because it's just
an HTTP server. In particular, anyone looking at your connection will
be able to snoop all of the data you transfer (including the
files). One way to fix this is to use HTTPS, which runs HTTP over
[SSL](https://en.wikipedia.org/wiki/Transport_Layer_Security). This
allows all of the communication with the server to be encrypted. Your
server does *not* implement SSL/TLS. So if you want to use it to store
files for the next part of the assignment, the smart thing to do is to
wall off the port and use it only for local traffic.

## Compiling Your Code

You might ask how you compile your code. This is your job to figure
out. I would most prefer it if you use a `Makefile`. If you're using
libraries, you better include them. Basically, I need to be able to
figure out how to build your code easily, and it is your job to figure
out how to do it. If in doubt, ask the people in the class that have
more experience with low-level programming. It is totally fine to
collaborate on things like Makefiles at any extent, as long as you
don't collaborate on the main source files (outside of your group:
within your group do whatever you want).

## Deliverables

_Note_: Deliverables will be accepted in no other format than those
listed here. Specifically: if you write code that technically works,
but does not fit the format the testing scripts expect, you will not
receive points. Also note that *no partial credit* will be assigned
throughout the course as a matter of policy, unless otherwise noted
explicitly.

Attacks: 

Part 0: Exploiting ASLR 
- [ ]/5 The function `dump_proc_self_maps` in `client.py`

Part 1: Mounting a Ret-to-libc attack
- [ ]/10 The function `exit` in `client.py`

Part 2: Finding ROP gadgets
- [ ]/10 The file `rop_gadgets.txt`

Part 3: Launching the attack
- [ ]/10 The function `rop_exit`

Part 4: ROP with Stack Protection
- [ ]/5 The file `server_exploitable_canaries.c` and friends

Defenses: 

Part 0: `GET``

- [ ]/5 Basic file lookup

- [ ]/2 If the file exists, it must *serve* that file in a response
  and responsd with a `200 OK`.

- [ ]/2 If the file does *not* exist, it must respond with a `404
  Not Found` and list the file which was not found.

- [ ]/2 If the file exists, but is *not* world readable, your
  server **must** return a `403 Forbidden`.

- [ ]/2 If--in looking up the file--you encounter a file which is
  not world-executable, you must return a `403 Forbidden`. For
  example, say that there is a file `files/myfolder/foo.txt`, and that
  `myfolder`'s permissions are `770` (i.e., readable, writable, and
  executable by user and group, but not readable, writable, or
  executable by anyone else) then you must return `403`, even if
  `foo.txt` is world readable.

- [ ]/5 Your server **must never** allow any request for any file
  that is in any subdirectory of the root directory. For example, it
  had better not leak `../../../../proc/self/maps` (or something like
  that).

- [ ]/5 In the case that the user attempts to `GET` a folder, the
  server must do something a bit special. It must return an HTML page
  that returns a list of links to files within the folder. For
  example, if the folder `/path` contains the files `foo` and `bar`,
  it should return a page something like this

Part 1: `POST`

- [ ]/10 `POST /path/to/file` should accept multipart-encoded
  data.

- [ ]/2 POST must return a `403` if the request traverses into a folder
  that is not world-executable or tries to write a file that is not
  world writable.

- [ ]/2 POST should return `400` (invalid request) if the filename
  that the user tries to write is invalid.

- [ ]/5 In the case that the `POST` tries to put the file in a folder
  that does not yet exist, the folder should be created. So, for
  example, if the client `POST`s data to `/path/to/file` and only the
  folder `path` exists, the folder `to` should be created (as long as
  `path` is executable and writable).

Part 2: `PUT`

- [ ]/2 This should do the same thing as POST, except that in the case
the file does not yet exist it should return a `400`.

Part 3: `DELETE`

- [ ]/5 Implementation of DELETE as described above.

Part 4: [ ]/5 Preventing External Traffic 

Part 5: [ ]/6 DOS Protection

Total: [ ]/100

## Personal Advice on Accomplishing this Project

With the proper selection of tools, this assignment should not be an
overwhelming amount of work. In particular, avoid parsing HTTP
manually: that's error prone and it's going to get complicated
fast. You likely have not had extensive exposure to HTTP before this
project, and possibly not extensive socket programming experience
either. Because of that, make sure you start early. Budget time to
learn about HTTP. Most of the time, the best way to figure things out
is to start hacking on something.

Right now, the server is written to manually parse the HTTP
request. In general, you shouldn't do that: you're going to expend a
ton of time and effort and probably get it wrong. Instead, I recommend
that you use a library that parses HTTP requests for you. Here are a
few I think might help you:

- https://github.com/nodejs/http-parser
- https://github.com/h2o/picohttpparser
- https://github.com/iafonov/multipart-parser-c
- https://github.com/Samsung/http-parser
- https://github.com/francoiscolas/multipart-parser
- https://github.com/soywod/c-request
- https://github.com/nekipelov/httpparser
- https://github.com/AndreLouisCaron/httpxx
- https://github.com/c9s/h3

Most of these parsers are callback-based: you set them up and they
call back a function whenever you've read all of the data. So--for
example--you can set them up to call one of your functions when
they've read all of the `POST` request, and then you get a pointer to
it (and its length), which you can use to write it to a file.

Remember that the point of this project is to practice writing secure
code. Using other people's libraries can be helpful: if they've
written code well, then it saves you time. But if you pull some random
student's HTTP parser off github and use it in your project, you
should be mindful it might have bugs: these will come back to bite you
(i.e., *lower your grade*) later in the "break it" phase of the final
project when other students find bugs in the code you
included. Therefore, make sure to read over the code you include.

## Resources and Advice

- I recommend you read [HTTP Made Really
  Easy](https://www.jmarshall.com/easy/http/)
- You'll also need to read about HTTP multipart POSTs: 
- https://en.wikipedia.org/wiki/List_of_HTTP_status_codes#4xx_Client_errors

