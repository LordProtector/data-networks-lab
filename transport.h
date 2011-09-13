/**
 * link.h
 *
 * @autors Stefan Tombers, Alexander Bunte, Jonas Bürse
 *
 * Header file for transport layer implementation.
 */

#ifndef TRANSPORT_H_
#define TRANSPORT_H_

void transport_transmit(CnetAddr addr, char *data, size_t size);
void transport_receive(CnetAddr addr, char *data, size_t size);
void transport_init();

#endif
