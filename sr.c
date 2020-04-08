#include "common.h"

pthread_t register_handler,alive_controller_thread,tcp_connections_thread;
volatile int register_handler_alive = 0,alive_controller_alive = 0,tcp_connections_alive = 0;
struct client clients[MAX_CLIENTS];
volatile int server_UDP_port = 0,server_TCP_port = 0;
volatile int server_UDP_socket;
char server_id[32];
int debug = 0;

struct PDU_UDP create_udp_packet(unsigned char tipus, char id[], char aleatori[], char dades[]){
    struct PDU_UDP packet;
    packet.tipus = tipus;
    strcpy(packet.id,id);
    strcpy(packet.aleatori,aleatori);
    strcpy(packet.dades,dades);
    return packet;
}

struct PDU_TCP create_tcp_packet(unsigned char tipus, char id[], char aleatori[], char element[], char valor[], char info[]){
    struct PDU_TCP packet;
    packet.tipus = tipus;
    strcpy(packet.id,id);
    strcpy(packet.aleatori,aleatori);
    strcpy(packet.element,element);
    strcpy(packet.valor,valor);
    strcpy(packet.info,info);
    return packet;
}

void print_debug(char msg[]){
    time_t timet = time(NULL);
    char *tlocal = ctime(&timet);
    tlocal[strlen(tlocal) - 1] = '\0';
    fflush(stdout);
    printf("%s: %s\n",tlocal,msg);
}

int is_REG_REQ_correct(struct PDU_UDP buff){
    /* -1 - No enviar res
        0 - Enviar REG_ACK
        1 - Enviar REG_NACK
        2 - Enviar REG_REJ
    */
    int i = 0;
    if(buff.tipus != REG_REQ){
        return -1;
    }
    for(i = 0; i < MAX_CLIENTS; i++){
        if(strcmp(buff.id,(char *) clients[i].id) == 0){
            break;
        }
        if(i == 15){
            return 2;
        }
    }
    if(clients[i].status != DISCONNECTED && clients[i].status != NOT_REGISTERED){
		if(debug == 1){
			print_debug("S'ha rebut [REG_REQ] en un estat que no era DISCONNECTED ni NOT_REGISTERED");
		}
        return 1;
    }
    if(strcmp(buff.aleatori,"00000000\0") != 0 || strcmp(buff.dades,"") != 0){
		if(debug == 1){
			print_debug("Les dades o l'aleatori del [REG_REQ] son incorrectes");
		}
        return 1;
    }else{
        return 0;
    }
}

void handle_cntrc(){
    print_debug("Sortint per ^C");
    register_handler_alive = 0;
    alive_controller_alive = 0;
    tcp_connections_alive = 0;
    exit(0);
}

void update_client(char affected_id[],int new_status){
	int i = 0;
	char debug_msg[128];
	fflush(stdout);
	for(i = 0;i < MAX_CLIENTS;i++){
		if(strcmp(clients[i].id,affected_id) == 0){
			if(new_status == DISCONNECTED){
				sprintf(debug_msg,"Nou estat del client %s: DISCONNECTED",(char *) affected_id);
			}else if (new_status == NOT_REGISTERED){
				sprintf(debug_msg,"Nou estat del client %s: NOT_REGISTERED",(char *) affected_id);
			}else if (new_status == WAIT_ACK_REG){
				sprintf(debug_msg,"Nou estat del client %s: WAIT_ACK_REG",(char *) affected_id);
			}else if (new_status == WAIT_INFO){
				sprintf(debug_msg,"Nou estat del client %s: WAIT_INFO",(char *) affected_id);
			}else if (new_status == WAIT_ACK_INFO){
				sprintf(debug_msg,"Nou estat del client %s: WAIT_ACK_INFO",(char *) affected_id);
			}else if (new_status == REGISTERED){
				sprintf(debug_msg,"Nou estat del client %s: REGISTERED",(char *) affected_id);
			}else if (new_status == SEND_ALIVE && clients[i].status != SEND_ALIVE){
				sprintf(debug_msg,"Nou estat del client %s: SEND_ALIVE",(char *) affected_id);
				print_debug(debug_msg);
			}
			if(new_status != SEND_ALIVE && debug == 1){
				print_debug(debug_msg);
			}
			clients[i].status = new_status;
		}
	}
}

