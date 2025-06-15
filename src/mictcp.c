#include <mictcp.h>
#include <api/mictcp_core.h>
#include <stdio.h>
#include <string.h>


#define NB_SOCKETS 10
#define PDU_ENVOYE 10 // taille de tableau fg (fenetre glissante)
#define CHECK_VALID_SOCKET(fg)  if (fg < 0 || fg >= NB_SOCKETS || sockets[fg].fd != fg) return -1
/*===============================Début variables globales===============================*/
int seq_num = 0;
int seq_attendu = 0;
unsigned long MAX_TIME = 100;//ms
int packets_sent = 0;
int max_pertes = 20; //pourcentage de perte acceptable
int fg[PDU_ENVOYE]; // fenetre glissante
mic_tcp_sock sockets[NB_SOCKETS]; //tableau des sockets
/*===============================Fin variables globales=================================*/


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

    // Init de fenetre glissant:
    for (int i = 0; i < PDU_ENVOYE; i++) fg[i] = 0;

    return fd;
}

/*
 * Permet d'attribuer une adresse à un socket.
 * Retourne 0 si succès, et -1 en cas d'échec
 */
int mic_tcp_bind(int socket, mic_tcp_sock_addr addr){
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    CHECK_VALID_SOCKET(socket);

    //Associate local addr
    sockets[socket].local_addr = addr;
    sockets[socket].state = IDLE;

    return 0;
}
/*
 * Met le socket en état d'acceptation de connexions 
 * On peut ajouter au state un état d'acceptation
 * Retourne 0 si succès, -1 si erreur
 */
int mic_tcp_accept(int socket, mic_tcp_sock_addr* addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    CHECK_VALID_SOCKET(socket);

    *addr = sockets[socket].remote_addr;
    sockets[socket].state = ESTABLISHED;
    return 0; //pas de phase d'etablissement de connexion à ce stade
}

/*
 * Permet de réclamer l’établissement d’une connexion
 * Retourne 0 si la connexion est établie, et -1 en cas d’échec
 */

int mic_tcp_connect(int socket, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    CHECK_VALID_SOCKET(socket);

    sockets[socket].remote_addr = addr;
    sockets[socket].state = ESTABLISHED;

    return 0;
}

/*
 * Calcul de condition de resend. 1 si oui, 0 sinon
 * A être utilisée dans mic_tcp_send()
 */
  int need_to_resend(){
    int nbReussi = 0;
    for (int i=0;i<PDU_ENVOYE;i++) nbReussi += fg[i];
    int pourcentagePerte = 100*(PDU_ENVOYE-nbReussi)/PDU_ENVOYE;

    printf("Packet perdu. Besoin de retransmettre? %d\n", pourcentagePerte >max_pertes);
    return pourcentagePerte >max_pertes;
 }

/*
 * Permet de réclamer l’envoi d’une donnée applicative
 * Retourne la taille des données envoyées, et -1 en cas d'erreur
 */
int mic_tcp_send(int mic_sock, char* mesg, int mesg_size)
{
    printf("[MIC-TCP] Appel de la fonction: %s\n", __FUNCTION__);
    CHECK_VALID_SOCKET(mic_sock);

    sockets[mic_sock].remote_addr.ip_addr.addr_size = sizeof(sockets[mic_sock].remote_addr.ip_addr.addr); // Update de taille d'addr. Seg fault sinon.

    //creation du pdu
    mic_tcp_pdu pdu = {.header = {sockets[mic_sock].local_addr.port,sockets[mic_sock].remote_addr.port, seq_num,0,0,0,0}, .payload = {mesg, mesg_size}};

    int sentSize = -1;
    do {
        sentSize = IP_send(pdu, sockets[mic_sock].remote_addr.ip_addr);
        if (sentSize < 0)  return -1;

        //prep rec ACK
        mic_tcp_pdu ack = {.header = {0,0,0,0,0,0,0}, .payload = {NULL, 0}};
        int recvSize = IP_recv(&ack, NULL, &sockets[mic_sock].remote_addr.ip_addr, MAX_TIME);

        //Si recu, process
        if (recvSize > -1) process_received_PDU(ack, sockets[mic_sock].local_addr.ip_addr, sockets[mic_sock].remote_addr.ip_addr);

    } while ( pdu.header.seq_num == seq_num && need_to_resend()); // à refaire si seq num pas mise à jour, et need_to_resend

    fg[packets_sent%PDU_ENVOYE] = pdu.header.seq_num != seq_num; // mise à jour tableau d'histoire
    packets_sent++;

    printf("Total packets sent: %d\n", packets_sent);
    return sentSize;
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
    CHECK_VALID_SOCKET(socket);
    
    //déclaration struct pour stocker le message
    mic_tcp_payload payload;
    payload.data = mesg;
    payload.size = max_mesg_size;

    int effective_data_size = app_buffer_get(payload);

    return effective_data_size >= 0 ? effective_data_size : -1;
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

    // Traitement des ACKS
    if (pdu.header.ack) {
        if (pdu.header.ack_num == (seq_num+1)%2)
            seq_num = (seq_num+1)%2;
        return;
    }

    //Traitement des packets
    //Prep de ack
    mic_tcp_pdu ack = {.header = {pdu.header.dest_port, pdu.header.source_port, 0, seq_attendu, 0, 1,0}, .payload = {NULL, 0}};

    // Si pdu prevu, Seq_attendu mise à jour et app_buffer_put
    if (pdu.header.seq_num == seq_attendu) {
        app_buffer_put(pdu.payload);
        seq_attendu = (seq_attendu+1)%2;
        ack.header.ack_num = seq_attendu;
        int result = IP_send(ack, remote_addr);
        if (result < 0) perror("ACK send failed\n");
    } else {
        //sinon, ack ce qu'on a recu
        int result = IP_send(ack, remote_addr);
        if (result < 0) perror("ACK send failed\n");
    }
    
}






