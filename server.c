#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_CLIENTS	50
#define BUFLEN 256

typedef struct {
	char nume[13];
	char prenume[13];
	char numar_card[7];
	char pin[5];
	char parola_secreta[9];
	double sold;
	int clifd; //descriptorul socketului unui client
	int failed_logs; //numarul de logari consecutive nereusite
					//efectuate de acelasi client
	int unlock_attempt; //0 - contul nu e in curs de deblocare
						//1 - contul e in curs de deblocare	
}user_account;

/* Structura este folostia pentru a retine conturile 
din care sunt transferati bani.*/
typedef struct {
	int clifd; //descriptorul clientului ce a demarat transferul
	int from_user; //indexul contului de unde se iau banii
	int to_user; //indexul contului unde sunt depusi banii
	double money; 
}STransfer;

void error(char *msg)
{
    perror(msg);
    exit(1);
}

/* Compara 2 stringuri, compararea celui de al doilea incepand
de la pozitia "index" */
int custom_cmp(char str1[], char str2[], int index, int size) {
	for(int i = 0; i < size; i++) {
		if(str1[i] != str2[index + i])
			return 1;
	}

	return 0;
}

/* Verifica existenta contului corespunzator numarului de card
ce se afla in buffer. Intoarce pozitia din vectorul unde sunt stocate
toate conturile bancare */
int check_card_no (user_account users_list[], int users_no, char buffer[BUFLEN], int start) {
	for(int i = 0 ; i < users_no; i++)
		if(custom_cmp(users_list[i].numar_card, buffer, start, 6) == 0)
			return i;

	return -1;
}

/* Permite (sau nu) login-ul unui client in contul corespunzator credidentialelor
oferite de acesta */
void log_in(user_account users_list[], int users_no, char buffer[BUFLEN], int clifd) {

	char aux_buff[BUFLEN];
	memcpy(aux_buff, buffer, BUFLEN);
	memset(buffer, 0, BUFLEN);

	/* Se verifica existenta contului corespunzator numarului de card oferit */
	int i = check_card_no(users_list, users_no, aux_buff, 6);
	if(i < 0) {
		strcat(buffer, "IBANK> -4 : Numar card inexistent");
		return;
	}

	/* Se verifica daca pin-ul oferit corespunde cu cel al contului */
	if(custom_cmp(users_list[i].pin, aux_buff, 13, 4) == 0) {

		if(users_list[i].failed_logs >= 3) {
			strcat(buffer, "IBANK> -5 : Card blocat");
			return;

		} else if (users_list[i].clifd != 0 && users_list[i].failed_logs == 0) {
			strcat(buffer, "IBANK> -2 : Sesiune deja deschisa");
			return;

		/* Clientul se poate autentifica */
		} else {
			users_list[i].clifd = clifd;
			users_list[i].failed_logs = 0;
			strcat(buffer, "IBANK> Welcome ");
			strcat(buffer, users_list[i].nume);
			strcat(buffer, " ");
			strcat(buffer, users_list[i].prenume);
			return;
		}

	/* Mai jos sunt tratate cazurile in care clientul
	a introdus pin-ul gresit */
	} else {

		/* Daca acelasi client a introdus un pin gresit,
		se va actualiza contorul ce tine evidenta incercarilor
		esuate de logare si, in cazul in care acesta a atins valoarea 3,
		contul se va bloca */
		if(users_list[i].clifd == clifd) {	
			if(++users_list[i].failed_logs >= 3) {
				strcat(buffer, "IBANK> -5 : Card blocat");
				return;
			}
			strcat(buffer, "IBANK> -3 : Pin gresit");
		
		/* Daca alt client incearca se se autentifice cu acest cont,
		contorul incercarilor esuate va fi resetat. In cazul in care
		contul este deja blocat, nu se ia in considerare aceasta
		incercare de autentificare */	
		} else {
			if(users_list[i].failed_logs >= 3) {
				strcat(buffer, "IBANK> -5 : Card blocat");
				return;
			}

			users_list[i].clifd = clifd;
			users_list[i].failed_logs = 1;
			strcat(buffer, "IBANK> -3 : Pin gresit");
		}
	}
		return;
}

/* Functia imi intoarce pozitia la care se afla contul cu care este logat
clientul "clifd" in vectorul unde sunt stocate toate conturile bancare */
int get_logged_user(user_account users_list[], int users_no, int clifd) {
	for(int i = 0; i < users_no; i++)
		if(users_list[i].clifd == clifd) 
			return i;

	return -1;
}