void list(){
    int i = 0,j = 0;
    printf("**********DADES DISPOSITIUS**********\n");
    printf("Id\t\tStatus\t\tDispositius\n");
    for(i = 0; i < MAX_CLIENTS;i++){
		if(strcmp(clients[i].id,"\0") != 0){
			if(clients[i].status == DISCONNECTED){
				printf("%s\tDISCONNECTED\t",(char *) clients[i].id);
			}else if (clients[i].status == NOT_REGISTERED){
				printf("%s\tNOT_REGISTERED\t",(char *) clients[i].id);
			}else if (clients[i].status == WAIT_ACK_REG){
				printf("%s\tWAIT_ACK_REG\t",(char *) clients[i].id);
			}else if (clients[i].status == WAIT_INFO){
				printf("%s\tWAIT_INFO\t",(char *) clients[i].id);
			}else if (clients[i].status == WAIT_ACK_INFO){
				printf("%s\tWAIT_ACK_INFO\t",(char *) clients[i].id);
			}else if (clients[i].status == REGISTERED){
				printf("%s\tREGISTERED\t",(char *) clients[i].id);
			}else if (clients[i].status == SEND_ALIVE){
				printf("%s\tSEND_ALIVE\t",(char *) clients[i].id);
			}
			for(j = 0; j < MAX_DISPS;j++){
				if(strcmp(clients[i].dispositius[j],"\0") == 0){
					break;
				}
				printf("%s;",(char *) clients[i].dispositius[j]);
			}
			printf("\n");
		}
    }
    printf("**************************************\n");
}

int get_client_udp_port(char id[]){
	int i = 0;
	for(i = 0;i < MAX_CLIENTS;i++){
		if(strcmp(clients[i].id,id) == 0){
			return clients[i].new_udp_port;
		}
	}
	return 0;
}

int get_client_status(char id[]){
	int i = 0;
	for(i = 0;i < MAX_CLIENTS;i++){
		if(strcmp(clients[i].id,id) == 0){
			return clients[i].status;
		}
	}
	return 0;
}

void set_client_address(char affected_id[], struct sockaddr_in cl_addrs){
	int i = 0;
	for(i = 0;i < MAX_CLIENTS;i++){
		if(strcmp(clients[i].id,affected_id) == 0){
			clients[i].addr_UDP = cl_addrs;
		}
	}
}

void set_client_random(char affected_id[], int random){
	int i = 0;
	for(i = 0;i < MAX_CLIENTS;i++){
		if(strcmp(clients[i].id,affected_id) == 0){
			clients[i].random = random;
		}
	}
}

void set_client_udp_port(char affected_id[], int udp_port){
	int i = 0;
	for(i = 0;i < MAX_CLIENTS;i++){
		if(strcmp(clients[i].id,affected_id) == 0){
			clients[i].new_udp_port = udp_port;
		}
	}
}

int is_ALIVE_correct(struct PDU_UDP buff){
	int i = 0;
    if(buff.tipus != ALIVE){
        return -1;
    }
    for(i = 0; i < MAX_CLIENTS; i++){
        if(strcmp(buff.id,(char *) clients[i].id) == 0){
            break;
        }
        if(i == (MAX_CLIENTS - 1)){
            return -1;
        }
    }
    if(clients[i].status != REGISTERED && clients[i].status != SEND_ALIVE){
		if(debug == 1){
			print_debug("S'ha rebut [ALIVE] en un estat que no era REGISTERED ni SEND_ALIVE");
		}
        return -1;
    }
    if(atoi(buff.aleatori) != clients[i].random || strcmp(buff.dades,"") != 0){
		if(debug == 1){
			print_debug("Les dades o l'aleatori del [ALIVE] son incorrectes");
		}
        return -1;
    }else{
        return 0;
    }
}

void set_client_alive(char id[]){
	int i = 0;
	for(i = 0;i < MAX_CLIENTS;i++){
		if(strcmp(clients[i].id,id) == 0){
			clients[i].alive_recved = 1;
			clients[i].alives_no_answer = 0;
		}
	}
}

void set_TCP_port(char id[], char port[]){
	int i = 0;
	for(i = 0;i < MAX_CLIENTS;i++){
		if(strcmp(clients[i].id,id) == 0){
			clients[i].TCP_port = atoi(port);
		}
	}
}

void set_TCP_addr(char id[], struct sockaddr_in addr){
	int i = 0;
	for(i = 0;i < MAX_CLIENTS;i++){
		if(strcmp(clients[i].id,id) == 0){
			clients[i].addr_TCP = addr;
		}
	}
}

void add_dispositiu(char id[], char dispositiu[]){
	int i = 0, j = 0;
	for(i = 0;i < MAX_CLIENTS;i++){
		if(strcmp(clients[i].id,id) == 0){
			j = 0;
			for(j = 0;j < MAX_DISPS;j++){
			    if(strcmp(clients[i].dispositius[j],dispositiu) == 0){
			        break;
			    }
				if(strcmp(clients[i].dispositius[j],"\0") == 0){
					strcpy(clients[i].dispositius[j],dispositiu);
					break;
				}
			}	
		}
	}
}

