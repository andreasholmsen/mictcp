#include <mictcp.h>
#include <api/mictcp_core.h>
#include <stdio.h>
#include <string.h>


#define NB_SOCKETS 10 // Nb de sockets actifs À la fois
#define MAX_PAYLOAD_SIZE 1000000 // Taille d'une PDU receptrice
#define ADDRESS_SIZE 16 // Taille d'une addresse IP
#define HISTPDUSIZE 10 // Taille de histPDU[]
#define MAXTIME 1 // Max temps d'attende de reception ACK (en ms)
#define POURCENTAGEMAXPERTE 20 // pourcentage de perte acceptable

/*===============================Début variables globales===============================*/
int seqEnvoye = 0; // Prochaine SeqNum À envoyer par source
int seqAttendu = 0; // Prochaine SeqNum attendu par puits
int nbPacketsSent = 0; // Nb de packets envoyé en total
int histPDU[HISTPDUSIZE]; // Tableau pour histoire de reussi des packets. 1 si reussi, 0 sinon
/*===============================Fin variables globales=================================*/


//tableau des sockets
mic_tcp_sock sockets[NB_SOCKETS];

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
    memset(&sockets[fd], 0, sizeof(sockets[fd]));
    sockets[fd].fd = fd;
    sockets[fd].state = IDLE;

    sockets[fd].local_addr.ip_addr.addr = "localhost";
    sockets[fd].local_addr.ip_addr.addr_size = sizeof("localhost");

    // Init de histoire PDU:
    for (int i = 0; i < HISTPDUSIZE; i++) histPDU[i] = 0;

    return fd;
}

/*
 * Permet d'attribuer une adresse à un socket.
 * Retourne 0 si succès, et -1 en cas d'échec
 */
