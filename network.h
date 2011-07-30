#ifndef NETWORK_H_
#define NETWORK_H_

void network_transmit(CnetAddr, char *, size_t);
void network_receive(int, char *, size_t);
void network_init();

#endif