void *client_manager(void *argvs){
	struct sockaddr_in serv_new_addrs,cl_addrs,cl_tcp;
	int rand,new_UDP_port,recved,new_UDP_socket,retl;
	char str_rand[9],str_new_UDP_port[5],str_TCP_port[5],debug_msg[128],info_split[128];
	char *ptr,*ptr2;
	struct PDU_UDP buffer,buffer2,REG_ACK_packet,REG_NACK_packet,REG_REJ_packet,ALIVE_packet, INFO_packet;
	fd_set selectset;
	struct timeval tv;
	int len = sizeof(cl_addrs);
	int server_UDP_socket = *((int *) argvs);
	
	recvfrom(server_UDP_socket,&buffer,84, MSG_WAITALL,(struct sockaddr *) &cl_addrs, (socklen_t *) &len);
	if(debug == 1){
		print_debug("S'ha rebut un paquet pel port UDP principal");
	}
	
	/*Mirar si es REG_REQ o ALIVE
	 * si es REG_REQ igual
	 * si es ALIVE contestar*/
	if((recved = is_REG_REQ_correct(buffer)) == 0){
		if(debug == 1){
			print_debug("El paquet REG_REQ es correcte");
		}
		buffer.id[12] = '\0';
		if(debug == 1){
			print_debug("Client envia REG_REQ, passa a WAIT_ACK_REG");
		}
		update_client(buffer.id,WAIT_ACK_REG);
		set_client_address(buffer.id,cl_addrs);
		
		rand = generate_random();
		new_UDP_port = generate_UDP_port();
		set_client_random(buffer.id,rand);
		set_client_udp_port(buffer.id,new_UDP_port);
		
		sprintf((char *) str_rand,"%i",rand);
		str_rand[8] = '\0';
		
		memset(&serv_new_addrs,0,sizeof(struct sockaddr_in));
		    
		serv_new_addrs.sin_family = AF_INET;
		serv_new_addrs.sin_port = htons(new_UDP_port);
		serv_new_addrs.sin_addr.s_addr = INADDR_ANY;
		
		new_UDP_socket = socket(AF_INET,SOCK_DGRAM,0);
		
		while(bind(new_UDP_socket,(const struct sockaddr *)&serv_new_addrs,sizeof(serv_new_addrs))<0){
			print_debug("ERROR => No s'ha pogut bindejar el socket");
			new_UDP_port = generate_UDP_port();
			serv_new_addrs.sin_port = htons(new_UDP_port);
		}
		if (debug == 1){
			print_debug("Nou socket bindejat correctament");
		}
		
		sprintf((char *) str_new_UDP_port,"%i",new_UDP_port);
		str_new_UDP_port[4] = '\0';
		
		REG_ACK_packet = create_udp_packet(REG_ACK,server_id,str_rand,str_new_UDP_port);
		
		/*Envia REG_ACK*/
		sendto(server_UDP_socket,(struct PDU_UDP *) &REG_ACK_packet,84,MSG_CONFIRM,(struct sockaddr *) &cl_addrs, len);
			
		FD_ZERO(&selectset);
		FD_SET(new_UDP_socket,&selectset);
	
		tv.tv_sec = 2;
		tv.tv_usec = 0;
		retl = select(new_UDP_socket+1,&selectset,NULL,NULL,(struct timeval *) &tv);
		if(retl){
			if(FD_ISSET(new_UDP_socket,&selectset)){
				fflush(stdout);
				recvfrom(new_UDP_socket,&buffer2,84, MSG_WAITALL,(struct sockaddr *) &cl_addrs, (socklen_t *) &len);
				/*Rebut paquet REG_INFO, contestar amb el paquet que toqui*/
				if(buffer2.tipus != REG_INFO){
					if (debug == 1){
						print_debug("Tipus de paquet no esperat, no es contestarà");
					}
					update_client(buffer2.id,DISCONNECTED);
				}else{
					if(rand == atoi(buffer2.aleatori) && strcmp(buffer.id,buffer2.id) == 0){
						
						strcpy(info_split,buffer2.dades);
						
						ptr = strtok(info_split, ",");
						
						set_TCP_port(buffer2.id,ptr);
						
						cl_tcp.sin_family = AF_INET;
						cl_tcp.sin_port = htons(atoi(ptr));
						cl_tcp.sin_addr.s_addr = cl_addrs.sin_addr.s_addr;
		
						set_TCP_addr(buffer2.id,cl_tcp);
		
						ptr = strtok(NULL, ",");
						
						ptr2 = strtok(ptr, ";");

						while (ptr2 != NULL)
						{
							add_dispositiu(buffer2.id,ptr2);
							ptr2 = strtok(NULL, ";");
						}
						
						sprintf(str_TCP_port,"%i",server_TCP_port);
						str_TCP_port[4] = '\0';
						sprintf((char *) str_rand,"%i",rand);
						str_rand[8] = '\0';
						INFO_packet = create_udp_packet(INFO_ACK,server_id,str_rand,str_TCP_port);
						sendto(new_UDP_socket,(struct PDU_UDP *) &INFO_packet,84,MSG_CONFIRM,(struct sockaddr *) &cl_addrs, len);
						/*Paquet REG_INFO correcte, llegir dades i contestar amb INFO_ACK*/
						update_client(buffer2.id,REGISTERED);
						sleep(3);
						if(get_client_status(buffer2.id) == REGISTERED){ /* El client no ha enviat el primer alive */
							sprintf(debug_msg,"El client %s no ha enviat el 1r [ALIVE]",(char *) buffer2.id);
							print_debug(debug_msg);
							update_client(buffer2.id,DISCONNECTED);
						}
					}else{
						sprintf((char *) str_rand,"%i",rand);
						str_rand[8] = '\0';
						INFO_packet = create_udp_packet(INFO_ACK,server_id,str_rand,"Info no acceptada");
						/*Paquet REG_INFO incorrecte, contestar amb INFO_NACK*/
					}
				}
			}
		}else{
			sprintf(debug_msg,"El client %s no ha contestat el REG_ACK",buffer.id);
			print_debug(debug_msg);
			update_client(buffer.id,DISCONNECTED);
		}
	}else if(recved == 1){
		REG_NACK_packet = create_udp_packet(REG_NACK,server_id,"00000000","Alguna cosa no quadra entre estats o dades");
		sendto(server_UDP_socket,(struct PDU_UDP *) &REG_NACK_packet,84,MSG_CONFIRM,(struct sockaddr *) &cl_addrs, len);
		update_client(buffer.id,DISCONNECTED);
	}else if(recved == 2){
		REG_REJ_packet = create_udp_packet(REG_REJ,server_id,"00000000","Error d'identificació");
		sendto(server_UDP_socket,(struct PDU_UDP *) &REG_REJ_packet,84,MSG_CONFIRM,(struct sockaddr *) &cl_addrs, len);
		update_client(buffer.id,DISCONNECTED);
	}else{ /* El paquet és un ALIVE */
		if((recved = is_ALIVE_correct(buffer)) == 0){
			set_client_alive(buffer.id);
			ALIVE_packet = create_udp_packet(ALIVE,server_id,buffer.aleatori,buffer.id);
			update_client(buffer.id,SEND_ALIVE);
			if(debug == 1){
				sprintf(debug_msg,"Enviant paquet [ALIVE] a %s",(char *) buffer.id); 
				print_debug(debug_msg);
			}
			sendto(server_UDP_socket,(struct PDU_UDP *) &ALIVE_packet,84,MSG_CONFIRM,(struct sockaddr *) &cl_addrs, len);
		}else if (recved == -1){
			ALIVE_packet = create_udp_packet(ALIVE_REJ,server_id,buffer.aleatori,buffer.id);
			update_client(buffer.id,SEND_ALIVE);
			sendto(server_UDP_socket,(struct PDU_UDP *) &ALIVE_packet,84,MSG_CONFIRM,(struct sockaddr *) &cl_addrs, len);
		}
	}
	return NULL;
}

