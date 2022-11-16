//
// Created by Shik Hur on 2022-10-28.
//

#include "udp_sender.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rc522.h"

unsigned char SN[4];
void print_info(unsigned char *p,int cnt);
int read_card();
int card_passworld(uint8_t auth_mode,uint8_t addr,uint8_t *Src_Key,uint8_t *New_Key,uint8_t *pSnr);
uint8_t write_card_data(uint8_t *data);
uint8_t read_card_data();
void MFRC522_HAL_Delay(unsigned int ms);
char buffer[MAX_DATA_LENGTH];

int init_sockaddr(struct sockaddr_in *addr)
{
    addr->sin_family = AF_INET;
    addr->sin_port = 0;
    addr->sin_addr.s_addr = htonl(INADDR_ANY);

    if(addr->sin_addr.s_addr == (in_addr_t) -1) // NOLINT(clang-analyzer-core.UndefinedBinaryOperatorResult)
    {
        return MY_FAILURE_CODE;
    }
    return MY_SUCCESS_CODE;
}

int init_proxy_sockaddr(struct sockaddr_in *proxy_addr, const struct options *opts)
{
    proxy_addr->sin_family = AF_INET;
    proxy_addr->sin_port = htons(opts->port_out);
    proxy_addr->sin_addr.s_addr = inet_addr(opts->ip_out);

    if (proxy_addr->sin_addr.s_addr == (in_addr_t) -1)
    {
        return MY_FAILURE_CODE;
    }
    return MY_SUCCESS_CODE;
}

int do_client(const struct options *opts, struct sockaddr_in *proxy_addr, struct sockaddr_in *from_addr)
{
    uint32_t current_seq = 0;
    //char buffer[MAX_DATA_LENGTH];
    ssize_t nwrote;
    ssize_t nread;
    rudp_packet_t response_packet;
    socklen_t from_addr_len = sizeof(struct sockaddr_in);

    uint8_t data[16]={0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x20,0x21,0x22,0x23,255,255,255,255};
    uint8_t status=1;
    InitRc522();
    memset(data,0,16);
    // grab user input from stdin
    printf("Please enter the data to be written:\r\n");
    scanf("%s", data);
    printf("Reading...Please place the card...\r\n");

    while (fgets(buffer, MAX_DATA_LENGTH, stdin) != NULL)
    {
        // write data to RFID card
        while (1) {
            /* code */
            status = write_card_data(data);
            if (status == MI_OK) {
                break;
            }
        }

        // create rudp fin packet header
        rudp_header_t header;
        header.packet_type = RUDP_SYN;
        header.seq_no = current_seq;

        // create rudp fin packet
        rudp_packet_t *packet = create_rudp_packet_malloc(&header, strlen(buffer), buffer);

        // sendto proxy server
        nwrote = sendto(opts->sock_fd, packet, sizeof(rudp_packet_t), 0, (const struct sockaddr *) proxy_addr, sizeof(struct sockaddr_in));
        if (nwrote == -1)
        {
            free(packet);
            return MY_FAILURE_CODE;
        }

wait_response_packet:
        nread = recvfrom(opts->sock_fd, &response_packet, sizeof(rudp_packet_t), 0, (struct sockaddr *)from_addr, &from_addr_len);
        if (nread == -1)
        {
            // timeout reached (3000 ms)
            fprintf(stdout, "\t|______________________________[TIMEOUT]: The packet has been sent again\n");        // NOLINT(cert-err33-c)
            nwrote = sendto(opts->sock_fd, packet, sizeof(rudp_packet_t), 0, (const struct sockaddr *) proxy_addr, sizeof(struct sockaddr_in));
            if (nwrote == -1)
            {
                free(packet);
                return MY_FAILURE_CODE;
            }
            goto wait_response_packet;
        }

        deserialize_packet(&response_packet);
        // if it receives NAK, or it receives ACK but the seq_no is not equal to the fin packet it sent, resend the fin packet.
        if (response_packet.header.packet_type == RUDP_NAK || (response_packet.header.seq_no != current_seq || response_packet.header.packet_type != RUDP_ACK))
        {
            nwrote = sendto(opts->sock_fd, packet, sizeof(rudp_packet_t), 0, (const struct sockaddr *) proxy_addr, sizeof(struct sockaddr_in));
            if (nwrote == -1)
            {
                free(packet);
                return MY_FAILURE_CODE;
            }
            goto wait_response_packet;
        }

        fprintf(stdout, "\t|______________________________[received ACK]\n");           // NOLINT(cert-err33-c)
        // increase sequence number and continue
        free(packet);
        current_seq++;
        // grab input from stdin
        printf("Please enter the data to be written:\r\n");
        scanf("%s", data);
        printf("Reading...Please place the card...\r\n");
        // write data to RFID card
        while (1) {
            /* code */
            status = write_card_data(data);
            if (status == MI_OK) {
                break;
            }
        }
    }

    // send FIN
    do
    {
        send_fin(current_seq, opts->sock_fd, proxy_addr);
        nread = recvfrom(opts->sock_fd, &response_packet, sizeof(rudp_packet_t), 0, (struct sockaddr *)from_addr, &from_addr_len);
        if (nread == -1)
        {
            fprintf(stdout, "\t|__FIN_________________________[TIMEOUT]: The FIN packet has been sent again\n");        // NOLINT(cert-err33-c)
            continue;
        }
        deserialize_packet(&response_packet);
    } while (nread == -1 || (response_packet.header.seq_no != current_seq || response_packet.header.packet_type != RUDP_ACK));

    fprintf(stdout, "\n[Finish sending messages (Received ACK for the FIN)]\n");                // NOLINT(cert-err33-c)
    return MY_SUCCESS_CODE;
}

