//
// Created by Shik Hur on 2022-10-28.
//

#ifndef UDP_SERVER_UDP_LCD_H
#define UDP_SERVER_UDP_LCD_H


void write_word(int data);

void send_command(int comm);

void send_data(int data);

void init();

void clear();

void writeToLCD(int x, int y, char data[]);


#endif //UDP_SERVER_UDP_LCD_H
