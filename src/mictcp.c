#include <mictcp.h>
#include <api/mictcp_core.h>
//start mode
start_mode start_m;
//socket local
mic_tcp_sock sock;
//adresse de dest
mic_tcp_sock_addr addr_dest;
//pdu_global global 
mic_tcp_pdu pdu_global;
//variable globale num de seq et de ack
unsigned int PA=0;
unsigned int PE=0;
/*  
 * Permet de créer un socket entre l’application et MIC-TCP
 * Retourne le descripteur du socket ou bien -1 en cas d'erreur
 */
int mic_tcp_socket(start_mode sm)
{
    int result = -1;
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    start_m=sm;
    result = initialize_components(sm); /* Appel obligatoire */
    set_loss_rate(0);
    if (result!=-1)
    {
        sock.state=IDLE;
        sock.fd=0;
        return sock.fd;
    }
    return result;
}

/*
 * Permet d’attribuer une adresse à un socket.
 * Retourne 0 si succès, et -1 en cas d’échec
 */
int mic_tcp_bind(int socket, mic_tcp_sock_addr addr)
{
   printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
   if (sock.fd==socket)
   {
        sock.addr=addr;
        return sock.fd;
   }
   return -1;
}

/*
 * Met le socket en état d'acceptation de connexions
 * Retourne 0 si succès, -1 si erreur
 */
int mic_tcp_accept(int socket, mic_tcp_sock_addr* addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    if (sock.fd==socket)
    {
        sock.state=ESTABLISHED;
        return sock.fd;
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
    if (sock.fd==socket)
    {
        addr_dest=addr;
        sock.state=ESTABLISHED;
        return sock.fd;
    }
    return -1;
}

/*
 * Permet de réclamer l’envoi d’une donnée applicative
 * Retourne la taille des données envoyées, et -1 en cas d'erreur
 */
int mic_tcp_send (int mic_sock, char* mesg, int mesg_size)
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
    if (sock.fd==mic_sock)
    {
        pdu_global.header.source_port=sock.addr.port;
        pdu_global.header.dest_port=addr_dest.port;
        pdu_global.payload.size=mesg_size;
        pdu_global.payload.data=mesg;
        pdu_global.header.seq_num=PE;
        pdu_global.header.ack='0';
        IP_send(pdu_global,addr_dest);
        mic_tcp_pdu pdu;
        int retour;
        retour=IP_recv(&pdu,&addr_dest,1);
        printf("retour =%d \n" , retour);
        int numb=0;
        printf("%c, %d, %d \n",pdu.header.ack,pdu.header.ack_num,pdu_global.header.seq_num);
        while (!(pdu.header.ack=='1' && pdu.header.ack_num==pdu_global.header.seq_num &&numb<100))
        {
            printf("ack not received \n");
            numb++;
            mic_tcp_send(mic_sock, mesg, mesg_size);
            retour=IP_recv(&pdu,&addr_dest,1); 
            printf("retour while=%d \n" , retour);
        }
        PE=(PE+1)%2;
        return sock.fd;
    }
    return -1;
}

/*
 * Permet à l’application réceptrice de réclamer la récupération d’une donnée
 * stockée dans les buffers de réception du socket
 * Retourne le nombre d’octets lu ou bien -1 en cas d’erreur
 * NB : cette fonction fait appel à la fonction app_buffer_get()
 */
int mic_tcp_recv (int socket, char* mesg, int max_mesg_size)
{
    int result = -1;
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
    if (sock.fd==socket)
    {
        mic_tcp_payload p;
        p.size=max_mesg_size;
        p.data=mesg;
        result=app_buffer_get(p);
        mesg=p.data;
    }
    return result;
}

/*
 * Permet de réclamer la destruction d’un socket.
 * Engendre la fermeture de la connexion suivant le modèle de TCP.
 * Retourne 0 si tout se passe bien et -1 en cas d'erreur
 */
int mic_tcp_close (int socket)
{
    printf("[MIC-TCP] Appel de la fonction :  "); printf(__FUNCTION__); printf("\n");
    if (sock.fd==socket)
    {
        sock.fd=1;
        return sock.fd;
    }
    return -1;
}

/*
 * Traitement d’un PDU MIC-TCP reçu (mise à jour des numéros de séquence
 * et d'acquittement, etc.) puis insère les données utiles du PDU dans
 * le buffer de réception du socket. Cette fonction utilise la fonction
 * app_buffer_put().
 */
void process_received_PDU(mic_tcp_pdu pdu, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
    app_buffer_put(pdu.payload);
    if (pdu.header.ack=='0')
    {
        pdu_global.header.ack='1';
        pdu_global.header.ack_num=pdu.header.seq_num;
        printf("envoie ack \n");
        IP_send(pdu_global,addr);
    }
    
}
