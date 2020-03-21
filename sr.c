#include "common.h"

pthread_t register_handler;
volatile int register_handler_alive = 0;
volatile int client_manager_alive[16];
struct client clients[16];
volatile int server_UDP_port = 0,server_TCP_port = 0;
char server_id[32];

struct PDU_UDP create_udp_packet(unsigned char tipus, char id[], char aleatori[], char dades[]){
    struct PDU_UDP packet;
    packet.tipus = tipus;
    strcpy(packet.id,id);
    strcpy(packet.aleatori,aleatori);
    strcpy(packet.dades,dades);
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
    for(i = 0; i < 16; i++){
        if(strcmp(buff.id,(char *) clients[i].id) == 0){
            break;
        }
        if(i == 15){
            return 2;
        }
    }
    if(clients[i].status != DISCONNECTED && clients[i].status != NOT_REGISTERED){
		print_debug("S'ha rebut [REG_REQ] en un estat que no era DISCONNECTED ni NOT_REGISTERED");
        return 1;
    }
    if(strcmp(buff.aleatori,"00000000\0") != 0 || strcmp(buff.dades,"") != 0){
		print_debug("Les dades o l'aleatori del [REG_REQ] son incorrectes");
        return 1;
    }else{
        return 0;
    }
}

void handle_cntrc(){
    print_debug("Sortint per ^C");
    register_handler_alive = 0;
    exit(0);
}

void update_client(char affected_id[],int new_status){
    /*
    setbuf(stdout,NULL);
	fflush(stdout);
	*/
	int i = 0;
	for(i = 0;i < 16;i++){
		if(strcmp(clients[i].id,affected_id) == 0){
			clients[i].status = new_status;
		}
	}
}

void list(){
    int i = 0,j = 0;
    printf("**********DADES DISPOSITIUS**********\n");
    printf("Id\t\tStatus\t\tDispositius\n");
    for(i = 0; i < 15;i++){
		if(strcmp(clients[i].id,"\0") != 0){
			printf("%s\t%i\t\t",(char *) clients[i].id,clients[i].status);
			/*Fer que printi text enlloc de número*/
			for(j = 0; j < 15;j++){
				if(strcmp(clients[i].dispositius[j],"\0") == 0){
					break;
				}
				printf("%s;",(char *) clients[i].dispositius[j]);
			}
			printf("\n");
		}
    }
}

int get_client_udp_port(char id[]){
	int i = 0;
	for(i = 0;i < 16;i++){
		if(strcmp(clients[i].id,id) == 0){
			return clients[i].new_udp_port;
		}
	}
	return 0;
}

int get_client_status(char id[]){
	int i = 0;
	for(i = 0;i < 16;i++){
		if(strcmp(clients[i].id,id) == 0){
			return clients[i].status;
		}
	}
	return 0;
}

void set_client_address(char affected_id[], struct sockaddr_in cl_addrs){
	int i = 0;
	for(i = 0;i < 16;i++){
		if(strcmp(clients[i].id,affected_id) == 0){
			clients[i].addr_UDP = cl_addrs;
		}
	}
}

void set_client_random(char affected_id[], int random){
	int i = 0;
	for(i = 0;i < 16;i++){
		if(strcmp(clients[i].id,affected_id) == 0){
			clients[i].random = random;
		}
	}
}

void set_client_udp_port(char affected_id[], int udp_port){
	int i = 0;
	for(i = 0;i < 16;i++){
		if(strcmp(clients[i].id,affected_id) == 0){
			clients[i].new_udp_port = udp_port;
		}
	}
}

struct client managing_client(char id[]){
	int i = 0;
	for(i = 0;i < 16;i++){
		if(strcmp(clients[i].id,id) == 0){
			return clients[i];
		}
	}
	return clients[0];
}

int is_ALIVE_correct(struct PDU_UDP buff){
	int i = 0;
    if(buff.tipus != ALIVE){
        return -1;
    }
    for(i = 0; i < 16; i++){
        if(strcmp(buff.id,(char *) clients[i].id) == 0){
            break;
        }
        if(i == 15){
            return -1;
        }
    }
    if(clients[i].status != REGISTERED && clients[i].status != SEND_ALIVE){
		print_debug("S'ha rebut [ALIVE] en un estat que no era REGISTERED ni SEND_ALIVE");
        return -1;
    }
    if(atoi(buff.aleatori) != clients[i].random || strcmp(buff.dades,"") != 0){
		print_debug("Les dades o l'aleatori del [ALIVE] son incorrectes");
        return -1;
    }else{
        return 0;
    }
}

