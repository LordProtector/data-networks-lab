/**
 * link.h
 *  
 * @autors Stefan Tombers, Alexander Bunte, Jonas Bürse
 *
 * Header file for link layer implementation.
 */
#ifndef LINK_H_
#define LINK_H_

void link_transmit(int link, char *data, size_t size);
void link_receive(int link, char *data, size_t size);
void link_init();

int link_get_bandwidth(int link);
int link_get_mtu(int link);
int link_get_queue_size(int link);

int link_num_links();

#endif
