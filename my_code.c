#include "arpnet.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>  
#include<string.h>
//-----------------------Functions
void find_next(node_id my_id, handshake_t hsm,int first);
handshake_t read_hsm(int sockfd);
node_id init_master(node_id my_id, int sockfd);
void init_general(node_id my_id,int sockfd);
message_t read_msg(int sockfd);
void deb();
void sent_RURZ(float band_tot,float band_fly);
void write_standard_msg(message_t msg, node_id my_id, node_id receiver);
//-----------------------Main
int main(int argc, char* argv[]){
	if (argc!=2){
		perror("wrong number of arguments, Please, also type the ip address");
		return -1;
	}
	char* my_ip=argv[1]; 
	node_id my_id = iptab_get_ID_of( my_ip );
	handshake_t hsm; //msg of type handshake
	message_t msg3; //msg of type std
	char* ip_winner; //varable to store winner ip
	node_id winner;//variable to store winner id
	int flag_winner=0;//variable to set if winner or not(initialized as NTL)
	int fd_nodnext;
	int fd_nodprec;
	int sockfd;
	struct sockaddr_in client_addr;//variable for client
	sockfd = net_server_init();//initializing socket
	srand(time(NULL));//initializing clock
	printf("my id: %d \n",my_id);
	////////////////Handshake + Initialization//////////////////

	//////////////////// NOT the 0 node ////////////////////////
	if(iptab_get_ID_of( my_ip ) != 0){ 
	printf("I'm not the zero node\n");
	//HandShake1
	hsm=read_hsm(sockfd);
	if(hsh_check_availability(my_id, &hsm)!=1){
		perror( "not available" );
		exit(EXIT_FAILURE);
	}
	find_next(my_id,hsm,1); 
	//HandShake2
	hsm=read_hsm(sockfd); 
	hsh_update_iptab( &hsm );
	if(my_id<iptab_len()-1){      
		find_next(my_id,hsm,2);
	}
	printf("I finished the handshake step, his is the updated list of available nodes: \n");
	deb();
	init_general(my_id,sockfd);
	}
	/////////////////// the 0 node ///////////////////
	else{
		printf("I'm not the zero node\n");
		hsh_init(&hsm );
		if(hsh_check_availability(my_id, &hsm)!=1){
			perror( "not available" ); 
			exit(EXIT_FAILURE);
		}
		find_next( my_id,hsm,1);
		hsm=read_hsm(sockfd); 
		hsh_update_iptab( &hsm );
		//HandShake2
		find_next( my_id,hsm,2);
		deb();
		winner=init_master( my_id,sockfd);
		printf("The next turn leader: %d \n",winner);
		if(winner!=my_id){
			msg_set_ids( &msg3, my_id, winner);
			write_standard_msg(msg3, my_id, winner);
			flag_winner=0;
		}
		else{
			flag_winner = 1;
		}   
	}//end of the zero node.

	///////////////// Here starts the communication ring /////////////////////
		for (int l = 0; l < 10; l++){
		printf("Round N°: %d \n",l);
		if(flag_winner!=1){
			msg3=read_msg(sockfd);
			winner=msg_get_turnLeader( &msg3 );
			if(winner==my_id){
				printf("I? the new turn leader \n");
				flag_winner=1; //sono il turn leader
			}
			else {//questo è inutile
				printf("I'm not the new turn leader \n");
				flag_winner=0;
			}
		}
			switch(flag_winner){
				case 1:{//This is the case when this node is the turn leader
				//variables to store and compute bandwith
				long int band_tot,band_fly;
				long int comm_tot_T=0;
				long int comm_fly_T=0;
					for(int j=0;j<10;j++){//As a turn leader , I must perform 10 complete ring loop of communication 
					message_t msg2;
					node_id random_node;
					struct timeval sent_time,rcv,snt,recv_last;
					long int tot_time_receive,tot_time_sent;
					msg_init( &msg2 );
					msg_set_ids(&msg2, my_id, my_id);
					for(int i=0; i<iptab_len(); i++){//Marking all unavailable nodes as already visited.
						if(iptab_is_available(i)==0){
							msg_mark( &msg2, i);		
							}	
					}
					msg_mark( &msg2, my_id);//marking myself as visited
					//populating and sending the msg struct to a random node
					random_node=msg_rand(&msg2);      
					msg_set_sent( &msg2 );
					sent_time= msg_get_sent(  &msg2 );//starting time of communication
					write_standard_msg(msg2, my_id, random_node);
					//Initialization of var in order to keep track of duration of communication
					tot_time_receive=0;
					tot_time_sent=1000000*sent_time.tv_sec+sent_time.tv_usec;
					rcv.tv_sec=0;
					rcv.tv_usec=0;
					snt.tv_sec=0;
					snt.tv_usec=0;
					//reading all message for a single ring loop
					for(int u =0; u<iptab_len_av()-1; u++){           
						msg2=read_msg(sockfd);
						rcv=msg_get_recv(&msg2);
						snt=msg_get_sent(&msg2);
						tot_time_receive=tot_time_receive+1000000*rcv.tv_sec+rcv.tv_usec;
						tot_time_sent=tot_time_sent+1000000*snt.tv_sec+snt.tv_usec;
					}

					//Computing tot and fly times for a single ring loop
					msg_set_recv(&msg2); 
					recv_last= msg_get_recv(&msg2);//ending time of a single ring loop
					tot_time_receive=tot_time_receive+1000000*recv_last.tv_sec+recv_last.tv_usec;
					comm_tot_T=((1000000*recv_last.tv_sec+recv_last.tv_usec)-(1000000*sent_time.tv_sec+sent_time.tv_usec)+comm_tot_T);
					comm_fly_T=(tot_time_receive-tot_time_sent + comm_fly_T);

				} //End of 10xcomplete ring loop for a turn leader

				//computing avarages and sending to Professor zaccaria
				comm_tot_T=(comm_tot_T/10);
				comm_fly_T=(comm_fly_T/10);
				band_tot=(1000000*8*sizeof(msg3)*iptab_len_av())/comm_tot_T;
				band_fly=(1000000*8*sizeof(msg3)*iptab_len_av())/comm_fly_T;
				band_tot=1000000*band_tot; //conversione in bit/s
				band_fly=1000000*band_fly;
				printf("banda tot (bit/s): %ld banda fly(bit/s) : %ld\n",band_tot,band_fly);
				sent_RURZ(band_tot,band_fly);


				//Votation to elect a turn leader for the next turn(As a TL)
				if(l<9){
					winner=init_master( my_id,sockfd);
					printf("The next turn leader: %d \n",winner);

					if(winner!=my_id){
					message_t msg1;
					msg_set_ids( &msg1, my_id, winner); 
					write_standard_msg(msg1, my_id, winner);
					flag_winner=0;
					}
				}
			}
			break;

				case 0:{
				int random_node;
				msg_set_recv(&msg3);
				msg_mark(&msg3, my_id);

				if(msg_all_visited( &msg3 )==0){//not all nodes visited->send to next random node
					random_node=msg_rand(&msg3);    
					msg_set_sent( &msg3 ); 
					write_standard_msg(msg3, my_id, random_node); 
				}	
				else{	//all nodes visited->do not send to next random node
					msg_set_sent( &msg3 ); 
				}

				write_standard_msg(msg3, my_id, winner);//send back to the turn leader

					for(int i=0; i<9; i++){//code is the same as above , repeated for the remaining 9 messages
					msg3=read_msg(sockfd);
					msg_set_recv(&msg3);
					msg_mark(&msg3, my_id);
						if(msg_all_visited( &msg3 )==0){
						random_node=msg_rand(&msg3);    
						msg_set_sent( &msg3 ); 
						write_standard_msg(msg3, my_id, random_node); 
					}
					else{
						msg_set_sent( &msg3 ); 
					}

					write_standard_msg(msg3, my_id, winner);
				}

				//Votation to elect a turn leader for next turn (as a NTL node)
				if(l<9){
					init_general(my_id,sockfd);
				}
			}break;
		}//Here ends the switch
	}/////////////// Here ends the complet communication (i.e. 10 turn leaders in the net perform 10 complete ring loops each)//////////////

	deb(); //print of available nodes
	close(sockfd);

	return 0;
}

