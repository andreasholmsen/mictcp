#include <mictcp.h>
#include <api/mictcp_core.h>

//modifier tcp.sock
//tester: make
//./tsock.texte
//push
//tag

//tableau des sockets pas encore utilisés
mic_tcp_sock sockets[100];

/*
 * Permet de créer un socket entre l’application et MIC-TCP
 * Retourne le descripteur du socket ou bien -1 en cas d'erreur
 */

int mic_tcp_socket(start_mode sm){
    int fd = 0;
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");

    if (initialize_components(sm) != -1){
        //trouver le 1er socket non-utilisé = etat CLOSED
        while(sockets[fd].state != CLOSED){
            fd++;
        }
        //relier fd avec le nouveau socket crée/pret à être utilisé
        sockets[fd].fd = fd;
        //signaler l'utilisation du socket et renvoyer fd
        sockets[fd].state = IDLE;
        return fd;
    } else {
        return -1;
    }
}

/*
 * Permet d’attribuer une adresse à un socket.
 * Retourne 0 si succès, et -1 en cas d’échec
 */
int mic_tcp_bind(int socket, mic_tcp_sock_addr addr)
{
  
    //addr.port = 8888;
   //tcp_sock.fd = 1;
   //tcp_sock.state = IDLE; //IDLE?
   //tcp_sock.local_addr = addr;

   return 0;
   printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    if (sockets[socket].fd != socket){
        return -1;
    } else {
        sockets[socket].local_addr = addr;
        return 0;
    }

}

/*
 * Met le socket en état d'acceptation de connexions
 * Retourne 0 si succès, -1 si erreur
 */
int mic_tcp_accept(int socket, mic_tcp_sock_addr* addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");

     if (sockets[socket].fd != socket){
        return -1;
    } else {
        sockets[socket].local_addr = *addr;
        return 0;
    }
}

/*
 * Permet de réclamer l’établissement d’une connexion
 * Retourne 0 si la connexion est établie, et -1 en cas d’échec
 */
int mic_tcp_connect(int socket, mic_tcp_sock_addr addr) //socket = fd 
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    sockets[socket].remote_addr = addr;
    sockets[socket].remote_addr = addr;
    sockets[socket].state = ESTABLISHED;

    return 0;
}

/*
 * Permet de réclamer l’envoi d’une donnée applicative
 * Retourne la taille des données envoyées, et -1 en cas d'erreur
 */
int mic_tcp_send (int mic_sock, char* mesg, int mesg_size)
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
    //Creation pdu
    mic_tcp_pdu pdu;
    pdu.payload.data = mesg;
    pdu.payload.size = mesg_size;

    //Config header
    pdu.header.source_port = sockets[mic_sock].local_addr.port;
    pdu.header.dest_port = sockets[mic_sock].remote_addr.port;
    
    //Emission du pdu
    int sent_size = IP_send(pdu, sockets[mic_sock].remote_addr.ip_addr);

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
    mic_tcp_pdu pdu;

    pdu.payload.data = mesg;
    pdu.payload.size = max_mesg_size;
    int effective_data_size = app_buffer_get(pdu.payload);

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
    sockets[socket].state = CLOSING;
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
    //TODO: Verifier que le port dans pdu.header est actuellement utilisé. Passé par un bind

    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
    app_buffer_put(pdu.payload);

}
