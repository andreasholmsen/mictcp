#include <mictcp.h>
#include <api/mictcp_core.h>

/*===========================Version 1: Phase de tranfert de données sans garantie de fiabilité=================================*/
mic_tcp_sock tcp_sock;
mic_tcp_pdu pdu;

/*
 * Permet de créer un socket entre l’application et MIC-TCP
 * Retourne le descripteur du socket ou bien -1 en cas d'erreur
 */
int mic_tcp_socket(start_mode sm)
{
   printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
   initialize_components(sm); /* Appel obligatoire */
   //set_loss_rate(0);

   return 1; //Seulement un socket utilisé donc ID = 1? TODO: Changer
}

/*
 * Permet d’attribuer une adresse à un socket.
 * Retourne 0 si succès, et -1 en cas d’échec
 */
int mic_tcp_bind(int socket, mic_tcp_sock_addr addr)
{
   printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
   
   addr.port = 8888;
   tcp_sock.fd = 1;
   tcp_sock.state = IDLE; //IDLE?
   tcp_sock.remote_addr = addr; // Remote ou local addr?

   return 0;
}

/*
 * Met le socket en état d'acceptation de connexions
 * Retourne 0 si succès, -1 si erreur
 */
int mic_tcp_accept(int socket, mic_tcp_sock_addr* addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    return -1;
}

/*
 * Permet de réclamer l’établissement d’une connexion
 * Retourne 0 si la connexion est établie, et -1 en cas d’échec
 */
int mic_tcp_connect(int socket, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    tcp_sock.remote_addr = addr;
    
    return 0;
}

/*
 * Permet de réclamer l’envoi d’une donnée applicative
 * Retourne la taille des données envoyées, et -1 en cas d'erreur
 */
int mic_tcp_send (int mic_sock, char* mesg, int mesg_size)
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
    
    pdu.payload.data = mesg;
    pdu.payload.size = mesg_size;

    pdu.header.source_port = 8889;
    pdu.header.dest_port = tcp_sock.remote_addr.port;
    
    pdu.header.seq_num = 0; // TODO: Changer
    pdu.header.ack_num = 0; // TODO: Changer

    pdu.header.syn = 0;
    pdu.header.ack = 0;
    pdu.header.fin = 0;
   
    int sent_size = IP_send(pdu, tcp_sock.remote_addr.ip_addr);

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
    
    pdu.payload.data = mesg;
    pdu.payload.size = max_mesg_size;

    int effective_data_size = app_buffer_get(pdu.payload);

    return effective_data_size; // TODO: Ou -1 si erreur
}

/*
 * Permet de réclamer la destruction d’un socket.
 * Engendre la fermeture de la connexion suivant le modèle de TCP.
 * Retourne 0 si tout se passe bien et -1 en cas d'erreur
 */
int mic_tcp_close (int socket)
{
    printf("[MIC-TCP] Appel de la fonction :  "); printf(__FUNCTION__); printf("\n");
    return -1;
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
