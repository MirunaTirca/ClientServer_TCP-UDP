#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "helpers.h"

#include <netinet/tcp.h>

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	int sockfdtcp, sockfdudp, newsockfd, portno;
	char buffer[BUFLEN];
	struct sockaddr_in serv_addr, cli_addr, cli_addr_udp;
	int n, i, ret;
	socklen_t clilen;

	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar
	int fdmax;			// valoare maxima fd din multimea read_fds

	if (argc < 2) {
		usage(argv[0]);
	}

	// se goleste multimea de descriptori de citire (read_fds) si multimea temporara (tmp_fds)
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	// deschid socket care sa permita conxiuni tcp
	sockfdtcp = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfdtcp < 0, "socketTCP");

	// iau portul pe care vreu sa functioneaza serverul
	portno = atoi(argv[1]);
	DIE(portno == 0, "atoi");

	// initalizez structura sockaddr_in cu parametri corespunzatoare
	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	// realizez operatiunea de bind
	ret = bind(sockfdtcp, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "bindTCP");

	// fac socketul tcp sa astepte alte conxiuni
	ret = listen(sockfdtcp, MAX_CLIENTS);
	DIE(ret < 0, "listenTCP");

	//dechid socket care sa permita conxiuni udp
	sockfdudp = socket(AF_INET, SOCK_DGRAM, 0);

	// deschid socket care sa permita conxiuni udp
	ret = bind(sockfdudp, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "bindUDP");

	// se adauga noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
	FD_SET(0, &read_fds);
	FD_SET(sockfdtcp, &read_fds);
	FD_SET(sockfdudp, &read_fds);
	if (sockfdudp > sockfdtcp)
		fdmax = sockfdudp;
	else fdmax = sockfdtcp;

	// creez vectrul de clienti tcp
	client *clients = malloc(sizeof(client) * 100);
	int no_clients = 0;
	int size_clients = 100;

	int flags = 1;
	setsockopt(sockfdtcp, IPPROTO_TCP, TCP_NODELAY, (void *)&flags, sizeof(flags));


	while (1) {

		// pentru a vedea daca am primit date de la tastatura si/sau server
		tmp_fds = read_fds; 
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");


		for (i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
				if (i == sockfdtcp) {
					// a venit o cerere de conexiune pe socketul inactiv (cel cu listen),
					// pe care serverul o accepta
					clilen = sizeof(cli_addr);
					newsockfd = accept(sockfdtcp, (struct sockaddr *) &cli_addr, &clilen);
					DIE(newsockfd < 0, "accept");

					// se adauga noul socket intors de accept() la multimea descriptorilor de citire
					FD_SET(newsockfd, &read_fds);
					if (newsockfd > fdmax) { 
						fdmax = newsockfd;
					}

				}
				else if (i == sockfdudp) {
					// primesc mesaje de pe portul udp
					clilen = sizeof(cli_addr_udp);
					ret = recvfrom(sockfdudp, buffer, BUFLEN, 0, (struct sockaddr *) &cli_addr_udp, &clilen);
					DIE(ret < 0, "recvfrom");

					//iau mesajul udp din buffer
					msg_server_to_client m; //{are mesajul in sine, ip, port}
					memcpy(&(m.message), buffer, sizeof(msg_udp_format));
					strcpy (m.ip, inet_ntoa(cli_addr_udp.sin_addr));
					m.port = ntohs(cli_addr_udp.sin_port);

					//caut clientii abonati la topic-ul din mesajul udp
					//trimit mai departe mesajul clientilor activi, iar
					//celor inactivi dar cu SF=1, adaug mesajul in lista de asteptare
					for (int j = 0; j < no_clients; j++) {
						//pentru fiecare clienti ii parcurg lista de subscriptions si caut topicul
						subscription *client_subscription = clients[j].subscriptions;
						for (int z = 0; z < clients[j].nr_subscriptions; z++) {
							if (strcmp (client_subscription[z].topic, m.message.topic) == 0) {
								if (clients[j].ON == 1) {
									//clientul e activ si ii trimit direct mesajul
									memcpy(buffer, &m, sizeof(msg_server_to_client));
									n = send(clients[j].sockfd, buffer, sizeof(msg_server_to_client), 0);
									DIE(n < 0, "send");

								} else if (client_subscription[z].SF == 1) {
									//pun mesajul in lista de asteptare
									if (client_subscription[z].nr_netrimise == client_subscription[z].cap_netrimise) {
										client_subscription[z].cap_netrimise *= 2;
										client_subscription[z].asteptare = realloc(client_subscription[z].asteptare, 
													client_subscription[z].cap_netrimise * sizeof(msg_server_to_client));
									}
									client_subscription[z].asteptare[client_subscription[z].nr_netrimise++] = m;
								}

								break;
							}
						}
						
					}

				}
				else if (i == 0){
					//citirea de la tastatura in server
					memset(buffer, 0, BUFLEN);
					fgets(buffer, BUFLEN - 1, stdin);
					//singura comanda valida a serverului este exit
					if (strncmp(buffer, "exit", 4) == 0) {
						// trimit comanda catre clienti tcp sa isi inchida conexiunea
						msg_server_to_client tmp;
						strcpy(tmp.message.topic, "exit");
						memcpy(buffer, &tmp, sizeof(msg_server_to_client));
						for(int j=0; j< no_clients; j++){
							if(clients[j].ON == 1){
								n = send(clients[j].sockfd, buffer, sizeof(msg_server_to_client), 0);
								DIE(n < 0, "send");
							}
						}	
						sleep(1);
						close(sockfdtcp);
						close(sockfdudp);
						exit(0);
						
					}
				}
				else {
					// s-au primit date pe unul din socketii de client,
					// asa ca serverul trebuie sa le receptioneze
					memset(buffer, 0, BUFLEN);
					n = recv(i, buffer, sizeof(msg_client_to_server), 0);
					DIE(n < 0, "recv");

					if(n == 0){
						// conexiunea s-a inchis
						close(i);
						// se scoate din multimea de citire socketul inchis 
						FD_CLR(i, &read_fds);

						int j;
						for(j=0; j < no_clients; j++)
							if(clients[j].sockfd == i){
								clients[j].ON = 0;
								break;
							}
						continue;
					}
					
					//am primit mesaj de la clienti tcp, care poate fi
					//o comanda de exit, subscribe sau unsubscribe
					msg_client_to_server msg;
					memcpy(&msg, buffer, sizeof(msg_client_to_server));

					if (strncmp(msg.command, "exit", 4) == 0) {
						// conexiunea s-a inchis, clientul a trimis comanda de exit
						close(i);
						// se scoate din multimea de citire socketul inchis 
						FD_CLR(i, &read_fds);
						for(int j=0; j < no_clients; j++)
							if(strcmp(clients[j].id, msg.id) == 0){
								clients[j].ON = 0;
								// index = j;
								break;
							}

						printf("Client %s disconnected\n", msg.id);	
						continue;
					} 
					else if (strncmp(msg.command, "NEW", 3) == 0) {
						//verific daca este un client nou sau doar s-a deconectat
						int ok_client_nou = 1;
						int idx;
						int STOP = 1;
						for (int j = 0; j < no_clients && ok_client_nou; j++)
							if (strcmp(clients[j].id, msg.id) == 0) {
								if (clients[j].ON == 1) {
									//aici tratez cazul cand se conecteaza 2 clienti cu acelasi nume
									//pe al doilea nu il mai las sa se conecteze
									// asa ca ii trimit mesaj de exit 
									idx = j;
									STOP = 0;
									break;
								}
								clients[j].ON = 1;
								ok_client_nou = 0;
								idx = j;
								break;
							}

						if (STOP == 0) {
							//se incearca conectarea unui client cu acelasi id ca un alt client mai vechi
							msg_server_to_client tmp;
							strcpy(tmp.message.topic, "exit");
							memcpy(buffer, &tmp, sizeof(msg_server_to_client));
							n = send(i, buffer, sizeof(msg_server_to_client), 0);
							DIE(n < 0, "send");						
							continue;	
						}

						if (ok_client_nou == 1) {
							//este un client nou
							//alocare de memorie
							if (no_clients == size_clients) {
								size_clients *= 2;
								clients = realloc(clients, size_clients * sizeof(client));
							}
							client new_client;
							strcpy(new_client.id, msg.id);
							new_client.sockfd = i;
							new_client.ON = 1;
							new_client.cap_subscriptions = 100;
							new_client.subscriptions = malloc(new_client.cap_subscriptions * sizeof(subscription));
							new_client.nr_subscriptions = 0;
							clients[no_clients++] = new_client;
						} 
						else {
						//client mai vechi reconectat
						//ii trimit mesajele din lista de asteptare
							for (int k = 0; k < clients[idx].nr_subscriptions; k++) {
								subscription rezolva = clients[idx].subscriptions[k];
								if (rezolva.SF == 1) {
									for (int t = 0; t < rezolva.nr_netrimise; t++){
										msg_server_to_client trimit = rezolva.asteptare[t];
										memcpy(buffer, &trimit, sizeof(msg_server_to_client));
										n = send(i, buffer, sizeof(msg_server_to_client), 0);
										DIE(n < 0, "send");
									}
									clients[idx].subscriptions[k].nr_netrimise = 0;
								}
							}							
						}
						//afisez mesaj de reconectare
						printf("New client %s connected from %s:%d\n", msg.id,
								inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
					}
					else if (strncmp(msg.command, "subscribe", 9) == 0) {
							for (int j = 0; j< no_clients; j++) {
								//caut clientul ce a trimis comanda
								if (strcmp(clients[j].id, msg.id) == 0) {
									//verific daca este un topic nou 
									int ok = 0; //nu mai are topicul ala
									subscription *client_subscription = clients[j].subscriptions;
									for (int z = 0; z < clients[j].nr_subscriptions; z++) {
										if (strcmp(client_subscription[z].topic, msg.topic) == 0) {
											//nu e un topic nou
											ok = 1;

											client_subscription[z].SF = msg.SF;

											if (msg.SF == 0) {
												//se pied toate mesajele de acum incolo	
												//practic, le suprascriu cu un mesaj "vid"							
												msg_server_to_client vid;
												strcpy(vid.ip, "vid");
												vid.port = -1;			
												for (int t = 0; t < client_subscription[z].nr_netrimise; t++) {
													client_subscription[z].asteptare[t] = vid; // renunt la toate mesajele
												}

												client_subscription[z].nr_netrimise = 0;
											}
											if (msg.SF == 1) {
												//trimit ce mesaje a pierdut
												msg_server_to_client de_trimis;
												for (int t = 0; t < client_subscription[z].nr_netrimise; t++) {

													de_trimis = client_subscription[z].asteptare[t];

													if (strcmp(de_trimis.ip, "vid") != 0) {

														memcpy(buffer, &de_trimis, sizeof(msg_server_to_client));
														n = send(i, buffer, sizeof(msg_server_to_client), 0);
														DIE(n < 0, "send");

													}

												}
												//eliberez mesajele care erau in asteptare
												msg_server_to_client vid;
												strcpy(vid.ip, "vid");
												vid.port = -1;

												for (int t = 0; t < client_subscription[z].nr_netrimise; t++) {
													client_subscription[z].asteptare[t] = vid; // renunt la toate mesajele cumva

												}
												client_subscription[z].nr_netrimise = 0;
											}

										}
									}
									if (ok == 0) {
										//topic nou - trb adaugat
										//alocare mem pt mesaje in asteptare
										//topic nou abonament nou practic
										if (clients[j].nr_subscriptions == clients[j].cap_subscriptions) {
											//realocare mem
											clients[j].cap_subscriptions *= 2;
											clients[j].subscriptions =  realloc(clients[j].subscriptions, clients[j].cap_subscriptions * sizeof(subscription));
										}
										//fac un abonament nou
										subscription new;
										strcpy(new.topic, msg.topic); //&M.topic?
										new.SF = msg.SF;
										new.asteptare = malloc (100 * sizeof(msg_server_to_client));
										new.nr_netrimise = 0;
										new.cap_netrimise = 100;
										//il adaug
										clients[j].subscriptions[clients[j].nr_subscriptions++] = new;
									}
								}
							}						
					}
					else if (strncmp (msg.command, "unsubscribe", 11) == 0) {
							//trebuie sa sterg abonarea
							//caut clientul care a trimis comanda
							int ok_find_topic = 1;

							for (int j = 0; j < no_clients && ok_find_topic; j++)
								if (strcmp(clients[j].id, msg.id) == 0) {
									//am gasit clientul
									//ii caut prin subscriptions topicul
									for (int z = 0; z < clients[j].nr_subscriptions && ok_find_topic; z++)
										if(strcmp(clients[j].subscriptions[z].topic, msg.topic) == 0) {
											//l-am gasit si trebuie sa-l sterg
											ok_find_topic = 0;
											for (int k = z ; k < clients[j].nr_subscriptions - 1; k++)
												memcpy(&(clients[j].subscriptions[k]), &(clients[j].subscriptions[z+1]), sizeof(subscription));
											clients[j].nr_subscriptions--;
										}
								}

							if (ok_find_topic == 1) {
								//Nu era abonat pe acel topic si trec mai departe
								continue;
							}
						}
				}
			}
		}
	}

	close(sockfdtcp);
	close(sockfdudp);
	return 0;
}