void *alive_controller(void *argvs){
	int i = 0;
	char str_rand[9],debug_msg[128];
	struct PDU_UDP ALIVE_packet;
	int len = sizeof(struct sockaddr_in);
	while(alive_controller_alive != 0){
		i = 0;
		sleep(2);
		for (i = 0;i < MAX_CLIENTS; i++){
			if(clients[i].status == SEND_ALIVE){
				if(clients[i].alives_no_answer == 3){
					sprintf(debug_msg,"El client %s ha deixat d'enviar alives",(char *) clients[i].id);
					print_debug(debug_msg);
					update_client(clients[i].id,DISCONNECTED);
				}
				if(clients[i].alive_recved == 0){
					sprintf((char *) str_rand,"%i",clients[i].random);
					str_rand[8] = '\0';
					ALIVE_packet = create_udp_packet(ALIVE,server_id,str_rand,clients[i].id);
					if(debug == 1){
						sprintf(debug_msg,"Enviant paquet [ALIVE] a %s",(char *) clients[i].id); 
						print_debug(debug_msg);
					}
					sendto(server_UDP_socket,(struct PDU_UDP *) &ALIVE_packet,84,MSG_CONFIRM,(struct sockaddr *) &clients[i].addr_UDP, len);
					clients[i].alives_no_answer++;
				}else{
					clients[i].alive_recved = 0;
				}
			}
		}
	}
	return NULL;
}

