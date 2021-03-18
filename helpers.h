#ifndef _HELPERS_H
#define _HELPERS_H 1

#include <stdio.h>
#include <stdlib.h>

/*
 * Macro de verificare a erorilor
 * Exemplu:
 *     int fd = open(file_name, O_RDONLY);
 *     DIE(fd == -1, "open failed");
 */

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

#define BUFLEN		256	// dimensiunea maxima a calupului de date
#define MAX_CLIENTS	5	// numarul maxim de clienti in asteptare

//structuri pentru tipurile de mesaje

typedef struct {
	char topic[50];
	char tip_date;
	char continut[1500];
} msg_udp_format;

typedef struct {
	char id[10];
	char command[100]; //{exit, subscribe, unsubscribe}
	char topic[50];
	int SF;
} msg_client_to_server;

typedef struct {
	char ip[16];
	int port;
	msg_udp_format message;
} msg_server_to_client;

//pentru clienti

typedef struct {
	char topic[50];
	int SF;
	msg_server_to_client *asteptare;
	int nr_netrimise;
	int cap_netrimise;  //LE AM PUS AICI!!!!!!!!!!
} subscription;

typedef struct {
	char id[10];
	int sockfd;
	int ON;
	subscription *subscriptions;
	int nr_subscriptions;
	int cap_subscriptions;
	// msg_server_to_client *asteptare;
	// int nr_netrimise;
	// int cap_netrimise;
} client;

#endif
