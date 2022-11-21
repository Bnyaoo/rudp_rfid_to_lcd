#include "../include/udp_receiver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>

int LCDAddr = 0x27;
int BLEN = 1;
int fd;

void write_word(int data){
	int temp = data;
	if ( BLEN == 1 )
		temp |= 0x08;
	else
		temp &= 0xF7;
	wiringPiI2CWrite(fd, temp);
}

void send_command(int comm){
	int buf;
	// Send bit7-4 firstly
	buf = comm & 0xF0;
	buf |= 0x04;			// RS = 0, RW = 0, EN = 1
	write_word(buf);
	delay(2);
	buf &= 0xFB;			// Make EN = 0
	write_word(buf);

	// Send bit3-0 secondly
	buf = (comm & 0x0F) << 4;
	buf |= 0x04;			// RS = 0, RW = 0, EN = 1
	write_word(buf);
	delay(2);
	buf &= 0xFB;			// Make EN = 0
	write_word(buf);
}

void send_data(int data){
	int buf;
	// Send bit7-4 firstly
	buf = data & 0xF0;
	buf |= 0x05;			// RS = 1, RW = 0, EN = 1
	write_word(buf);
	delay(2);
	buf &= 0xFB;			// Make EN = 0
	write_word(buf);

	// Send bit3-0 secondly
	buf = (data & 0x0F) << 4;
	buf |= 0x05;			// RS = 1, RW = 0, EN = 1
	write_word(buf);
	delay(2);
	buf &= 0xFB;			// Make EN = 0
	write_word(buf);
}

void init(){
	send_command(0x33);	// Must initialize to 8-line mode at first
	delay(5);
	send_command(0x32);	// Then initialize to 4-line mode
	delay(5);
	send_command(0x28);	// 2 Lines & 5*7 dots
	delay(5);
	send_command(0x0C);	// Enable display without cursor
	delay(5);
	send_command(0x01);	// Clear Screen
	wiringPiI2CWrite(fd, 0x08);
}

void clear(){
	send_command(0x01);	//clear Screen
}

void writeToLCD(int x, int y, char data[]){
	int addr, i;
	int tmp;
	if (x < 0)  x = 0;
	if (x > 15) x = 15;
	if (y < 0)  y = 0;
	if (y > 1)  y = 1;

	// Move cursor
	addr = 0x80 + 0x40 * y + x;
	send_command(addr);
	
	tmp = strlen(data);
	for (i = 0; i < tmp; i++){
		send_data(data[i]);
	}
}


int init_sockaddr(struct sockaddr_in *addr, const struct options *opts)
{
    addr->sin_family = AF_INET;
    addr->sin_port = htons(opts->port_in);
    addr->sin_addr.s_addr = opts->ip_in ? inet_addr(opts->ip_in) : htonl(INADDR_ANY);

    if(addr->sin_addr.s_addr == (in_addr_t) -1) // NOLINT(clang-analyzer-core.UndefinedBinaryOperatorResult)
    {
        return MY_FAILURE_CODE;
    }
    return MY_SUCCESS_CODE;
}

int do_server(struct options *opts, struct sockaddr_in *proxy_addr)
{
    rudp_packet_t packet;
    socklen_t from_addr_len = sizeof(struct sockaddr_in);
    long current_seq_no = -1;

    ssize_t nread;
    char buffer[MAX_DATA_LENGTH];
    fprintf(stdout, "[Listening on port %d]\n", opts->port_in); // NOLINT(cert-err33-c, concurrency-mt-unsafe)
    fd = wiringPiI2CSetup(LCDAddr);
    init();
    writeToLCD(0, 0, "Msg Received:");
    writeToLCD(0, 1, "");
    do
    {
        nread = recvfrom(opts->sock_fd, &packet, sizeof(rudp_packet_t), 0, (struct sockaddr *)proxy_addr, &from_addr_len);
        if (nread == -1)
        {
            return MY_FAILURE_CODE;
        }

        deserialize_packet(&packet);
        rudp_header_t response_packet_header;
        rudp_packet_t *response_packet;
        if (packet.header.packet_type == RUDP_SYN)
        {
            if (packet.header.seq_no == 0 && current_seq_no == -1)
            {
                fprintf(stdout, "[Start receiving a message from client]\n"); // NOLINT(cert-err33-c, concurrency-mt-unsafe)
                current_seq_no = 0;
            }

            // check if the data is corrupted or not.
            uint16_t check_sum_of_data_in_pack;
            check_sum_of_data_in_pack = generate_crc16(packet.data, packet.data_length);
            if (check_sum_of_data_in_pack != packet.check_sum)
            {
                init_rudp_header(RUDP_NAK, packet.header.seq_no, &response_packet_header);
                response_packet = create_rudp_packet_malloc(&response_packet_header, 0, NULL);
                sendto(opts->sock_fd, response_packet, sizeof(rudp_packet_t), 0, (const struct sockaddr *) proxy_addr, sizeof(struct sockaddr_in));
                free(response_packet);
                continue;
            }

            // if the sender sent the same packet with previous one, send ACK to client again for the same packet.
            // this situation only happens when the client can not receive the ACK of the packet from the server
            if (packet.header.seq_no != current_seq_no)
            {
                init_rudp_header(RUDP_ACK, packet.header.seq_no, &response_packet_header);
                response_packet = create_rudp_packet_malloc(&response_packet_header, 0, NULL);
                sendto(opts->sock_fd, response_packet, sizeof(rudp_packet_t), 0, (const struct sockaddr *) proxy_addr, sizeof(struct sockaddr_in));
                free(response_packet);
                continue;
            }

            // copy data from the client, print it on stdout
            strncpy(buffer, packet.data, packet.data_length + 1);
            buffer[packet.data_length + 1] = '\0';
            fprintf(stdout, "\t%s\n", buffer);       
            fd = wiringPiI2CSetup(LCDAddr);
            init();
            writeToLCD(0, 1, buffer);

            // send ACK
            init_rudp_header(RUDP_ACK, packet.header.seq_no, &response_packet_header);
            response_packet = create_rudp_packet_malloc(&response_packet_header, 0, NULL);
            sendto(opts->sock_fd, response_packet, sizeof(rudp_packet_t), 0, (const struct sockaddr *) proxy_addr, sizeof(struct sockaddr_in));

            // free response packet and increase current_seq_no
            current_seq_no++;
        }
        else if (packet.header.packet_type == RUDP_FIN)
        {
            if (current_seq_no != -1)
            {
                fprintf(stdout, "[Finish the message transmission]\n");    // NOLINT(cert-err33-c)
            }
            // send ACT to FIN
            init_rudp_header(RUDP_ACK, packet.header.seq_no, &response_packet_header);
            response_packet = create_rudp_packet_malloc(&response_packet_header, 0, NULL);
            sendto(opts->sock_fd, response_packet, sizeof(rudp_packet_t), 0, (const struct sockaddr *) proxy_addr, sizeof(struct sockaddr_in));
            current_seq_no = -1;
        }
        else
        {
            continue;
        }

        // free allocated memory (response_packet)
        free(response_packet);
    } while (nread > 0);

    return MY_SUCCESS_CODE;
}