void *register_handler_fun(void *argvs){ /*Bindeja socket 1 i crea thread al rebre paquets REG_REQ*/
    struct sockaddr_in serv_addrs;
    pthread_t client_manager_thread;
	fd_set selectset;
    int retl;
	
    memset(&serv_addrs,0,sizeof(struct sockaddr_in));

    serv_addrs.sin_family = AF_INET;
    serv_addrs.sin_port = htons(server_UDP_port);
    serv_addrs.sin_addr.s_addr = INADDR_ANY;

    server_UDP_socket = socket(AF_INET,SOCK_DGRAM,0);

    if(bind(server_UDP_socket,(const struct sockaddr *)&serv_addrs,sizeof(serv_addrs))<0){
        print_debug("ERROR => No s'ha pogut bindejar el socket");
        exit(-1);
    }
    if(debug == 1){
		print_debug("Socket bindejat correctament");
	}
    
    while(register_handler_alive == 1){
		FD_ZERO(&selectset);
		FD_SET(server_UDP_socket,&selectset);
		retl = select(server_UDP_socket+1,&selectset,NULL,NULL,0);
		if(retl){
			if(FD_ISSET(server_UDP_socket,&selectset)){
				if(debug == 1){
					print_debug("Creant thread per a rebre un paquet UDP");
				}
				pthread_create(&client_manager_thread,NULL,client_manager,(void *) &server_UDP_socket);
				sleep(0.1);
			}
		}
    }
    return NULL;
}

int correct_id(char id[],int aleatori){
	/* 0 = error
	 * 1 = correcte */
	int i = 0;
	for (i = 0; i < MAX_CLIENTS; i++){
		if(strcmp(clients[i].id,id) == 0){
			if(aleatori == clients[i].random){
				if(clients[i].status == SEND_ALIVE){
					return 1;
				}else{
					return 0;
				}
			}else{
				return 0;
			}
		}
	}
	return 0;
}

int element_in_client(char id[], char element[]){
	int i = 0, j = 0;
	for(i = 0;i < MAX_CLIENTS;i++){
		if(strcmp(clients[i].id,id) == 0){
			j = 0;
			for(j = 0;j < MAX_DISPS;j++){
			    if(strcmp(clients[i].dispositius[j],element) == 0){
			        return 1;
			    }
			}
			return 0;	
		}
	}
	return 0;
}

int is_SEND_DATA_correct(struct PDU_TCP buffer){
	/* -1 = No contestar
	 *  0 = DATA_ACK
	 *  1 = DATA_NACK fitxer
	 *  2 = DATA_REJ
	 *  3 = DATA_NACK dades
	 * */
	FILE *logfile;
	char filename[18];
	char res_str[128];
	time_t timet;
	char *tlocal;
	int putsr;
	
	if(buffer.tipus != SEND_DATA){
		return -1;
	}
	
	if(correct_id(buffer.id,atoi(buffer.aleatori)) == 0){
		update_client(buffer.id,DISCONNECTED);
		return 2;
	}else{
		sprintf(filename,"%s.data",(char *) buffer.id);
		if(element_in_client(buffer.id,buffer.element) == 0){
			return 3;
		}
		logfile = fopen(filename,"a");
		timet = time(NULL);
		tlocal = ctime(&timet);
		tlocal[strlen(tlocal) - 1] = '\0';
		fflush(stdout);
		sprintf(res_str,"%s;SEND_DATA;%s;%s\n",tlocal,buffer.element,buffer.valor);
		putsr = fputs(res_str,logfile);
		if (putsr == EOF){
			return 1;
		}
		putsr = fclose(logfile);
		if (putsr == EOF){
			return 1;
		}
		return 0;
	}
}

