#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include "helpers.h"


void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_address server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	int sockfd, n, ret;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN];

	if (argc < 3) {
		usage(argv[0]);
	}

	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar
	int fdmax;			// valoare maxima fd din multimea read_fds

	FD_ZERO(&tmp_fds);
	FD_ZERO(&read_fds);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	FD_SET(sockfd, &read_fds);
	FD_SET(0, &read_fds);
	fdmax = sockfd;

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3])); 
	ret = inet_aton(argv[2], &serv_addr.sin_addr); 
	DIE(ret == 0, "inet_aton");

	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");

	char ID[50];
	strcpy(ID, argv[1]);


	//trimit mesaj cu comanda "NEW" pentrua crea clientul in server
	msg_client_to_server create_msg;
	memset(create_msg.command, 0, sizeof(create_msg.command));
	memset(create_msg.topic, 0, sizeof(create_msg.topic));
	memset(create_msg.id, 0, sizeof(create_msg.id));
	strncpy(create_msg.command, "NEW", 3);
	strcpy(create_msg.id, argv[1]);
	memset(buffer, 0, BUFLEN);
	memcpy(buffer, &create_msg, sizeof(msg_client_to_server));

	n = send(sockfd, buffer, BUFLEN, 0);
	DIE(n < 0, "send");

	// dezactivez algoritmul lui neagle
	int flags = 1;
	setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (void *)&flags, sizeof(flags));

	while (1) {
		tmp_fds = read_fds;
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		if(FD_ISSET(0, &tmp_fds)){
	  		// se citeste de la tastatura
			memset(buffer, 0, BUFLEN);
			fgets(buffer, BUFLEN - 1, stdin);
	
			//va trebui sa trimit comenzile valide serverului		
			msg_client_to_server msg;
			
			if (strncmp(buffer, "exit", 4) == 0) {
				memset(msg.command, 0, sizeof(msg.command));
				memset(msg.id, 0, sizeof(msg.id));
				strncpy(msg.command, "exit", 4);
				strcpy(msg.id, ID);

				char buffer1[BUFLEN];
				memset(buffer1, 0, BUFLEN);
			
				memcpy(buffer1, &msg, sizeof(msg_client_to_server));

				// se trimite mesaj la server
				n = send(sockfd, buffer1, BUFLEN, 0);
				DIE(n < 0, "send");

				sleep(1);
				close (sockfd);
				exit(0);
				return 0;
			}
			else if (strncmp(buffer, "subscribe", 9) == 0){
				char *token = strtok(buffer, " ");
				token = strtok(NULL, " ");
				if (token == NULL){
					printf("Comanda invalida, parametru lipsa\n");
					continue;
				}
				char *topic = token;
				token = strtok(NULL, " \n");
				if (token == NULL){
					printf("Comanda invalida, parametru lipsa\n");
					continue;
				}
				if (!((token[0]-'0') >= 0 && (token[0]-'0') < 9)){
					printf("Comanda invalida, SF gresit\n");
					continue;
				}
				int SF = atoi(token);
				if (SF != 0 && SF != 1) {
					printf("Comanda invalida de la SF\n");
					continue;
				}
				strncpy(msg.command, "subscribe", 9);
				strcpy(msg.topic, topic);
				msg.SF = SF;

				strcpy(msg.id, ID);

				memcpy(buffer, &msg, sizeof(msg_client_to_server));

				// se trimite mesaj la server
				n = send(sockfd, buffer, BUFLEN, 0);
				DIE(n < 0, "send");
			}
			else if (strncmp(buffer, "unsubscribe", 11) == 0){
				char *token = strtok(buffer, " ");
				token = strtok(NULL, " \n");
				char *topic = token;

				if (token == NULL){
					printf("Comanda invalida, parametru lipsa\n");
					continue;
				}

				memset(msg.id, 0, sizeof(msg.id));
				strcpy(msg.id, ID);

				strncpy(msg.command, "unsubscribe",11);
				strcpy(msg.topic, topic);

				memcpy(buffer, &msg, sizeof(msg_client_to_server));

				// se trimite mesaj la server
				n = send(sockfd, buffer, BUFLEN, 0);
				DIE(n < 0, "send");
			}
			else{
				printf("Comanda invalda\n");
				continue;
			}
			
		}
		if(FD_ISSET(sockfd, &tmp_fds)){
			//primesc mesaj de la server
			memset(buffer, 0, BUFLEN);
			n = recv(sockfd, buffer, sizeof(msg_server_to_client), 0);

			msg_server_to_client m;
			memcpy(&m, buffer, sizeof(msg_server_to_client));

			//verific daca primesc comanda de exit de la server
			if (strncmp(m.message.topic, "exit", 4) == 0 || n == 0) {
				close(sockfd);
				exit(0);
				break;
			}

			//este mesaj udp si trebuie afisat 
			printf("%s:%d - %s - ", m.ip, m.port, m.message.topic);
			// interpretez tipul de date
			if(m.message.tip_date == 0){
				char semn;
				uint32_t valoare;
				memcpy(&semn, m.message.continut, sizeof(char));
				memcpy(&valoare, m.message.continut + sizeof(char), sizeof(uint32_t));
				valoare = ntohl(valoare);
				if(semn == 1)
					valoare = - valoare;
				printf ("INT - %d\n", valoare);
			}
			else if(m.message.tip_date == 1){
				uint16_t valoare;
				memcpy(&valoare, m.message.continut, sizeof(uint16_t));
				valoare = (ntohs(valoare)) / 100; // *1.0
				printf ("SHORT_REAL - %f\n", 1.0 * valoare);
			}
			else if(m.message.tip_date == 2){
				char semn;
				uint32_t baza;
				uint8_t exponent;
				float valoare;
				memcpy(&semn, m.message.continut, sizeof(char));
				memcpy(&baza, m.message.continut + sizeof(char), sizeof(uint32_t));
				memcpy(&exponent, (m.message.continut + sizeof(char) + sizeof(uint32_t)), sizeof(uint8_t));
				valoare = ntohl(baza);
					
				for (int j = 0; j < exponent; j++)
					valoare /= 10;
				if(semn == 1)
						valoare = - valoare;
				printf ("FLOAT - %f\n", 1.0 * valoare);
			}
			else if (m.message.tip_date == 3){
				char str[1501];
				memcpy(str, m.message.continut, 1501);
				str[1500] = '\0';
				printf ("STRING - %s\n", str);
			}
			else {
				printf("Tip de date invalid\n");
			}

			
		}
	}
	close(sockfd);
	return 0;
}
