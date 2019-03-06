#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <unistd.h>
#include <arpa/inet.h>

#define BUFLEN 256

/* Functie ce faciliteaza scrierea in fisier si afisarea
comenzilor primite de la stdin si a raspunsurilor serverului */
void log_writer(char msg[], FILE *log, int fatal_error) {
    printf("%s\n\n", msg);
    fprintf(log, "%s\n\n", msg);
    fflush(log);

    if(fatal_error){
        fclose(log);
        exit(0);
    }
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

/* Functia intoarce prin efect lateral numarul de card (card_no)
ce se afla in buffer incepand de la pozitia 6 */
void extract_card (char buffer[BUFLEN], char card_no[6]) {
    int i;
    for (i = 0; i < 6; i++)
        card_no[i] = buffer[i + 6];
    card_no[i] = '\0';
}

int main(int argc, char *argv[]) {
    int sockfd, resp;
    char buffer[BUFLEN];
    struct sockaddr_in serv_addr;
    struct hostent *server;

    fd_set read_fds;   //multimea de citire folosita in select()
    fd_set tmp_fds;    //multime folosita temporar 

    if (argc < 3) {
       fprintf(stderr,"Usage %s server_address server_port\n", argv[0]);
       exit(0);
    }

    char ID[16];
    sprintf(ID, "%d", getpid());
    strcat(ID, ".log");

    /* Deschid fisierul de log */
    FILE *log = fopen(ID, "wt");
    if(log == NULL) {
        printf("Error when trying to open log file\n");
        exit(0);
    }

    /* Golim multimea de descriptori de citire (read_fds) 
    si multimea tmp_fds */ 
    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);
    
    /* Initializare socket TCP */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        log_writer("-10: eroare la apel socket()", log, 1);
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    inet_aton(argv[1], &serv_addr.sin_addr);
    
    if (connect(sockfd,(struct sockaddr*) &serv_addr,sizeof(serv_addr)) < 0) 
        log_writer("-10: eroare la apel connect()", log, 1);

    FD_SET(sockfd, &read_fds);
    FD_SET(0, &read_fds);
    /* Sfrasitul initializarii TCP */

    /* Initializare socket UDP */
    int sock_udp, udp_len = sizeof(struct sockaddr);
    struct sockaddr_in udp_server_addr;

    if ((sock_udp = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
        log_writer("-10: eroare la apel socket()", log, 1);

    udp_server_addr.sin_family = AF_INET;
    udp_server_addr.sin_port = htons(5000);
    inet_aton(argv[1], &udp_server_addr.sin_addr);
    bzero(&(udp_server_addr.sin_zero),8);
    /* Sfarsitul initializarii UDP */

    int logged = 0; //clientul este/nu este logat intr-un cont
    int login_attempt = 0; //clientul intentioneaza/nu intentioneaza sa se logheze
    int unlock_attempt = 0; //clientul intentioneaza/nu intentioneaza sa deblocheze ultimul cont
    char last_card[6]; //numarul de card corespunzator ulimului cont in care a incercat clientul
                        //sa se logheze
    
    while(1) {	
        tmp_fds = read_fds;
        if (select(sockfd + 1, &tmp_fds, NULL, NULL, NULL) == -1) 
            log_writer("-10: eroare la apel select()", log, 1);

         /* Am primit date de la stdin */
        if(FD_ISSET(0, &tmp_fds)) {

            memset(buffer, 0 , BUFLEN);
            fgets(buffer, BUFLEN-1, stdin);

            int len = strlen(buffer);
            if(buffer[len - 1] == '\n')
                buffer[len - 1] = '\0';

            fprintf(log, "%s\n", buffer);

            /* Cazul in care clientul trebuie sa trimita din nou date
            pe socketul UDP, mai exact parola */
            if (unlock_attempt == 1) {
                char aux_buff[BUFLEN];
                memset(aux_buff, 0, BUFLEN);
                strcat(aux_buff, last_card);
                strcat(aux_buff, " ");
                strcat(aux_buff, buffer);

                sendto(sock_udp, aux_buff, BUFLEN, 0, (struct sockaddr *)&udp_server_addr, udp_len);

                memset(buffer, 0 ,BUFLEN);

                recvfrom(sock_udp, buffer, BUFLEN, 0, (struct sockaddr*)&udp_server_addr, &udp_len);

                log_writer(buffer, log, 0);
                unlock_attempt = 0;

                continue;
            }

            /* In general, la procesarea unei comenzi se verifica daca
            clientul este sau nu logat pentru ca programul sa actioneze corespunzator */

            else if(strncmp(buffer, "login", 5) == 0) {

                memset(last_card, 0, 6);
                extract_card(buffer, last_card);

                if(logged) {
                    log_writer("IBANK> -2 : Sesiune deja deschisa", log, 0);
                    continue;
                }
                login_attempt = 1;

            } else if (strncmp(buffer, "logout", 6) == 0) {
                if(!logged) {
                    log_writer("IBANK> -1 : Clientul nu este autentificat", log, 0);
                    continue;
                }
                logged = 0;
            
            } else if (strncmp(buffer, "listsold", 8) == 0) {
                if(!logged) {
                    log_writer("IBANK> -1 : Clientul nu este autentificat", log, 0);
                    continue;
                }

            } else if (strncmp(buffer, "transfer", 8) == 0) {
                if(!logged) {
                    log_writer("IBANK> -1 : Clientul nu este autentificat", log, 0);
                    continue;
                }
            
            } else if (strncmp(buffer, "quit", 4) == 0) {
                resp = send(sockfd, buffer, BUFLEN, 0);
                if (resp < 0) 
                    log_writer("-10: eroare la apel send()", log, 1);

                close(sockfd);
                close(sock_udp);
                break;
            
            /* Daca s-a dat comanda unlock, clientul va trimite serverului
            cererea de deblocare pe socketul UDP */
            } else if (strncmp(buffer, "unlock", 6) == 0) {
                memset(buffer, 0 ,BUFLEN);
                strcat(buffer, "unlock ");
                strcat(buffer, last_card);
                sendto(sock_udp, buffer, BUFLEN, 0, (struct sockaddr *)&udp_server_addr, udp_len);

                memset(buffer, 0, BUFLEN);
                recvfrom(sock_udp, buffer, BUFLEN, 0, (struct sockaddr*)&udp_server_addr, &udp_len);

                log_writer(buffer, log, 0);

                if (strncmp(buffer, "UNLOCK> Trimite parola secreta", 30) == 0)
                    unlock_attempt = 1;

                continue;   
            }

            /* Trimit mesaj la server */
            resp = send(sockfd, buffer, BUFLEN, 0);
            if (resp < 0) 
                log_writer("-10: eroare la apel send()", log, 1);

        } else {
            memset(buffer, 0 , BUFLEN);

            if((resp = recv(sockfd, buffer, BUFLEN, 0)) <= 0) {
                log_writer("-10: eroare la apel recv()", log, 1);
            }

            /* Serverul inchide conexiunea */
            if(strncmp(buffer, "SERVER>", 6) == 0) {
                close(sockfd);
                close(sock_udp);
                log_writer(buffer, log, 1);
            }

            /* Cazul in care clientul a inceput procesul de logare
            intr-un cont, acesta asteptand mesajul de confirmare
            de la server */
            if(login_attempt == 1) {

                login_attempt = 0;
                if(custom_cmp("Welcome", buffer, 7, 7) == 0) {
                    logged = 1;
                }
            }
            log_writer(buffer, log, 0);
        }
    }
    fclose(log);
    return 0;
}


