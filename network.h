/**
 * link.h
 *  
 * @autors Stefan Tombers, Alexander Bunte, Jonas Bürse
 *
 * Header file for network layer implementation.
 */

#ifndef NETWORK_H_
#define NETWORK_H_

void network_transmit(CnetAddr, char *, size_t);
void network_receive(int, char *, size_t);
void network_init();

int network_lookup(CnetAddr);
CnetAddr network_get_address();
int network_get_bandwidth(CnetAddr addr);

#endif
