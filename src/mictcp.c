#include <mictcp.h>
#include <api/mictcp_core.h>
#include <stdio.h>
#include <string.h>


#define NB_SOCKETS 10 // Nb de sockets actifs À la fois
#define MAX_PAYLOAD_SIZE 1000000 // Taille d'une PDU receptrice
#define ADDRESS_SIZE 32 // Taille d'une addresse IP
#define HISTPDUSIZE 10 // Taille de histPDU[]
#define MAXTIME 1 // Max temps d'attende de reception ACK (en ms)
//#define POURCENTAGEMAXPERTE 20 // pourcentage de perte acceptable  

/*===============================Début variables globales===============================*/
int seqEnvoye = 0; // Prochaine SeqNum À envoyer par source
int seqAttendu = 0; // Prochaine SeqNum attendu par puits
int nbPacketsSent = 0; // Nb de packets envoyé en total
int histPDU[HISTPDUSIZE]; // Tableau pour histoire de reussi des packets. 1 si reussi, 0 sinon
int max_perte;
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
    memset(&sockets[fd], 0, sizeof(mic_tcp_sock));
    sockets[fd].fd = fd;
    sockets[fd].state = IDLE;

    // Allocation address locale
    sockets[fd].local_addr.ip_addr.addr = malloc(ADDRESS_SIZE);
    if (sockets[fd].local_addr.ip_addr.addr == NULL) return -1;
    memset(sockets[fd].local_addr.ip_addr.addr, 0, ADDRESS_SIZE);
    sockets[fd].local_addr.ip_addr.addr_size = ADDRESS_SIZE;

    // Allocation address remote
    sockets[fd].remote_addr.ip_addr.addr = malloc(ADDRESS_SIZE);
    if (sockets[fd].remote_addr.ip_addr.addr == NULL) return -1;
    memset(sockets[fd].remote_addr.ip_addr.addr, 0, ADDRESS_SIZE);
    sockets[fd].remote_addr.ip_addr.addr_size = ADDRESS_SIZE;

    //Ports
    sockets[fd].local_addr.port = 0;
    sockets[fd].remote_addr.port = 0;

    // Init l'histoire PDU:
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
    sockets[socket].local_addr.ip_addr.addr_size = sizeof(sockets[socket].local_addr.ip_addr.addr);

    return 0;
}

/* Demande de max perte, utilisé par serveur et client
 * dans l'établissement de connéxion   
*/

void demande_max_perte(int * max_perte){
    printf("Entre 0 et 100, je propose un max perte de :\n ");
    while (1){
        scanf("%d",max_perte);
        if (*max_perte<0 || *max_perte > 100){
            printf("Il faut un entier entre 0 et 100\n");
            continue;
        } else{
            break;
        } 
    } 
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
    // Prep rec SYN
    mic_tcp_header synHeader = {0, 0, 0, 0, 0, 0, 0,0};
    mic_tcp_payload synPayload = {NULL, 0};
    mic_tcp_pdu syn = {synHeader, synPayload};

    while (1) {
        perror("Waiting for signal...");
        int recv = IP_recv(&syn, &sockets[socket].local_addr.ip_addr, &sockets[socket].remote_addr.ip_addr, 60*1000);
        printf("RECV : %d\n", recv);
        if (recv != 0) continue;

        if (!syn.header.syn || syn.header.ack || syn.header.fin) continue;
        sockets[socket].state = SYN_RECEIVED;
        // Prep envoye SynAck
        printf("SEQ %d, ACK %d\n", seqEnvoye, syn.header.seq_num+1);
        mic_tcp_header synAckHeader = {sockets[socket].local_addr.port, syn.header.source_port, seqEnvoye, syn.header.seq_num+1, 1, 1, 0,0};
        mic_tcp_payload synAckPayload = {NULL, 0};
        mic_tcp_pdu synAck = {synAckHeader, synAckPayload};

        // Envoye SynAck
        IP_send(synAck, sockets[socket].remote_addr.ip_addr);
        
        // Prep recu ACK
        mic_tcp_header ackHeader = {0, 0, 0, 0, 0, 0, 0, 0};
        mic_tcp_payload ackPayload = {NULL, 0};
        mic_tcp_pdu ack = {ackHeader, ackPayload};

        // TODO: recv2 kaller på pdu_process_pdu istedet for å returnere som 0 av seg selv.
        // blir stuck tror jeg. Forrige IP_recv litt høyere opp fungerer som den skal.
        perror("ACK À RECEVOIR");
        int recv2 = IP_recv(&ack, &sockets[socket].local_addr.ip_addr, &addr->ip_addr, 60*1000);
        perror("ACK RECU");
        if (recv2 != 0) continue;

        if(ack.header.ack && !ack.header.syn && !ack.header.fin) {
            sockets[socket].state = ESTABLISHED;
            seqAttendu = ack.header.seq_num;
            seqEnvoye = (ack.header.ack+1)%2;
            printf("SEQ ATTENDU %d, SEQ ENVOYER %d\n", seqAttendu, seqEnvoye);
            return 0;
        }
        perror("Error ack received");
    }

    //Determination de max perte propose par le serveur
    int max_perte_serv;
    demande_max_perte(&max_perte_serv);
   
    //Prep envoie de max perte propose par le serveur
    mic_tcp_header maxHeader = {sockets[socket].local_addr.port, syn.header.source_port, seqEnvoye, syn.header.seq_num+1, 0, 0, 0,max_perte_serv};
    mic_tcp_payload maxPayload = {NULL, 0};
    mic_tcp_pdu max = {maxHeader, maxPayload};
    IP_send(max,sockets[socket].remote_addr.ip_addr);

    printf("SEQ ATTENDU %d, SEQ ENVOYER %d\n", seqAttendu, seqEnvoye);

    return 0;
}

