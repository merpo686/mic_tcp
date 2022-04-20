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
    set_loss_rate(1);
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
    mic_tcp_pdu pdu={0};
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
    if (sock.fd==mic_sock)
    {
        pdu_global.header.source_port=sock.addr.port;
        pdu_global.header.dest_port=addr_dest.port;
        pdu_global.payload.size=mesg_size;
        pdu_global.payload.data=mesg;
        pdu_global.header.seq_num=PE;
        pdu_global.header.ack='0';
        pdu_global.header.ack_num=-1;
        pdu_global.header.syn='0';
        pdu_global.header.fin='0';
        int Timeout=0; // Timeout=-1 => il y a eu timeout
        int nbRetransmit=5;
        int Sent=-1; 
        // Transmission PDU 
        Sent=IP_send(pdu_global,addr_dest);
        do {
            Timeout=IP_recv(&pdu,&addr_dest,100);
            if (Timeout == -1) { // il y a eu timeout
                // Retransmission 
                printf("Timeout. Send again\n");	
                Sent=IP_send(pdu_global,addr_dest);
                nbRetransmit--;
            }
            else // PDU recu
            {
                if (pdu.header.ack=='1' && pdu.header.ack_num==PE) { // c'est le bon ACK
                printf("pdu %d bien acquitte - PE=%d\n",pdu_global.header.seq_num,PE);
                }
                else 
                {
                    printf("BAD Ack received. Send again\n");	
                    Sent=IP_send(pdu_global,addr_dest);
                    nbRetransmit--;
                }
            }
        }
        while (Timeout==-1 && nbRetransmit>0);
        // fin  while , on a fini de traiter le Message
        if (nbRetransmit==0 || (Timeout==-1)) { // Nbre de retransmission max atteint
            printf("Nbr retrans max atteint !PE=%d ! \n",PE);
        }
        PE=(PE+1)%2;
        return Sent;
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
    if (pdu.header.ack=='0')
    { 
        pdu_global.header.ack='1';
        pdu_global.header.source_port=sock.addr.port;
        pdu_global.header.dest_port=addr_dest.port;
        pdu_global.header.seq_num=-1;
        pdu_global.header.ack_num=PA;
        pdu_global.header.syn='0';
        pdu_global.header.fin='0';
        printf("envoie ack %d \n",pdu_global.header.ack_num);
        while(IP_send(pdu_global,addr)==-1){
            printf("echec d'envoi du ack, on le renvoie");
        } 
        if (  pdu.header.seq_num>=PA)  { 
            app_buffer_put(pdu.payload);
        }
        else {
            printf("mauvais num\n");
        }
        PA=(pdu.header.seq_num+1) % 2;
    } 
}