/////////////////////////////////////		FUNCTIONS		/////////////////////////////////
void find_next(node_id my_id, handshake_t hsm,int first){
	node_id next;
	char* ip_next;
	int fd_nodnext,w;
	switch(first){
	case 1:{
		next=my_id+1;
		if(next == iptab_len()){ 
			next = 0;
		}
		fd_nodnext=0;
		int counter=0;
		while( next>=iptab_len()||fd_nodnext==0){ 
			ip_next = iptab_getaddr(next);
			if (ip_next != NULL){
				struct timeval timeout;
				timeout.tv_sec=2;
				timeout.tv_usec=0;
				fd_nodnext = net_client_connection_timeout(ip_next,&timeout);
				if (fd_nodnext!=0){
					printf("mi sono connesso con il file descriptor: %d al nodo %d\n", fd_nodnext, next);
				}
			}
			next++;
			counter++;
			if(next == iptab_len()){ 
				next = 0;
			}

		}//fine while
	}
	break;//case1
	case 2:{
		next = iptab_get_next( my_id );
		ip_next = iptab_getaddr(next); 
		fd_nodnext = net_client_connection( ip_next );
		if( fd_nodnext < 0 ){
			perror( "connection " );
			exit(EXIT_FAILURE);
		}
	}
	break;//case2
	}//fine switch
	w = write(fd_nodnext,&hsm,sizeof(hsm));
	close( fd_nodnext );
	if( w < 0 ){
		perror( "error in writing hsm" );
		exit(EXIT_FAILURE);
	}      
}