void *client_manager(void *argvs){
	struct sockaddr_in serv_new_addrs,cl_addrs;
	int rand,new_UDP_port,recved,new_UDP_socket,retl;
	char str_rand[9],str_new_UDP_port[5],str_TCP_port[5],debug_msg[128];
	struct PDU_UDP buffer,buffer2,REG_ACK_packet,REG_NACK_packet,REG_REJ_packet,ALIVE_packet, INFO_packet;
	fd_set selectset;
	struct timeval tv;
	int len = sizeof(cl_addrs);
	int server_UDP_socket = *((int *) argvs);
	
	recvfrom(server_UDP_socket,&buffer,84, MSG_WAITALL,(struct sockaddr *) &cl_addrs, (socklen_t *) &len);
	print_debug("S'ha rebut un paquet");
	
	/*Mirar si es REG_REQ o ALIVE
	 * si es REG_REQ igual
	 * si es ALIVE contestar*/
	if((recved = is_REG_REQ_correct(buffer)) == 0){
		print_debug("El paquet REG_REQ es correcte"); /* El client passa a WAIT_ACK_REG*/
		buffer.id[12] = '\0';
		print_debug("Client envia REG_REQ, passa a WAIT_ACK_REG");
		update_client(buffer.id,WAIT_ACK_REG);
		set_client_address(buffer.id,cl_addrs);
		
		rand = generate_random();
		new_UDP_port = generate_UDP_port();
		set_client_random(buffer.id,rand);
		set_client_udp_port(buffer.id,new_UDP_port);
		
		sprintf((char *) str_rand,"%i",rand);
		str_rand[8] = '\0';
		sprintf((char *) str_new_UDP_port,"%i",new_UDP_port);
		str_new_UDP_port[4] = '\0';
		
		REG_ACK_packet = create_udp_packet(REG_ACK,server_id,str_rand,str_new_UDP_port);
		
		memset(&serv_new_addrs,0,sizeof(struct sockaddr_in));
		    
		serv_new_addrs.sin_family = AF_INET;
		serv_new_addrs.sin_port = htons(new_UDP_port);
		serv_new_addrs.sin_addr.s_addr = INADDR_ANY;
		
		new_UDP_socket = socket(AF_INET,SOCK_DGRAM,0);
		
		if(bind(new_UDP_socket,(const struct sockaddr *)&serv_new_addrs,sizeof(serv_new_addrs))<0){
			print_debug("ERROR => No s'ha pogut bindejar el socket");
			exit(-1);
		}
		print_debug("Nou socket bindejat correctament");
		
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
				printf("Tipus nou paquet %i tipus reg_info %i\n",buffer2.tipus,REG_INFO);
				/*Rebut paquet REG_INFO, contestar amb el paquet que toqui*/
				if(buffer2.tipus != REG_INFO){
					print_debug("Tipus de paquet no esperat, no es contestarà");
					update_client(buffer2.id,DISCONNECTED);
				}else{
					printf("buffer %s buffer2 %s\n",buffer.id,buffer2.id);
					if(rand == atoi(buffer2.aleatori) && strcmp(buffer.id,buffer2.id) == 0){
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
			char debug_msg[128];
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
			ALIVE_packet = create_udp_packet(ALIVE,server_id,buffer.aleatori,buffer.id);
			update_client(buffer.id,SEND_ALIVE);
			sendto(server_UDP_socket,(struct PDU_UDP *) &ALIVE_packet,84,MSG_CONFIRM,(struct sockaddr *) &cl_addrs, len);
		}else if (recved == -1){
			ALIVE_packet = create_udp_packet(ALIVE_REJ,server_id,buffer.aleatori,buffer.id);
			update_client(buffer.id,SEND_ALIVE);
			sendto(server_UDP_socket,(struct PDU_UDP *) &ALIVE_packet,84,MSG_CONFIRM,(struct sockaddr *) &cl_addrs, len);
		}
	}
	return NULL;
}

void *register_handler_fun(void *argvs){ /*Bindeja socket 1 i crea thread al rebre paquets REG_REQ*/
    struct sockaddr_in serv_addrs;
    int server_UDP_socket;
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
    print_debug("Socket bindejat correctament");
    
    while(register_handler_alive == 1){
		FD_ZERO(&selectset);
		FD_SET(server_UDP_socket,&selectset);
		retl = select(server_UDP_socket+1,&selectset,NULL,NULL,0);
		if(retl){
			if(FD_ISSET(server_UDP_socket,&selectset)){
				print_debug("Creant thread per a rebre un paquet UDP");
				pthread_create(&client_manager_thread,NULL,client_manager,(void *) &server_UDP_socket);
				sleep(0.1);
			}
		}
    }
    return NULL;
}

void ezlist(){
	int i = 0;
	for(i = 0;i < 16;i++){
		printf("id %s status %i\n", clients[i].id,clients[i].status);
	}
}

int main(int argc,char *argv[]){
    FILE *cfg_file,*dat_file;
    int i,j,debug = 0;
    char filename[64] = "",datab_name[64] = "";
    char server_UDP_port_read[16],server_UDP_port_arr[4];
    char server_TCP_port_read[16],server_TCP_port_arr[4];
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
    while(i < 15){
        fgets(clients[i].id,32,dat_file);
        clients[i].id[12] = '\0';
        clients[i].status = DISCONNECTED;
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
    pthread_create(&register_handler,NULL,register_handler_fun,NULL);
    signal(SIGINT,handle_cntrc);
    while(0 < 1){
        sleep(10);
        list();
    }
    exit(0);
}
