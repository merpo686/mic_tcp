#include <mictcp.h>
#include <api/mictcp_core.h>
#include <pthread.h>
//start mode
start_mode start_m;
//socket local
mic_tcp_sock sock;
//adresse de dest
mic_tcp_sock_addr addr_dest;
//pdu_global global 
mic_tcp_pdu pdu_global={0};
//variable globale num de seq et de ack
unsigned int PA=0;
unsigned int PE=0;
//reprise des pertes : tableau des 10 derniers messages
double pertes[10] ={1}; 
//taux de pertes admissibles
double perte_admi=0.2;
/*  
 * Permet de créer un socket entre l’application et MIC-TCP
 * Retourne le descripteur du socket ou bien -1 en cas d'erreur
 */
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;


int mic_tcp_socket(start_mode sm)
{
    int result = -1;
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    start_m=sm;
    result = initialize_components(sm); /* Appel obligatoire */
    set_loss_rate(51);
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
        printf("On attend le syn\n");
        pthread_cond_wait(&cond,&mutex);
        pthread_mutex_unlock(&mutex);
        printf("connexion établie\n");
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
        mic_tcp_pdu syn={0};
        mic_tcp_pdu synack={0};
        syn.header.syn='1';
        syn.header.source_port=sock.addr.port;
        syn.header.dest_port=addr.port;
        int nbboucle=100;
        do{
            while(IP_send(syn,addr)==-1){
                printf("echec d'envoi du syn, on le renvoie\n");
            }
            printf("Syn envoyé\n");
            sock.state=SYN_SENT;
            nbboucle--;
        }
        while(IP_recv(&synack,&addr,1000)==-1 && !(synack.header.syn=='1' && synack.header.ack=='1')&& nbboucle>0 );
        printf("SYNACK reçu, on va envoyer le ACK\n");
        syn.header.syn='0';
        syn.header.ack='1';
        syn.header.source_port=sock.addr.port;
        syn.header.dest_port=addr.port;
        while(IP_send(syn,addr)==-1){
            printf("echec d'envoi du ack, on le renvoie\n");
        } 
        printf("ACK envoyé, connexion établie\n");
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
  printf("\n[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
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
      pertes[PE] = 0;
      
      // La regle du stop and wait est qu'une retransmission n'est seulement declenchee qu'au timeout et pas en reaction a une reception d'un PDU inapproprie
      
      int Timeout=0; // Timeout=-1 => il y a eu timeout
      int nbRetransmit=5;
      // on anticipe la perte pour la prise de déc
      // precalcul du taux de reussite actuel ..
      double sum=0;
      for (int i=0; i<10;i++){
	    sum+=pertes[i];
      }
     
      int Sent=-1;
      int Retransmettre=(perte_admi + (sum)/10) < 1;// à 1 => retransmission necessaire 
      printf("Estimation avant envoi dans le cas où l'on perd le paquet,\nTaux de perte : %f,Taux Livraison:%f/10 et Retransmettre=%d \n",1-sum/10,sum,Retransmettre);
      
      // Transmission PDU 
      Sent=IP_send(pdu_global,addr_dest);
      do {
	Timeout=IP_recv(&pdu,&addr_dest,100);
	if (Timeout == -1) { // il y a eu timeout
	  if (Retransmettre) {  // il y a necessite de retransmisison 
	      // Retransmission 
	      Sent=IP_send(pdu_global,addr_dest);
	      nbRetransmit--;
	    }
	}
	else // PDU recu
	  {
	    if (pdu.header.ack=='1' && pdu.header.ack_num==pdu_global.header.seq_num) { // c'est le bon ACK
	      pertes[PE]=1;
	      printf("pdu %d bien acquitte - PE=%d\n",pdu_global.header.seq_num,PE);
	    }
	    else 
	      {
		    printf("BAD Ack received. Taux de réussite actuel: %f \n",perte_admi + sum/10);	
	      }
	    
	  }
	
      }
      while (Timeout==-1 && Retransmettre && nbRetransmit>0);

      // fin  while , on a fini de traiter le Message
      if (nbRetransmit==0 || (Timeout==-1 && !Retransmettre)) { // Nbre de retransmission max atteint ou perte admissible, marquer paquet perdu
	//	pertes[PE] deja initialisées à 0 (odouble pertes[10] ={1};n part de l'hypothese d'une perte) on pourra enlever cette condition pour la version optimisee
	printf("perte admissible ou Nbr retrans max atteint !  Nb Retrans restante: %d,PE=%d ! \n",nbRetransmit,PE);

      }
      PE=(PE+1)%10;
	//        printf("ack received \n");

        return Sent;
    }
    return -1;
}

/*
 * Permet à l’application réceptrice de réclamer la récupération d’une donnée
 * stockée dans les buffers de réception du socket
 * Retourne le nombre d’octets lu ou bien -1 en cas d’erre        else{
            printf("echec de connexion\n");
        }ur
 * NB : cette fonction fait appel à la fonction app_buffer_get()
 */
int mic_tcp_recv (int socket, char* mesg, int max_mesg_size)
{
    int result = -1;
    printf("\n[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
    if (sock.fd==socket)
    {
        mic_tcp_payload p;
        p.size=max_mesg_size;
        p.data=mesg;
        result=app_buffer_get(p);
        mesg=p.data;
    }
    printf("[MIC-TCP-RCV] Message recu:%s\n",mesg);
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

  printf("[MIC-TCP] Appel de la fonction (seqnum,acknum) :%d;%d",pdu.header.seq_num,pdu.header.ack_num); printf(__FUNCTION__); printf("\n");
    if (pdu.header.ack=='0' && pdu.header.syn=='0')
    { 
        pdu_global.header.ack='1';
        pdu_global.header.source_port=sock.addr.port;
        pdu_global.header.dest_port=addr_dest.port;
        pdu_global.header.seq_num=-1;
        pdu_global.header.ack_num=pdu.header.seq_num; //si l'emetteur a decidé de zapper des numéros, on se recale dessus
        pdu_global.header.syn='0';
        pdu_global.header.fin='0';
        printf("envoie ack %d \n",pdu_global.header.ack_num);
        while(IP_send(pdu_global,addr)==-1){
            printf("echec d'envoi du ack, on le renvoie");
        } 
        if (  pdu.header.seq_num>=PA)  { 
            app_buffer_put(pdu.payload);
            printf("[MIC-process] Message attendu recu:%s\n",pdu.payload.data);
        }
        else {
            printf("Mauvais num de seq. Message déjà reçu ou déjà abandonné. Envoi d'un ack pour que l'envoyeur passe au prochain");
        }
        PA=(pdu.header.seq_num+1) % 10;
    } 
    else if (pdu.header.ack=='1'){
        //envoie la variable de condition (signal ou broadcast) pour réveiller accept
        printf("récéption du ack de connexion\n");
    }
    else if(pdu.header.syn=='1'){
        printf("Syn reçu, on envoie le synack\n");
        sock.state=SYN_RECEIVED;
        pdu_global.header.ack='1';
        pdu_global.header.source_port=sock.addr.port;
        pdu_global.header.dest_port=addr_dest.port;
        pdu_global.header.seq_num=-1;
        pdu_global.header.ack_num=-1; 
        pdu_global.header.syn='1';
        pdu_global.header.fin='0';
        while(IP_send(pdu_global,addr)==-1){
            printf("echec d'envoi du synack, on le renvoie");
        } 
        pthread_cond_signal(&cond);
    }
    
}
