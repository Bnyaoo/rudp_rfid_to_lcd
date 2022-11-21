#ifndef UDP_SERVER_UDP_RECEIVER_H
#define UDP_SERVER_UDP_RECEIVER_H


#define MY_FAILURE_CODE (1)
#define OPEN_SOCKET_FAILURE_CODE (2)
#define MY_SUCCESS_CODE (0)


#include "option_handler.h"
#include "rudp_types.h"

void write_word(int data);

void send_command(int comm);

void send_data(int data);

void set_LCDaddr_fd();

void clear();

void writeToLCD(int x, int y, char data[]);

/**
 * Set an ip address and port number to be used to a socket.
 * @param addr a sockaddr_in pointer which will contains the address of the host
 * @param opts a pointer to struct options
 * @return an integer represents the status code
 */
int init_sockaddr(struct sockaddr_in *addr, const struct options *opts);

/**
 * Receive a rudp packet from the proxy server and sends ACK or NAK rudp packet.
 * @param opts a pointer to struct options
 * @param proxy_addr a sockaddr_in pointer points to the address of the proxy server
 * @return an integer represents the status code
 */
int do_server(struct options *opts, struct sockaddr_in *proxy_addr);


#endif //UDP_SERVER_UDP_RECEIVER_H