void *tcp_man(void *argvs){
	int csocket;
	struct PDU_TCP RECV_packet, SEND_packet;
	int recved;
	
	csocket = *((int *) argvs);
	
	recv(csocket,(struct PDU_UDP *) &RECV_packet,sizeof(struct PDU_TCP),0);
	
	if((recved = is_SEND_DATA_correct(RECV_packet)) == 0){
		SEND_packet = create_tcp_packet(DATA_ACK,server_id,RECV_packet.aleatori,RECV_packet.element,RECV_packet.valor,RECV_packet.id);
	}else if(recved == -1){
		close(csocket);
	}else if(recved == 1){
		SEND_packet = create_tcp_packet(DATA_NACK,server_id,RECV_packet.aleatori,RECV_packet.element,RECV_packet.valor,"Hi ha hagut un error amb el fitxer");
	}else if(recved == 2){
		SEND_packet = create_tcp_packet(DATA_REJ,server_id,RECV_packet.aleatori,RECV_packet.element,RECV_packet.valor,"Error d'identificació");
	}else if(recved == 3){
		SEND_packet = create_tcp_packet(DATA_NACK,server_id,RECV_packet.aleatori,RECV_packet.element,RECV_packet.valor,"Hi ha hagut un error amb els dispositius");
	}
	if(recved != -1){
		send(csocket,&SEND_packet,sizeof(struct PDU_TCP),0);
		close(csocket);
	}
	return NULL;
}

void *tcp_connections(void *argvs){
	int TCP_socket,new_socket,retl,len;
	struct sockaddr_in serv_addrs,cl_addrs;
	pthread_t pack_manager;
	fd_set selectset;
	
	len = sizeof(serv_addrs);
	
	TCP_socket = socket(AF_INET,SOCK_STREAM,0);
	
	serv_addrs.sin_family = AF_INET;
    serv_addrs.sin_port = htons(server_TCP_port);
    serv_addrs.sin_addr.s_addr = INADDR_ANY;
    
	if(bind(TCP_socket,(const struct sockaddr *)&serv_addrs,(socklen_t) len)<0){
        print_debug("ERROR => No s'ha pogut bindejar el socket TCP");
        exit(-1);
    }
    if(debug == 1){
		print_debug("Socket TCP bindejat correctament");
	}
	
	listen(TCP_socket,5);
	
	while(tcp_connections_alive == 1){
		FD_ZERO(&selectset);
		FD_SET(TCP_socket,&selectset);
		retl = select(TCP_socket+1,&selectset,NULL,NULL,0);
		if(retl){
			new_socket = accept(TCP_socket,(struct sockaddr *) &cl_addrs,(socklen_t * ) &len);
			if (debug == 1){
				print_debug("S'ha rebut una connexió pel port TCP");
			}
			pthread_create(&pack_manager,NULL,tcp_man,(void *) &new_socket);
		}
	}
	return NULL;
}

int set(char clid[],char elem[],char val[]){
	struct PDU_TCP SEND_pack,buffer;
	int tcp_sock;
	int i,retl;
	char str_rand[9];
	struct timeval tv;
	fd_set selectset;
	FILE *logfile;
	char filename[128];
	char res_str[128];
	time_t timet;
	char *tlocal;
	if(strcmp(clid,"") == 0 || strcmp(elem,"") == 0 || strcmp(val,"") == 0){
		print_debug("Comanda errònea. Ús: set <id_client> <element> <valor>");
	}else{
		fflush(stdout);
		tcp_sock = socket(AF_INET,SOCK_STREAM,0);
		i = 0;
		for(i = 0; i < MAX_CLIENTS;i++){
			if(strcmp(clients[i].id,clid) == 0){
				sprintf(str_rand,"%i",clients[i].random);
				SEND_pack = create_tcp_packet(SET_DATA,server_id,str_rand,elem,val,clid);
				connect(tcp_sock,(struct sockaddr *) &clients[i].addr_TCP,sizeof(struct sockaddr_in));
				send(tcp_sock,(struct PDU_TCP *) &SEND_pack,sizeof(struct PDU_TCP),0);
				FD_ZERO(&selectset);
				FD_SET(tcp_sock,&selectset);
				tv.tv_sec = 3;
				tv.tv_usec = 0;
				retl = select(tcp_sock+1,&selectset,NULL,NULL,(struct timeval *) &tv);
				if(retl){
					recv(tcp_sock,&buffer,sizeof(struct PDU_TCP),0);
					if(buffer.tipus == DATA_REJ){
						print_debug("S'han rebutjat les dades");
					}else if(buffer.tipus == DATA_NACK){
						print_debug("No s'han pogut guardar les dades");
						print_debug(buffer.info);
					}else if(buffer.tipus == DATA_ACK){
						print_debug("S'han acceptat les dades");
						sprintf(filename,"%s.data",(char *) buffer.id);
						logfile = fopen(filename,"a");
						timet = time(NULL);
						tlocal = ctime(&timet);
						tlocal[strlen(tlocal) - 1] = '\0';
						fflush(stdout);
						sprintf(res_str,"%s;SET_DATA;%s;%s\n",tlocal,buffer.element,buffer.valor);
						fputs(res_str,logfile);
						fclose(logfile);
						close(tcp_sock);
						return 0;
					}else{
						print_debug("Paquet no esperat");
					}
				}else{
					print_debug("El client no ha contestat");
				}
			}
		}
	}
	close(tcp_sock);	
	return -1;
}