/* Functie auxiliara ce imi asigura conversia din formatul char[] al soldului
,ce se afla in buffer, in formatul double. Ca efect lateral functia va oferi
un char array unde se va afla doar soldul */
double double_convertor(char buffer[BUFLEN], char conv[BUFLEN - 16]) {
	int i, len = strlen(buffer);
	double sum;

	for(i = 0; i < len; i++)
		conv[i] = buffer[16 + i];
	conv[i] = '\0';

	sscanf(conv, "%lf", &sum);
	return sum;
}

/* Functia ne spune daca se poate efectua un transfer din contul corespunzator
credidentialelor oferite de client */ 
double money_transfer(user_account users_list[], int users_no, char buffer[BUFLEN],
	                 int from_user, int *to_user) {

	char aux_buff[BUFLEN]; char money[BUFLEN - 16];
	memcpy(aux_buff, buffer, BUFLEN);
	memset(buffer, 0, BUFLEN);
	
	/* Se verifica existenta contului in functie de numarul de card */
	(*to_user) = check_card_no(users_list, users_no, aux_buff, 9);
	if((*to_user) < 0) {
		strcat(buffer, "IBANK> -4 : Numar card inexistent");
		return -1;
	}

	/* Se verifica soldul curent al contului */
	double sum = double_convertor(aux_buff, money);
	if(users_list[from_user].sold < sum) {
		strcat(buffer, "IBANK> -8 : Fonduri insuficiente");
		return -1;
	}

	strcat(buffer, "IBANK> Transfer ");
	strcat(buffer, money);
	strcat(buffer, " ");
	strcat(buffer, "catre ");
	strcat(buffer, users_list[*to_user].nume);
	strcat(buffer, " ");
	strcat(buffer, users_list[*to_user].prenume);
	strcat(buffer, "? [y/n]");

	return sum;
}

/* In functie de raspunsul unui client, functia efectueaza (sau nu)
transferul bancar */
void resolve_transfers(user_account users_list[], STransfer transfer_queue[],
					int *transfer_len, char buffer[BUFLEN], int client) {

	/* Se parcurge vectorul de transferuri bancare */
	for(int j = 0; j < (*transfer_len); j++) {
		/* Daca s-a gasit clientul curent ce comunica cu serverul, 
		raspunsul acestuia va fi evaluat */
		if(transfer_queue[j].clifd == client) {

			if(strncmp(buffer, "y", 1) == 0) {
				int from_user =  transfer_queue[j].from_user;
				int to_user = transfer_queue[j].to_user;
				double money = transfer_queue[j].money;

				users_list[from_user].sold -= money;
				users_list[to_user].sold += money;

				memset(buffer, 0, BUFLEN);
				strcat(buffer, "IBANK> Transfer realizat cu succes");

			} else {
				memset(buffer, 0, BUFLEN);
				strcat(buffer, "IBANK> -9 : Operatie anulata");
			}

			/* Se sterge clientul curent din vectorul deoarece
			transferul s-a incheiat */
			for(int k = j; k < (*transfer_len) - 1; k++)
				transfer_queue[k] = transfer_queue[k + 1];
			(*transfer_len)--;

			send(client, buffer, BUFLEN, 0);
		}
	}
}

