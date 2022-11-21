//
// Created by Benny Chao on 2022-11-21.
//

#ifndef UDP_CLIENT_RFID_H
#define UDP_CLIENT_RFID_H

#include <stdint.h>

void print_info(unsigned char *p, int cnt);

void insert_info(const unsigned char *p,int cnt, char* cardData);

int read_card();

int card_password(uint8_t auth_mode,uint8_t addr,uint8_t *Src_Key,uint8_t *New_Key,uint8_t *pSnr);

uint8_t write_card_data(uint8_t *data);

uint8_t read_card_data();


#endif //UDP_CLIENT_RFID_H