int get(char clid[],char elem[]){
	struct PDU_TCP SEND_pack,buffer;
	int tcp_sock;
	int i,retl;
	char str_rand[9];
	struct timeval tv;
	fd_set selectset;
	FILE *logfile;
	char filename[128];
	char res_str[128];
	time_t timet;
	char *tlocal;
	if(strcmp(clid,"") == 0 || strcmp(elem,"") == 0){
		print_debug("Comanda errònea. Ús: get <id_client> <element>");
	}else{
		fflush(stdout);
		tcp_sock = socket(AF_INET,SOCK_STREAM,0);
		i = 0;
		for(i = 0; i < MAX_CLIENTS;i++){
			if(strcmp(clients[i].id,clid) == 0){
				sprintf(str_rand,"%i",clients[i].random);
				SEND_pack = create_tcp_packet(GET_DATA,server_id,str_rand,elem,"",clid);
				connect(tcp_sock,(struct sockaddr *) &clients[i].addr_TCP,sizeof(struct sockaddr_in));
				send(tcp_sock,(struct PDU_TCP *) &SEND_pack,sizeof(struct PDU_TCP),0);
				FD_ZERO(&selectset);
				FD_SET(tcp_sock,&selectset);
				tv.tv_sec = 3;
				tv.tv_usec = 0;
				retl = select(tcp_sock+1,&selectset,NULL,NULL,(struct timeval *) &tv);
				if(retl){
					recv(tcp_sock,&buffer,sizeof(struct PDU_TCP),0);
					if(buffer.tipus == DATA_REJ){
						print_debug("S'han rebutjat les dades");
					}else if(buffer.tipus == DATA_NACK){
						print_debug("No s'han pogut guardar les dades");
					}else if(buffer.tipus == DATA_ACK){
						print_debug("S'han acceptat les dades");
						sprintf(filename,"%s.data",(char *) buffer.id);
						logfile = fopen(filename,"a");
						timet = time(NULL);
						tlocal = ctime(&timet);
						tlocal[strlen(tlocal) - 1] = '\0';
						fflush(stdout);
						sprintf(res_str,"%s;GET_DATA;%s;%s\n",tlocal,buffer.element,buffer.valor);
						fputs(res_str,logfile);
						fclose(logfile);
						close(tcp_sock);
						return 0;
					}else{
						print_debug("Paquet no esperat");
					}
				}else{
					print_debug("El client no ha contestat");
				}
			}
		}
	}
	close(tcp_sock);	
	return -1;
}

void ajuda(){
	printf("*************** AJUDA **************\n");
	printf("Comanda \t\tÚs \t\tFunció\n");
	printf("set \t\tset <id_client> <dispositiu> <valor>\tEnvia el valor entrat al dispositiu del client\n");
	printf("get \t\tget <id_client> <dispositiu>\tRep el valor del dispositiu del client\n");
	printf("list \t\tlist \tMostra els clients acceptats amb els seus dispositius\n");
	printf("quit \t\tquit \tTanca el servidor\n");
	printf("debug \t\tdebug \tActiva o desactiva el mode debug\n");
	printf("ajuda \t\t? \tMostra aquesta ajuda\n");
	printf("*************************************\n");
}

void quit(){
    register_handler_alive = 0;
    alive_controller_alive = 0;
    tcp_connections_alive = 0;
    exit(0);
}