/* Permet de calculer le max perte en prenant en compte
 * les propositions côté serveur et côté client  
*/

int max_perte_negocie(int max_perte_serv, int max_perte_client){
    if (max_perte_serv >= max_perte_client){ //client impose le maximum perte
        return max_perte_client;
    } else {
        int moy = (max_perte_serv + max_perte_client)/2;
        return moy;
    }   
} 

/*
 * Permet de réclamer l’établissement d’une connexion
 * Retourne 0 si la connexion est établie, et -1 en cas d’échec
 */

int mic_tcp_connect(int socket, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
   
    // Creation SYN
    mic_tcp_header synHeader = {socket[sockets].local_addr.port, addr.port, 0, 0, 1,0,0,0};
    mic_tcp_payload synPayload = {NULL, 0};
    mic_tcp_pdu syn = {synHeader, synPayload};

    // Send SYN
    printf("SEQ %d, ACK %d\n", 0, 0);
    IP_send(syn, addr.ip_addr);
    sockets[socket].state = SYN_SENT;

    //Prep recu synAck
    mic_tcp_header synAckHeader = {0, 0, 0, 0, 0, 0, 0, 0};
    mic_tcp_payload synAckPayload = {NULL, 0};
    mic_tcp_pdu synAck = {synAckHeader, synAckPayload};

    //Attente SynAck
    int recv = IP_recv(&synAck, &sockets[socket].local_addr.ip_addr, &sockets[socket].remote_addr.ip_addr, 60*1000);
    if (recv != 0) return -1;

    if (!(synAck.header.syn && synAck.header.ack && !synAck.header.fin)) return -1;

    // Creation ACK
    mic_tcp_header ackHeader = {socket[sockets].local_addr.port, addr.port, 1, (synAck.header.seq_num+1)%2, 0,1,0,0};
    mic_tcp_payload ackPayload = {NULL, 0};
    mic_tcp_pdu ack = {ackHeader, ackPayload};

    printf("SEQ %d, ACK %d\n", ackHeader.seq_num, syn.header.seq_num+1);
    IP_send(ack, addr.ip_addr);


    seqAttendu = ack.header.ack_num;
    seqEnvoye = ack.header.seq_num;

    sockets[socket].remote_addr = addr;
    sockets[socket].state = ESTABLISHED;

    printf("SEQ ATTENDU %d, SEQ À ENVOYER %d\n", seqAttendu, seqEnvoye);

    //Determination de max perte propose par le client
    int max_perte_client;
    demande_max_perte(&max_perte_client);

    //Prep reception de max perte propose par le serveur
    mic_tcp_header maxHeader = {0, 0, 0, 0, 0, 0, 0, 0};
    mic_tcp_payload maxPayload = {NULL, 0};
    mic_tcp_pdu max = {maxHeader, maxPayload};

    //Attente max
    recv = IP_recv(&max, &sockets[socket].local_addr.ip_addr, &sockets[socket].remote_addr.ip_addr, 60*1000);
    if (recv != 0) return -1;
    if (max.header.syn || max.header.ack || max.header.fin) return -1;
    if (max.header.max_perte < 0 || max.header.max_perte > 100) return -1; //unodvendig siden det sjekkes allerede i demande_max_perte ?

    max_perte = max_perte_negocie(max_perte_client,max.header.max_perte);

    return 0;
}

