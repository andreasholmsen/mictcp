#include <mictcp.h>
#include <api/mictcp_core.h>
#include <stdio.h>
#include <string.h>

#define NB_SOCKETS 10
#define MAX_PAYLOAD_SIZE 1000000
#define ADDRESS_SIZE 16
#define PDU_ENVOYE 10

/*===============================Début variables globales===============================*/
int next_seq_num = 0;
int seq_attendu = 0;
unsigned long MAX_TIME = 1;//ms

int packets_sent = 0;
int fg[PDU_ENVOYE];
/*===============================Fin variables globales=================================*/


//tableau des sockets pas encore utilisés
mic_tcp_sock sockets[NB_SOCKETS];

int need_to_resend() {
    return (int) rand();
}


/*
 * Permet de créer un socket entre l’application et MIC-TCP
 * Retourne le descripteur du socket ou bien -1 en cas d'erreur
 */

int mic_tcp_socket(start_mode sm) {
    printf("[MIC-TCP] Appel de la fonction: %s\n", __FUNCTION__);
    
    if (initialize_components(sm) == -1) return -1;
    
    // Find first available socket
    int fd;
    for (fd = 0; fd < NB_SOCKETS; fd++) if (sockets[fd].state == CLOSED) break;
    
    // Check if we found an available socket
    if (fd >= NB_SOCKETS) return -1;
    
    // Initialize the socket
    memset(&sockets[fd], 0, sizeof(sockets[fd]));
    sockets[fd].fd = fd;
    sockets[fd].state = IDLE;

    sockets[fd].local_addr.ip_addr.addr = "localhost";
    sockets[fd].local_addr.ip_addr.addr_size = sizeof("localhost");

    return fd;
}

/*
 * Permet d'attribuer une adresse à un socket.
 * Retourne 0 si succès, et -1 en cas d'échec
 */
int mic_tcp_bind(int socket, mic_tcp_sock_addr addr){
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");

    // Check if socket index is valid
    if (socket < 0 || socket >= NB_SOCKETS) return -1;
    if (sockets[socket].fd != socket) return -1;

    sockets[socket].local_addr = addr;

    return 0;
}
/*
 * Met le socket en état d'acceptation de connexions 
 * On peut ajouter au state un état d'acceptation
 * Retourne 0 si succès, -1 si erreur
 */
int mic_tcp_accept(int socket, mic_tcp_sock_addr* addr)
{
    if (socket < 0 || socket >= NB_SOCKETS) return -1;


    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    *addr = sockets[socket].remote_addr;

    return 0; //pas de phase d'etablissement de connexion à ce stade
}

/*
 * Permet de réclamer l’établissement d’une connexion
 * Retourne 0 si la connexion est établie, et -1 en cas d’échec
 */

int mic_tcp_connect(int socket, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    if (sockets[socket].fd != socket) return -1;

    sockets[socket].remote_addr = addr;
    sockets[socket].state = ESTABLISHED;


    // Init de fenetre glissant:
    for (int i = 0; i < sizeof(fg)/sizeof(fg[0]); i++) fg[i] = 0;



    return 0;
}

/*
 * Permet de réclamer l’envoi d’une donnée applicative
 * Retourne la taille des données envoyées, et -1 en cas d'erreur
 */
