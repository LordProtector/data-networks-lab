/**
 * link.h
 *  
 * @autors Stefan Tombers, Alexander Bunte, Jonas Bürse
 *
 * Header file for transport layer implementation.
 */

#ifndef TRANSPORT_H_
#define TRANSPORT_H_

void transport_transmit(CnetAddr, char *, size_t);
void transport_receive(CnetAddr, char *, size_t);
void transport_init();

#endif
