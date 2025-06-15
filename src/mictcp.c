#include <mictcp.h>
#include <api/mictcp_core.h>
#include <stdio.h>
#include <string.h>


#define NB_SOCKETS 10 // Nb de sockets actifs À la fois
#define MAX_PAYLOAD_SIZE 2000 // Taille d'une PDU receptrice
#define ADDRESS_SIZE 100 // Taille d'une addresse IP
#define HISTPDUSIZE 10 // Taille de histPDU[]
#define MAXTIME 100 // Max temps d'attende de reception ACK (en ms)
#define POURCENTAGEMAXPERTE 20 // pourcentage de perte acceptable
#define NEXT_SEQ (seqTracker[0]+1)%2
#define NEXT_ACK (seqTracker[1]+1)%2
#define EMPTY_PDU {.header = {0,0,0,0,0,0,0}, .payload = {NULL, 0}}
#define CHECK_VALID_SOCKET(fg)  if (fg < 0 || fg >= NB_SOCKETS || sockets[fg].fd != fg) return -1 // Voir si socket existe et bien initialisé

/*===============================Début variables globales===============================*/

mic_tcp_sock sockets[NB_SOCKETS]; //tableau des sockets
int seqTracker[] = {1,1}; // seqTracker[0] = pour le source. [1] = pour le puits
int nbPacketsSent = 0; // Nb de packets envoyé en total
int histPDU[HISTPDUSIZE]; // Tableau pour histoire de reussi des packets. 1 si reussi, 0 sinon
int lastsocketused = -1; // pour visibilité d'un socket dans process_received_PDU()

/*===============================Fin variables globales=================================*/





/*
 * Permet de créer un socket entre l’application et MIC-TCP
 * Retourne le descripteur du socket ou bien -1 en cas d'erreur
 */

int mic_tcp_socket(start_mode sm) {
    printf("[MIC-TCP] Appel de la fonction: %s\n", __FUNCTION__);
    if (initialize_components(sm) == -1) return -1;
    
    // Trouver premier socket pas utilisé
    int fd;
    for (fd = 0; fd < NB_SOCKETS; fd++) if (sockets[fd].state == CLOSED) break;
    
    // Voir si trouvé
    if (fd >= NB_SOCKETS) return -1;
    
    // Init de socket
    memset(&sockets[fd], 0, sizeof(mic_tcp_sock));
    sockets[fd].fd = fd;

    // Init l'histoire PDU:
    for (int i = 0; i < HISTPDUSIZE; i++) histPDU[i] = 0;
    lastsocketused = fd;
    return fd;
}

/*
 * Permet d'attribuer une adresse à un socket.
 * Retourne 0 si succès, et -1 en cas d'échec
 */
int mic_tcp_bind(int socket, mic_tcp_sock_addr addr){
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    CHECK_VALID_SOCKET(socket);

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
    lastsocketused = socket; // Pour visibilité dans process_received_PDU

    mic_tcp_sock_addr remote_addr = {.ip_addr = {.addr = malloc(ADDRESS_SIZE), .addr_size = ADDRESS_SIZE}}; //init d'un remote_addr

    //prep rec SYN
    mic_tcp_pdu syn = EMPTY_PDU;
    while(1) {
        printf("Waiting for signal...\n");
        int recv = IP_recv(&syn, NULL, &remote_addr.ip_addr, 60*1000);
        if (recv > -1) process_received_PDU(syn, sockets[socket].local_addr.ip_addr, remote_addr.ip_addr); 

        if (sockets[socket].state != SYN_RECEIVED) continue;

        //prep send synack
        mic_tcp_pdu synack = {.header = {syn.header.dest_port, syn.header.source_port, 0, 1, 1,1,0}, .payload = {NULL, 0}};
        if (IP_send(synack, remote_addr.ip_addr) < 0) return -1;

        //prep rec ACK
        mic_tcp_pdu ack = EMPTY_PDU;
        int recv2 = IP_recv(&ack, NULL, &remote_addr.ip_addr, 5*1000);
        if (recv2 > -1) process_received_PDU(ack, sockets[socket].local_addr.ip_addr, remote_addr.ip_addr); 

        if (sockets[socket].state != CONNECTED) return -1; // Check valide 
        sockets[socket].remote_addr = remote_addr;

        // SEND PAX PERTE PACKAGE

        sockets[socket].state = ESTABLISHED;
        free(remote_addr.ip_addr.addr);
        perror("ESTABLISHED");
        return 0;
        }

    return -1;
}

/*
 * Permet de réclamer l’établissement d’une connexion
 * Retourne 0 si la connexion est établie, et -1 en cas d’échec
 */

int mic_tcp_connect(int socket, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    CHECK_VALID_SOCKET(socket);

    lastsocketused = socket; // Pour visibilité dans process_received_PDU
    mic_tcp_sock_addr remote_addr = {.ip_addr = {.addr = malloc(ADDRESS_SIZE), .addr_size = ADDRESS_SIZE}}; //init d'un remote_addr
    
    //prep send SYN
    mic_tcp_pdu syn = {.header = {sockets[socket].local_addr.port, addr.port, 0, 0, 1,0,0}, .payload = {NULL, 0}};
    if (IP_send(syn, addr.ip_addr) < 0) return -1;
    sockets[socket].state = SYN_SENT;

    // attente SYNACK
    mic_tcp_pdu synack = EMPTY_PDU;
    int recv = IP_recv(&synack, NULL, &remote_addr.ip_addr, 10*1000);
    if (recv > -1) process_received_PDU(synack, sockets[socket].local_addr.ip_addr, remote_addr.ip_addr);
    
    if (sockets[socket].state != CONNECTED) return -1;

    //prep send ACK
    mic_tcp_pdu ack = {.header = {socket[sockets].local_addr.port, addr.port, 1, 1, 0,1,0}, .payload = {NULL, 0}};
    if (IP_send(ack, addr.ip_addr) < 0) return -1;

    //Association d'addresse
    sockets[socket].remote_addr = addr;
    
    // RECEIVE MAX PERTE PACKET

    sockets[socket].state = ESTABLISHED;

    free(remote_addr.ip_addr.addr);

    return 0;
}