int mic_tcp_send(int mic_sock, char* mesg, int mesg_size)
{
    printf("[MIC-TCP] Appel de la fonction: %s\n", __FUNCTION__);
    
    if (mic_sock < 0 || mic_sock >= NB_SOCKETS || sockets[mic_sock].state != ESTABLISHED) return -1;

    mic_tcp_header pdu_header = {sockets[mic_sock].local_addr.port, sockets[mic_sock].remote_addr.port, next_seq_num, 0, 0, 0, 0,};
    mic_tcp_payload pdu_payload = {mesg, mesg_size};
    mic_tcp_pdu pdu = {pdu_header, pdu_payload};

    int sent_size = IP_send(pdu, sockets[mic_sock].remote_addr.ip_addr);
    if (sent_size < 0) return -1;

    mic_tcp_pdu *ack_pdu = malloc(sizeof(mic_tcp_pdu));
    ack_pdu->payload.size = 0;

    /*
    mic_tcp_ip_addr *loc_addr = malloc(sizeof(mic_tcp_ip_addr));
    loc_addr->addr = malloc(ADDRESS_SIZE);
    loc_addr->addr_size = ADDRESS_SIZE;
    */  mic_tcp_ip_addr * loc_addr = NULL;

    mic_tcp_ip_addr *rmt_addr = malloc(sizeof(mic_tcp_ip_addr));
    rmt_addr->addr = malloc(ADDRESS_SIZE);
    rmt_addr->addr_size = ADDRESS_SIZE;

    while (1) {
        int recv = IP_recv(ack_pdu, loc_addr, rmt_addr, MAX_TIME); // Recoit le ACK
        if (recv == -1) { // Si timeout

            if(need_to_resend()) { // Si besoin de retransmettre
                 IP_send(pdu, sockets[mic_sock].remote_addr.ip_addr);
                 printf("RETRANSMISSION\n");
                 packets_sent++;
                 continue; //Recommence l'attente de ACK
            }

            printf("DROPPING PACKET\n");
            //Si pas besoin de retransmettre, on met à jour le tableau, et on fini le boucle
            fg[packets_sent%PDU_ENVOYE] = 0;
           break;
        } else if (ack_pdu->header.ack && ack_pdu->header.ack_num == next_seq_num) {
            printf("ACK RECIEVED, packets sent: %d\n", packets_sent);
            next_seq_num = (next_seq_num + 1) % 2;
            fg[packets_sent%PDU_ENVOYE] = 1;
            break;
        }
    }
    packets_sent++;
    return sent_size;
}

/*
 * Permet à l’application réceptrice de réclamer la récupération d’une donnée
 * stockée dans les buffers de réception du socket
 * Retourne le nombre d’octets lu ou bien -1 en cas d’erreur
 * NB : cette fonction fait appel à la fonction app_buffer_get()
 */
int mic_tcp_recv (int socket, char* mesg, int max_mesg_size)
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
    
    // Check if socket is valid
    if (socket < 0 || socket >= NB_SOCKETS) return -1;
    
    //déclaration struct pour stocker le message
    mic_tcp_payload payload;
    payload.data = mesg;
    payload.size = max_mesg_size;

    int effective_data_size = app_buffer_get(payload);

    if (effective_data_size >= 0) return effective_data_size;
    return -1;
}

/*
 * Permet de réclamer la destruction d’un socket.
 * Engendre la fermeture de la connexion suivant le modèle de TCP.
 * Retourne 0 si tout se passe bien et -1 en cas d'erreur
 */
int mic_tcp_close (int socket)
{
    printf("[MIC-TCP] Appel de la fonction :  "); printf(__FUNCTION__); printf("\n");
    sockets[socket].state = CLOSED;
    return 0;
}

/*
 * Traitement d’un PDU MIC-TCP reçu (mise à jour des numéros de séquence
 * et d'acquittement, etc.) puis insère les données utiles du PDU dans
 * le buffer de réception du socket. Cette fonction utilise la fonction
 * app_buffer_put().
 */
void process_received_PDU(mic_tcp_pdu pdu, mic_tcp_ip_addr local_addr, mic_tcp_ip_addr remote_addr)
{
    printf("[MIC-TCP] Appel de la fonction: %s\n", __FUNCTION__);
    //printf("seq: %d, attendu: %d \n", pdu.header.seq_num, seq_attendu);

    int ack_num = seq_attendu;

    if (pdu.header.seq_num == seq_attendu) {
        //printf("PACKET RECIEVED\n");
        seq_attendu = (seq_attendu+1) % 2;
        app_buffer_put(pdu.payload); 
    }

    //Creation de ACK
    mic_tcp_header ack_header = {pdu.header.dest_port, pdu.header.source_port, 1, ack_num, 0,1,0};
    mic_tcp_payload ack_payload = {NULL, 0};
    mic_tcp_pdu ack = {ack_header, ack_payload};

    int result = IP_send(ack, remote_addr);
    if (result < 0) printf("ACK send failed\n");
    
}