int mic_tcp_bind(int socket, mic_tcp_sock_addr addr){
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");

    // Check si index valable et bien initialisé
    if (socket < 0 || socket >= NB_SOCKETS || sockets[socket].fd != socket) return -1;

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
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");

    if (socket < 0 || socket >= NB_SOCKETS || sockets[socket].fd != socket) return -1;

    //*addr = sockets[socket].remote_addr; // Association d'addresse

    //Prep de SYN
    mic_tcp_pdu syn;

    while(1) {
        printf("WAITING...\n");
        int recv = IP_recv(&syn, &sockets[socket].local_addr.ip_addr, NULL, 10000);
        printf("SYN RECEIVED: seq nr%d, ack nr %d\n", syn.header.seq_num, syn.header.ack_num);

        if (recv == -1) continue;
        
        if (syn.header.syn) {
            sockets[socket].state = SYN_RECEIVED;

           //Creation de SYNACK
            mic_tcp_header synAckHeader = {sockets[socket].local_addr.port, syn.header.source_port, seqEnvoye, (syn.header.seq_num+1)%2,1,1,0};
            mic_tcp_payload synAckPayload = {NULL, 0};
            mic_tcp_pdu synAck = {synAckHeader, synAckPayload};
            
            
            printf("SENT SYNACK: seq nr%d, ack nr %d\n", synAck.header.seq_num, synAck.header.ack_num);
            int sendSize = IP_send(synAck, addr->ip_addr);
            if (sendSize < 0) return -1;
            
            //Prep ack
            mic_tcp_pdu ack;

            recv = IP_recv(&ack, &sockets[socket].local_addr.ip_addr, &(addr->ip_addr), 100000);

        //    printf("ACK Receieved: seq nr%d, ack nr %d\n", ack.header.seq_num, ack.header.ack_num);
            if (recv != 1 && ack.header.ack && ack.header.ack_num == (seqAttendu)) {
                sockets[socket].state = ESTABLISHED;
                sockets[socket].remote_addr =*addr;
                seqEnvoye = (seqEnvoye+1)%2; // SEQ = 1

                seqAttendu = (ack.header.ack_num+1)%2;
            }
        }

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
    if (socket < 0 || socket >= NB_SOCKETS || sockets[socket].fd != socket) return -1;

    sockets[socket].remote_addr = addr;

    //Creation de SYN
    mic_tcp_header synHeader = {sockets[socket].local_addr.port, addr.port, seqEnvoye, 0, 1,0,0}; // SYN = 1
    mic_tcp_payload synPayload = {NULL, 0};
    mic_tcp_pdu syn = {synHeader, synPayload};

    // Prep de SYNACK
    mic_tcp_pdu synAck;

    IP_send(syn, addr.ip_addr);
    //printf("SENT SYN: seq nr%d, ack nr %d\n", syn.header.seq_num, syn.header.ack_num);
    sockets[socket].state = SYN_SENT;

    while(1) {
        int recv = IP_recv(&synAck, &(sockets[socket].local_addr.ip_addr), &addr.ip_addr, 100000);
        printf("SYNACK RECIEVED: seq nr%d, ack nr %d\n", synAck.header.seq_num, synAck.header.ack_num);

        if (recv > -1) break;
    }
    

    if (synAck.header.syn && synAck.header.ack && synAck.header.ack_num == (seqEnvoye +1) % 2) { // si ACK = 1
        seqEnvoye = (seqEnvoye+1)%2;
        //Creation de ACK
        mic_tcp_header ackHeader = {sockets[socket].local_addr.port, addr.port, seqEnvoye, (synAck.header.seq_num+1) %2, 0,1,0};
        mic_tcp_payload ackPayload = {NULL, 0};
        mic_tcp_pdu ack = {ackHeader, ackPayload};

        printf("SENT ACK: seq nr%d, ack nr %d\n", ack.header.seq_num, ack.header.ack_num);
        int result = IP_send(ack, addr.ip_addr);
        if (result < 0) printf("ACK send failed\n");

        sockets[socket].state = ESTABLISHED;
        seqEnvoye = (seqEnvoye + 1) % 2; // SEQ POUR PREMIER TRANSFERT = 1
        return 0;
    }

    return -1;
}

/*
 * Calcul de condition de retransmission
 * A être utilisée dans mic_tcp_send()
 */
 int calculRetransmission(){
    int nbReussi = 0;

    for (int i=0;i<HISTPDUSIZE;i++) nbReussi += histPDU[i];
    int pourcentagePerte = (HISTPDUSIZE-nbReussi)*HISTPDUSIZE;

    return 1*(pourcentagePerte >POURCENTAGEMAXPERTE);
 }

/*
 * Permet de réclamer l’envoi d’une donnée applicative
 * Retourne la taille des données envoyées, et -1 en cas d'erreur
 */
int mic_tcp_send(int mic_sock, char* mesg, int mesg_size)
{
    printf("[MIC-TCP] Appel de la fonction: %s\n", __FUNCTION__);
    
    if (mic_sock < 0 || mic_sock >= NB_SOCKETS || sockets[mic_sock].state != ESTABLISHED) return -1;

    mic_tcp_header pduHeader = {sockets[mic_sock].local_addr.port, sockets[mic_sock].remote_addr.port, seqEnvoye, 0, 0, 0, 0,};
    mic_tcp_payload pduPayload = {mesg, mesg_size};
    mic_tcp_pdu pdu = {pduHeader, pduPayload};

    int sentSize = IP_send(pdu, sockets[mic_sock].remote_addr.ip_addr);
    if (sentSize < 0) return -1;

    mic_tcp_pdu ackPdu;

    while (1) {
        int recv = IP_recv(&ackPdu, &sockets[mic_sock].local_addr.ip_addr, &sockets[mic_sock].remote_addr.ip_addr, MAXTIME); // Recoit le ACK
        if (recv == -1) { // Si timeout

            if(calculRetransmission()) { // Si besoin de retransmettre
                printf("RETRANSMISSION\n");
                IP_send(pdu, sockets[mic_sock].remote_addr.ip_addr);
                 continue; //Recommence l'attente de ACK
            }

            printf("DROPPING PACKET\n");
            //Si pas besoin de retransmettre, on met à jour le tableau, et on fini le boucle
            histPDU[nbPacketsSent%HISTPDUSIZE] = 0;
           break;
        } else if (ackPdu.header.ack && ackPdu.header.ack_num == (seqEnvoye+1)%2) {
            printf("ACK RECIEVED, packets sent: %d\n", nbPacketsSent);
            seqEnvoye = (seqEnvoye + 1) % 2;
            histPDU[nbPacketsSent%HISTPDUSIZE] = 1;
            break;
        }
    }

    nbPacketsSent++;
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
    
    // Check if socket is valid
    if (socket < 0 || socket >= NB_SOCKETS || sockets[socket].fd != socket) return -1;
    
    //déclaration struct pour stocker le message
    mic_tcp_payload payload;
    payload.data = mesg;
    payload.size = max_mesg_size;

    int effectiveSize = app_buffer_get(payload);

    if (effectiveSize >= 0) return effectiveSize;
    return -1;
}

/*
 * Permet de réclamer la destruction d’un socket.
 * Engendre la fermeture de la connexion suivant le modèle de TCP.
 * Retourne 0 si tout se passe bien et -1 en cas d'erreur
 */
int mic_tcp_close (int socket)
{
    if (socket < 0 || socket >= NB_SOCKETS || sockets[socket].fd != socket) return -1;

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
    //printf("seq: %d, attendu: %d \n", pdu.header.seq_num, seqAttendu);



    if (pdu.header.seq_num == seqAttendu) {
        //printf("PACKET RECIEVED\n");
        seqAttendu = (seqAttendu+1) % 2;
        app_buffer_put(pdu.payload); 
    }

    //Creation de ACK
    mic_tcp_header ackHeader = {pdu.header.dest_port, pdu.header.source_port, seqEnvoye, seqAttendu, 0,1,0};
    mic_tcp_payload ackPayload = {NULL, 0};
    mic_tcp_pdu ack = {ackHeader, ackPayload};

    int result = IP_send(ack, remote_addr);
    if (result < 0) printf("ACK send failed\n");
    
}