handshake_t read_hsm(int sockfd){
	struct sockaddr_in client_addr;
	int fd_nodprec,r;
	handshake_t hsm;
	fd_nodprec = net_accept_client(sockfd,&client_addr);
	if( fd_nodprec < 0 ){
		perror( "connection" );
		exit(EXIT_FAILURE);
	}
	r = read(fd_nodprec,&hsm,sizeof(hsm));
	close(fd_nodprec);
	if( r < 0 ){
		perror( "error in reading hsm " );
		exit(EXIT_FAILURE);
	}
	return hsm;
}

node_id init_master(node_id my_id, int sockfd){
	node_id next;
	struct sockaddr_in client_addr;
	char* ip_next;
	int fd_nodnext,fd_nodprec,w,r;
	votation_t msg;
	vote_init( &msg );
	vote_do_votation( &msg );
	next= iptab_get_next( my_id );    
	ip_next = iptab_getaddr(next); 
	fd_nodnext = net_client_connection( ip_next );     
	w = write(fd_nodnext,&msg,sizeof(msg));
	close(fd_nodnext);
	if( w < 0 )
	{
		perror( "error in writing votation " );
		exit(EXIT_FAILURE);
	}
	fd_nodprec = net_accept_client(sockfd,&client_addr);
	if( fd_nodprec < 0 ){
		perror( "connection" );
		exit(EXIT_FAILURE);
	}
	r = read(fd_nodprec,&msg,sizeof(msg));
	close(fd_nodprec);
	if( r < 0 )
	{
		perror( "error in reading votation " );
		exit(EXIT_FAILURE);
	}
	node_id winner_from_functions;
	winner_from_functions = vote_getWinner( &msg );
	if( winner_from_functions==-1)
	{
		perror( "no winner" );
		exit(EXIT_FAILURE);
	}
	return winner_from_functions;
}

void init_general(node_id my_id,int sockfd){
	struct sockaddr_in client_addr;
	votation_t msgturn; 
	node_id next;
	char* ip_next;
	int fd_nodnext,fd_nodprec,w,r;
	fd_nodprec = net_accept_client(sockfd,&client_addr);
	if( fd_nodprec < 0 ){
		perror( "connection" );
		exit(EXIT_FAILURE);
	}
	r = read(fd_nodprec,&msgturn,sizeof(msgturn));
	close(fd_nodprec);
	if( r < 0 ){
		perror( "error in reading votation " );
		exit(EXIT_FAILURE);
	}
	vote_do_votation( &msgturn );
	next = iptab_get_next( my_id );
	ip_next = iptab_getaddr(next); 
	fd_nodnext = net_client_connection( ip_next );
	if( fd_nodnext < 0 ){
		perror( "connection" );
		exit(EXIT_FAILURE);
	}
	w = write(fd_nodnext,&msgturn,sizeof(msgturn));
	close( fd_nodnext );
	if( w < 0 ){
		perror( "error in writing votation " );
		exit(EXIT_FAILURE);
	}  
}

message_t read_msg(int sockfd){           
	message_t msg;
	int fdnew,r;
	struct sockaddr_in client_addr;
	fdnew = net_accept_client(sockfd,&client_addr);
	if( fdnew < 0 ){
		perror( "connection" );
		exit(EXIT_FAILURE);
	}
	r = read(fdnew,&msg,sizeof(msg));
	close(fdnew);
	if( r < 0 ){
		perror( "error in reading standard message " );
		exit(EXIT_FAILURE);
	}
	return msg;
}

void write_standard_msg(message_t msg, node_id my_id, node_id receiver){
	int fd,w;
	char* ip;
	ip= iptab_getaddr(receiver); 
	fd= net_client_connection( ip);
	if( fd< 0 ){
		perror( "connection std msg" );
		exit(EXIT_FAILURE);
	}     
	w = write(fd,&msg,sizeof(msg));
	close(fd);
	if(w<0){
		perror( "error in writing standard message" );
		exit(EXIT_FAILURE);
	}    
}

void sent_RURZ(float band_tot, float band_fly){
	int fd,w;
	stat_t *st_msg;
	char* RURZ_ip;
	stat_message_init(st_msg);
	stat_message_set_totBitrate(st_msg, band_tot);
	stat_message_set_flyBitrate(st_msg, band_fly);
	RURZ_ip = stat_get_RURZ_addr();
	fd = net_client_connection( RURZ_ip );      
	if( fd< 0 ){
		perror( "connection" );
		exit(EXIT_FAILURE);
	}
	w = write(fd,&st_msg,sizeof(st_msg));
	close(fd);
	if( w < 0 ){
		perror( "error in writing statistic message" );
		exit(EXIT_FAILURE);
	}
}

void deb(){
	printf("Available nodes %d\n",iptab_len_av());
	printf("1 for available, 0 for not available:\n");
	for (int a=0; a<iptab_len(); a++)
		{
		printf("Node: %d: %d\n", a, iptab_is_available(a));	
		}
}