int main(int argc, char *argv[])
{
	int sockfd, newsockfd, portno, clilen;
	char buffer[BUFLEN], aux_buff[BUFLEN];
	struct sockaddr_in serv_addr, cli_addr;
	int n, i, j, card_check, users_no, transfer_len = 0;

	fd_set read_fds;	//multimea de citire folosita in select()
	fd_set tmp_fds;	//multime folosita temporar 
	int fdmax;		//valoare maxima file descriptor din multimea read_fds


	if (argc < 3) {
	 fprintf(stderr,"Usage : %s <port> <users_data_file\n", argv[0]);
	 exit(1);
	}

	FILE *users_file;
	if((users_file = fopen(argv[2], "rt")) == NULL)
		perror("-10: eroare la apel fopen()");

	/* Intorduc datele din fisier in structura de date a serverului */ 
	fscanf(users_file, "%d", &users_no);
	user_account users_list[users_no];

	/* Am creat un vector de trasnferuri bancare unde vor fi introdusi
	toti clientii de la care serverul asteapta raspunsul de confirmare */
	STransfer transfer_queue[users_no];

	for(i = 0; i < users_no; i++) {
		fscanf(users_file, "%s", users_list[i].nume);
		fscanf(users_file, "%s", users_list[i].prenume);
		fscanf(users_file, "%s", users_list[i].numar_card);
		fscanf(users_file, "%s", users_list[i].pin);
		fscanf(users_file, "%s", users_list[i].parola_secreta);
		fscanf(users_file, "%lf", &users_list[i].sold);
		
		users_list[i].clifd = 0;
		users_list[i].failed_logs = 0;
		users_list[i].unlock_attempt = 0;
	}

	fclose(users_file);

	/* Golim multimea de descriptori de citire (read_fds) 
	si multimea tmp_fds */ 
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	/* Initializare socket TCP */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) 
		error("-10: eroare la apel socket()");

	portno = atoi(argv[1]);

	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;	// foloseste adresa IP a masinii
	serv_addr.sin_port = htons(portno);

	if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0) 
		error("-10: eroare la apel bind()");

	listen(sockfd, MAX_CLIENTS);

	/* Adaugam noul file descriptor (socketul pe care se asculta conexiuni) 
	si file descriptor pt stdin in multimea read_fds */
	FD_SET(sockfd, &read_fds);
	FD_SET(0, &read_fds);
	fdmax = sockfd;
	/* Sfarsitul initializarii TCP */

	/* Initializare socket UDP */
	int sock_udp, udp_len = sizeof(struct sockaddr);
	struct sockaddr_in udp_server_addr, udp_client_addr;

	if ((sock_udp = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		error("-10 : eroare la apel socket()");

	udp_server_addr.sin_family = AF_INET;
	udp_server_addr.sin_port = htons(5000);
	udp_server_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(udp_server_addr.sin_zero),8);

	if (bind(sock_udp,(struct sockaddr *)&udp_server_addr, sizeof(struct sockaddr)) == -1)
		error("-10 : eroare la apel bind()");

	if(fdmax < sock_udp)
		fdmax = sock_udp;

	FD_SET(sock_udp, &read_fds);
	/* Sfarsitul initializarii UDP */

	while (1) {
	tmp_fds = read_fds; 
	if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1) 
		error("-10: eroare la apel select()");

		for(i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {

				/* Am primit un mesaj de la stdin */
				if (i == 0) {
					memset(buffer, 0 , BUFLEN);
		    		fgets(buffer, BUFLEN-1, stdin);

		    		int len = strlen(buffer);
		    		if(buffer[len - 1] == '\n')
		        		buffer[len - 1] = '\0';

		        	/* Serverul efectueaaza o singura comanda obtinuta de la stdin: quit */
		        	if(strncmp(buffer, "quit", 4) == 0) {
		        		memset(buffer, 0, BUFLEN);
		        		strcat(buffer, "SERVER> Server shuts down...");

		        		/* Instiintez toti utilizatorii despre inchiderea serverului */
		        		for(int j = 4; j <= fdmax; j++) {
		        			send(j, buffer, BUFLEN, 0);
		        			FD_CLR(j, &read_fds);
		        		}

		        		close(sockfd);
		        		close(sock_udp);
		        		return(0);
		        	}

				} else if (i == sockfd) {
					/* A venit ceva pe socketul inactiv(cel cu listen) = o noua conexiune
					actiunea serverului: accept() */
					clilen = sizeof(cli_addr);
					if ((newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen)) == -1) {
						error("-10: eraore la apel accept()");
					} 
					else {
						/* Adaug noul socket intors de accept() 
						la multimea descriptorilor de citire */
						FD_SET(newsockfd, &read_fds);
						if (newsockfd > fdmax) { 
							fdmax = newsockfd;
						}
					}
					printf("Noua conexiune de la %s, port %d, socket_client %d\n ", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port), newsockfd);
				
				} else if (i == sock_udp) {
					/* A venit un mesaj pe socketul UDP */
					memset(buffer, 0, BUFLEN);
					recvfrom(sock_udp, buffer, BUFLEN, 0 ,(struct sockaddr*)&udp_client_addr, &udp_len);
					printf ("Am primit de la clientul de pe socketul UDP, mesajul: %s\n", buffer);

					/* Mesajul ce a venit pe socket-ul UDP poate contine: 
						-> comanda unlock, urmata de numar_card
						-> numar_card, urmat de parola secreta */

					/* In cazul in care am primit comanda unlock, efecutez verificarile
					necesare deblocarii cardului */
					if(strncmp(buffer, "unlock", 6) == 0){ 
						card_check = check_card_no(users_list, users_no, buffer, 7);

						memset(buffer, 0, BUFLEN);
						
						/* Verific existena contului */
						if(card_check < 0) {
							strcat(buffer, "UNLOCK> -4 : Numar card inexistent");
						
						/* Verific daca contul este deja in curs de deblocare */
						} else if(users_list[card_check].unlock_attempt) {
							strcat(buffer, "UNLOCK> -7 : Deblocare esuata");
						
						/* Verificarea a avut loc cu succes.
						Ii cer utilizatorului parola secreta */
						} else {
							strcat(buffer, "UNLOCK> Trimite parola secreta");
							users_list[card_check].unlock_attempt = 1;
						}

						sendto(sock_udp, buffer, BUFLEN, 0, (struct sockaddr *)&udp_client_addr, sizeof(struct sockaddr));
					
					/* Am primit parola secreta */
					} else {
						
						memset(aux_buff, 0, BUFLEN);
						int pswd_len = strlen(users_list[card_check].parola_secreta);

						/* Verific daca contul este blocat */
						if(users_list[card_check].failed_logs < 3) {
							strcat(aux_buff, "UNLOCK> -6 : Operatie esuata");
						
						/* Verific parola secreta */
						} else if (custom_cmp(users_list[card_check].parola_secreta, buffer, 7, pswd_len) != 0) {
							strcat(aux_buff, "UNLOCK> Deblocare esuata");
						
						/* In urma verificarilor efectuate cu succes deblochez contul */
						} else {
							strcat(aux_buff, "UNLOCK> Card deblocat");
							users_list[card_check].failed_logs = 0;
							users_list[card_check].clifd = 0;
							users_list[card_check].unlock_attempt = 0;
						}

						memset(buffer, 0, BUFLEN);
						sendto(sock_udp, aux_buff, BUFLEN, 0, (struct sockaddr *)&udp_client_addr, sizeof(struct sockaddr));
					}

				} else {
					/* Am primit date pe unul din socketii cu care vorbesc cu clientii
					actiunea serverului: recv() */

					memset(buffer, 0, BUFLEN);
					if ((n = recv(i, buffer, sizeof(buffer), 0)) <= 0) {
						if (n == 0) {
							/* Conexiunea s-a inchis */
							printf("SERVER> : Socket %d hung up\n", i);
						} else {
							error("-10: eroare la apel recv()");
						}
						/* Scoatem din multimea de citire socketul respectiv */
						close(i); 
						FD_CLR(i, &read_fds); 
					
					} else {
						printf ("Am primit de la clientul de pe socketul %d, mesajul: %s\n", i, buffer);

						/* Inainte de evaluarea oricarei comenzi, serverul o sa verifice daca 
						clientul actual este implicat in vreo tranzactie, pentru a putea procesa
						raspunsul de confirmare al acestuia. */
						resolve_transfers(users_list, transfer_queue, &transfer_len, buffer, i);
						int index;

						/* In functie de comanda primita efectuez actiunile
						corespunzatoare acesteia */
						
						if (strncmp(buffer, "login", 5) == 0) {
							log_in(users_list, users_no, buffer, i);
							printf("%s\n", buffer);
							send(i, buffer, BUFLEN, 0);

						} else if (strncmp(buffer, "logout", 6) == 0) {
							index = get_logged_user(users_list, users_no, i);
							users_list[index].clifd = 0;

							memset(buffer, 0, BUFLEN);
							strcat(buffer, "IBANK> Clientul a fost deconectat");
							send(i, buffer, BUFLEN, 0);

						} else if (strncmp(buffer, "listsold", 8) == 0) {
							index = get_logged_user(users_list, users_no, i);
							char sold[16];
							/* Fac conversia din double in char[] pentru a putea introduce
							soldul in buffer */
							sprintf(sold, "%.2f", users_list[index].sold);

							memset(buffer, 0, BUFLEN);
							strcat(buffer, "IBANK> ");
							strcat(buffer, sold);
							send(i, buffer, BUFLEN, 0);

						} else if (strncmp(buffer, "transfer", 8) == 0) {
							index = get_logged_user(users_list, users_no, i);
							int to_user;
							
							/* Obtin suma de bani ce trebuie transferata */
							double money = money_transfer(users_list, users_no, buffer, index, &to_user);

							/* Daca fondurile sunt suficiente,
							introduc in vectorul de tranzactii tranzactia curenta */
							if(money > 0) {
								STransfer trans;
								trans.clifd = i;
								trans.from_user = index;
								trans.to_user = to_user;
								trans.money = money;
								transfer_queue[transfer_len++] = trans;
							}
							send(i, buffer, BUFLEN, 0);
						}
					}
				} 
			}
		}
     }
    close(sockfd);   
    return 0; 
}