int send_fin(uint32_t current_seq, int sock_fd, struct sockaddr_in *proxy_addr)
{
    ssize_t nwrote;
    // send FIN, get ACK
    rudp_header_t fin_header;
    init_rudp_header(RUDP_FIN, current_seq, &fin_header);
    rudp_packet_t *fin_packet = create_rudp_packet_malloc(&fin_header, 0, NULL);
    nwrote = sendto(sock_fd, fin_packet, sizeof(rudp_packet_t), 0, (const struct sockaddr *) proxy_addr, sizeof(struct sockaddr_in));
    if (nwrote == -1)
    {
        free(fin_packet);
        return MY_FAILURE_CODE;
    }
    free(fin_packet);
    return MY_SUCCESS_CODE;
}

void print_info(unsigned char *p,int cnt)
{
    int i;
    for(i=0;i<cnt;i++) {
        printf("%c",p[i]);
    }
    printf("\r\n");
}

void insert_info(unsigned char *p,int cnt, char* buffer)
{
    int i;
    for(i=0;i<cnt;i++) {
        buffer[i] = p[i];
    }
    char delim = p[cnt+1];
    buffer[cnt+1] = delim;
}

int read_card()
{
    unsigned char CT[2];
    uint8_t status=1;
    status=PcdRequest(PICC_REQIDL ,CT);

    if(status==MI_OK)
    {
        status=MI_ERR;
        status=PcdAnticoll(SN);

        printf("Card type: ");
        if(CT[0] == 0x44)
        {
            printf("Mifare_UltraLight \r\n");
        }
        else if(CT[0]==0x4)
        {
            printf("MFOne_S50 \r\n");
        }
        else if(CT[0]==0X2)
        {
            printf("MFOne_S70 \r\n");
        }
        else if (CT[0]==0X8)
        {
            printf("Mifare_Pro(X)\r\n");
        }

        printf("Card ID: 0x");
        printf("%X%X%X%X\r\n",SN[0],SN[1],SN[2],SN[3]);
    }
    if(status==MI_OK)
    {
        status=MI_ERR;
        status =PcdSelect(SN);
    }
    return status;
}

int card_passworld(uint8_t auth_mode,uint8_t addr,uint8_t *Src_Key,uint8_t *New_Key,uint8_t *pSnr)
{
    int status;

    status=read_card();

    if(status==MI_OK)
    {
        status=PcdAuthState(auth_mode,addr,Src_Key,pSnr);
    }

    if(status==MI_OK)
    {
        status=PcdWrite(addr,New_Key);
    }
    return status;
}

uint8_t write_card_data(uint8_t *data)
{
    uint8_t KEY[6]={0xff,0xff,0xff,0xff,0xff,0xff};

    int status=MI_ERR;

    status=read_card();


    if(status==MI_OK)
    {
        status=PcdAuthState(PICC_AUTHENT1A,3,KEY,SN);

    }

    if(status==MI_OK)
    {
        status=PcdWrite(2,data);
    }
    if(status==MI_OK)
    {
        printf("Write Data: ");
        print_info(data,16);
        insert_info(data, 16, buffer);
    }
    return status;
}

uint8_t read_card_data()
{
    uint8_t KEY[6]={0xff,0xff,0xff,0xff,0xff,0xff};
    int status;
    uint8_t data[16];

    status=read_card();

    if(status==MI_OK)
    {
        status=PcdAuthState(PICC_AUTHENT1A,3,KEY,SN);
    }

    if(status==MI_OK)
    {
        status=PcdRead(2,data);
    }
    if(status==MI_OK)
    {
        printf("Data: ");
        print_info(data,16);
        insert_info(data, 16, buffer);
    }
    return status;
}