/*
 * Calcul de condition de retransmission
 * A être utilisée dans mic_tcp_send()
 */
 int calculRetransmission(){
    int nbReussi = 0;

    for (int i=0;i<HISTPDUSIZE;i++) nbReussi += histPDU[i];
    int pourcentagePerte = (HISTPDUSIZE-nbReussi)*HISTPDUSIZE;

    return 1*(pourcentagePerte >max_perte);
 }

/*
 * Permet de réclamer l’envoi d’une donnée applicative
 * Retourne la taille des données envoyées, et -1 en cas d'erreur
 */
int mic_tcp_send(int mic_sock, char* mesg, int mesg_size)
{
    printf("[MIC-TCP] Appel de la fonction: %s\n", __FUNCTION__);
    
    if (mic_sock < 0 || mic_sock >= NB_SOCKETS || sockets[mic_sock].state != ESTABLISHED) return -1;

    mic_tcp_header pduHeader = {sockets[mic_sock].local_addr.port, sockets[mic_sock].remote_addr.port, seqEnvoye, 0, 0, 0, 0, 0};
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
    printf("[MIC-TCP] Appel de la fonction :  "); printf(__FUNCTION__); printf("\n");
    if (socket < 0 || socket >= NB_SOCKETS || sockets[socket].fd != socket) return -1;

    
    free(sockets[socket].local_addr.ip_addr.addr);
    free(sockets[socket].remote_addr.ip_addr.addr);
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
    perror("CALL PROCESSED PDU");
    //printf("[MIC-TCP] Appel de la fonction: %s\n", __FUNCTION__);
    //printf("seq: %d, attendu: %d \n", pdu.header.seq_num, seqAttendu);

    pdu.remote_addr.ip_addr.addr_size = sizeof(pdu.remote_addr.ip_addr.addr);//prevent seg fault

    //Ne pas envoyer si dans phase d'établissement
    if (pdu.header.syn || (pdu.header.syn && pdu.header.ack) || nbPacketsSent == 0) return;
    // si seulement ack, commence phase de négociation
    if (pdu.header.ack && !pdu.header.syn && !pdu.header.fin){
        printf("Ack recu\n ");
        //Determination de max perte propose par le serveur
        int max_perte_serv;
        demande_max_perte(&max_perte_serv);
   
        //Prep envoie de max perte propose par le serveur
        mic_tcp_header maxHeader = {pdu.header.dest_port, pdu.header.source_port, seqEnvoye, pdu.header.seq_num+1, 0, 0, 0,max_perte_serv}; //champ 3 et 4 à modifier
        mic_tcp_payload maxPayload = {NULL, 0};
        mic_tcp_pdu max = {maxHeader, maxPayload};
        IP_send(max,remote_addr);
    } 
    if (pdu.header.seq_num == seqAttendu) {
        perror("PACKET RECEIVED\n");
        seqAttendu = (seqAttendu+1) % 2;
        app_buffer_put(pdu.payload); 
    }

    //Creation de ACK
    mic_tcp_header ackHeader = {pdu.header.dest_port, pdu.header.source_port, seqEnvoye, seqAttendu, 0,1,0,0};
    mic_tcp_payload ackPayload = {NULL, 0};
    mic_tcp_pdu ack = {ackHeader, ackPayload};

    int result = IP_send(ack, remote_addr);
    if (result < 0) perror("ACK send failed\n");
    
}