int main(int argc,char *argv[]){
    FILE *cfg_file,*dat_file;
    int i,j,operation_result;
    char filename[64] = "",datab_name[64] = "";
    char server_UDP_port_read[16],server_UDP_port_arr[4];
    char server_TCP_port_read[16],server_TCP_port_arr[4];
    char buff_comm[255];
    char params[4][255];
    char *ptr;
    for(i = 1; i < argc;i++){
        if(strcmp(argv[i],"-c") == 0){
            if((i+1) < argc && strlen(argv[i+1]) <= 64){
                strcpy(filename,argv[i+1]);
                i++;
            }else{
                printf("Ús: ./sr {-c <nom_fitxer>} {-d} {-u <nom_fitxer>}\n");
            }
        }else if(strcmp(argv[i],"-u") == 0){
            if((i+1) < argc && strlen(argv[i+1]) <= 64){
                strcpy(datab_name,argv[i+1]);
                i++;
            }else{
                printf("Ús: ./sr {-c <nom_fitxer>} {-d} {-u <nom_fitxer>}\n");
            }
        }else if(strcmp(argv[i],"-d") == 0){
            debug = 1;
        }else{
            printf("Ús: ./sr {-c <nom_fitxer>} {-d} {-u <nom_fitxer>}\n");
        }
    }
    if(strcmp(filename,"") == 0){
        strcpy(filename,"server.cfg");
    }
    if(strcmp(datab_name,"") == 0){
        strcpy(datab_name,"bbdd_dev.dat");
    }
    if(debug == 1){
        print_debug("Llegint fitxers de configuració");
    }
    cfg_file = fopen(filename,"r");
    fgets(server_id,32,cfg_file);
    server_id[strlen(server_id) - 1] = '\0';
    for(j = 0; j < 5;j++){
        for(i = 1; i < strlen(server_id);i++){
            server_id[i-1] = server_id[i];
            server_id[i] = '_';
        }
    }
    for(i = 0; i < strlen(server_id);i++){
        if(server_id[i] == '_'){
            server_id[i] = '\0';
        }
    }
    fgets(server_UDP_port_read,32,cfg_file);
    j = 0;
    for(i = 0; i < strlen(server_UDP_port_read);i++){
        if(isdigit(server_UDP_port_read[i])){
            server_UDP_port_arr[j] = server_UDP_port_read[i];
            j++;
        }
    }
    server_UDP_port = atoi(server_UDP_port_arr);
    fgets(server_TCP_port_read,32,cfg_file);
    j = 0;
    for(i = 0; i < strlen(server_TCP_port_read);i++){
        if(isdigit(server_TCP_port_read[i])){
            server_TCP_port_arr[j] = server_TCP_port_read[i];
            j++;
        }
    }
    server_TCP_port = atoi(server_TCP_port_arr);
    if(fclose(cfg_file) != 0){
        if(debug == 1){
            print_debug("Hi ha hagut un error. Sortint");
        }
        exit(-1);
    }
    dat_file = fopen(datab_name,"r");
    i = 0;
    while(i < MAX_CLIENTS){
        fgets(clients[i].id,32,dat_file);
        clients[i].id[12] = '\0';
        clients[i].status = DISCONNECTED;
        j = 0;
        for (j = 0; j < MAX_DISPS;j++){
			strcpy(clients[i].dispositius[j],"\0");
		}
        clients[i].alive_recved = 0;
        clients[i].alives_no_answer = 0;
        i++;
    }
    if(fclose(dat_file) != 0){
        if(debug == 1){
            print_debug("Hi ha hagut un error. Sortint");
        }
        exit(-1);
    }
    if(debug == 1){
        print_debug("Lectures inicials finalitzades");
    }
    /*  server_id = id servidor
        server_TCP_port = port TCP servidor
        server_UDP_port = port UDP servidor
        debug = cal fer debug o no
        allowed_disps = ids aceptades de clients
    */
    register_handler_alive = 1;
    alive_controller_alive = 1;
    tcp_connections_alive = 1;
    pthread_create(&register_handler,NULL,register_handler_fun,NULL);
    pthread_create(&alive_controller_thread,NULL,alive_controller,NULL);
    pthread_create(&tcp_connections_thread,NULL,tcp_connections,NULL);
    signal(SIGINT,handle_cntrc);
	fflush(stdout);
    while(0 < 1){
		i = 0;
		while (i < 4){
			strcpy(params[i],"");
			i++;
		}
		fflush(stdout);
		fgets(buff_comm, 255, stdin);
		buff_comm[strlen(buff_comm) - 1] = '\0';
		ptr = strtok(buff_comm, " ");
		i = 0;
		while(i < 4 && ptr != NULL){
			strcpy(params[i],ptr);
			ptr = strtok(NULL, " ");
			i++;
		}
		if (strcmp(params[0],"set") == 0){
			operation_result = set(params[1],params[2],params[3]);
			if(operation_result >= 0){
				print_debug("Operació exitosa");
			}else{
				print_debug("Operació fallida");
			}
		}else if(strcmp(params[0], "get") == 0){
			operation_result = get(params[1],params[2]);
			if(operation_result >= 0){
				print_debug("Operació exitosa");
			}else{
				print_debug("Operació fallida");
			}
		}else if(strcmp(params[0],"list") == 0){
			list();
		}else if(strcmp(params[0],"quit") == 0){
			quit();
		}else if(strcmp(params[0],"debug") == 0){
			if(debug == 0){
				print_debug("Mode debug activat");
				debug = 1;
			}else{
				debug = 0;
				print_debug("Mode debug desactivat");
			}
		}else if(strcmp(params[0],"?") == 0){
			ajuda();
		}else{
			print_debug("Comanda errònea");
		}
    }
    exit(0);
}