/*
 * Calcul de condition de retransmission
 * A être utilisée dans mic_tcp_send()
 * Renvoye 1 si besoin de retransmettre, 0 sinon
 */
 int calculRetransmission(){
    int nbReussi = 0;
    for (int i=0;i<HISTPDUSIZE;i++) nbReussi += histPDU[i];
    int pourcentagePerte = 100*(HISTPDUSIZE-nbReussi)*HISTPDUSIZE;

    printf("Packet perdu. Besoin de retransmettre? %d\n", pourcentagePerte >POURCENTAGEMAXPERTE);
    return pourcentagePerte >POURCENTAGEMAXPERTE;
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

    //prep envoye pdu
    mic_tcp_pdu pdu = {.header = {sockets[mic_sock].local_addr.port,sockets[mic_sock].remote_addr.port,seqTracker[0],0,0,0,0}, .payload = {mesg, mesg_size}};

    int sentSize = -1;
    do { // while nbSeq pas mise à jour et besoin de retransmission
        if ((sentSize = IP_send(pdu, sockets[mic_sock].remote_addr.ip_addr) < 0)) return -1;

        //prep rec ACK
        mic_tcp_pdu ack = EMPTY_PDU;
        int recvSize = IP_recv(&ack, NULL, &sockets[mic_sock].remote_addr.ip_addr, MAXTIME);
        if (recvSize > -1) process_received_PDU(ack, sockets[mic_sock].local_addr.ip_addr, sockets[mic_sock].remote_addr.ip_addr);

    } while ( pdu.header.seq_num == seqTracker[0] && calculRetransmission()); 

    histPDU[nbPacketsSent%HISTPDUSIZE] = pdu.header.seq_num != seqTracker[0]; // Mise à jour fenetre glissante. 1 si success, 0 sinon
    nbPacketsSent++;

    printf("Total packets sent: %d\n", nbPacketsSent);
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

    mic_tcp_payload payload = {mesg, max_mesg_size};
    int effectiveSize = app_buffer_get(payload);

    return effectiveSize >= 0 ? effectiveSize : -1;
}

/*
 * Permet de réclamer la destruction d’un socket.
 * Engendre la fermeture de la connexion suivant le modèle de TCP.
 * Retourne 0 si tout se passe bien et -1 en cas d'erreur
 */
int mic_tcp_close (int socket)
{
    printf("[MIC-TCP] Appel de la fonction :  "); printf(__FUNCTION__); printf("\n");
    CHECK_VALID_SOCKET(socket);
    
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

     // RECEPTION ACK DANS PHASE D'ETABLISSEMENT
    if (sockets[lastsocketused].state == SYN_RECEIVED) {
        if (pdu.header.ack && !pdu.header.syn && !pdu.header.fin) {
            sockets[lastsocketused].state = CONNECTED;

            // FONCTIONALITE V4.2
            // Extract PDU
            // set max perte = pdu.payload recu
        } 
    }

    // RECEPTION SYN DANS PHASE D'ETABLISSEMENT
    if (sockets[lastsocketused].state == IDLE) {
        if (pdu.header.syn && !pdu.header.ack && !pdu.header.fin) {
            sockets[lastsocketused].state = SYN_RECEIVED;
        } 
    }

    // RECEPTION SYNACK DANS PHASE D'ETABLISSEMENT
    if (sockets[lastsocketused].state == SYN_SENT) {
        if (pdu.header.syn && pdu.header.ack && !pdu.header.fin) {
            sockets[lastsocketused].state = CONNECTED;
        }
    }

    // SI PAS PRÊT À ENVOYER, RENVOYE
    if (sockets[lastsocketused].state != ESTABLISHED) return;

    // RECEPTION ACK, SOURCE
    if (pdu.header.ack) {
        if (pdu.header.ack_num == NEXT_SEQ) {
            seqTracker[0] = NEXT_SEQ;
        }
        return;
    } 

    //PROCESS PDU, PUITS
    mic_tcp_pdu ack = {.header = {pdu.header.dest_port, pdu.header.source_port, 0, seqTracker[1], 0, 1,0}, .payload = {NULL, 0}};

    // si PDU attendu, mise à jour seq attendu et app_buffer_put
    if (pdu.header.seq_num == seqTracker[1]) {
        app_buffer_put(pdu.payload);
        seqTracker[1] = NEXT_ACK;
        ack.header.ack_num = seqTracker[1]; // ack la prochaine pdu prevu
    }  //si pas PDU attendu, ack ce qu'on a recu. Pas mise à jour
       
    int sendSize = IP_send(ack, remote_addr);
    if (sendSize < 0) perror("renvoye ACK echoé");
}






