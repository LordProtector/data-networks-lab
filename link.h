/**
 * link.h
 *  
 * @autors Stefan Tombers, Alexander Bunte, Jonas Bürse
 *
 * Header file for link layer implementation.
 */
#ifndef LINK_H_
#define LINK_H_

void link_transmit(int, char *, size_t);
void link_receive(int, char *, size_t);
void link_init();

#endif
