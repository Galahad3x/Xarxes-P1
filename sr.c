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
			clients[i].status = WAIT_ACK_REG;
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

void *manage_client_register(void *argvs){
	fd_set selectset;
	struct sockaddr_in serv_addrs,cl_addrs;
	struct PDU_UDP buffer;
	int new_UDP_socket;
	int retl,len;
	struct timeval tv;
	struct client a = *((struct client *) argvs);
	
	len = sizeof(cl_addrs);
	
	printf("id %s port %i dades %i\n",(char *) a.id, (int) a.new_udp_port, (int) a.random);
	
	memset(&serv_addrs,0,sizeof(struct sockaddr_in));

    serv_addrs.sin_family = AF_INET;
    serv_addrs.sin_port = htons(a.new_udp_port);
    serv_addrs.sin_addr.s_addr = INADDR_ANY;

    new_UDP_socket = socket(AF_INET,SOCK_DGRAM,0);

	if(bind(new_UDP_socket,(const struct sockaddr *)&serv_addrs,sizeof(serv_addrs))<0){
        print_debug("ERROR => No s'ha pogut bindejar el socket");
        exit(-1);
    }
    print_debug("Nou socket bindejat correctament");

	FD_ZERO(&selectset);
	FD_SET(new_UDP_socket,&selectset);
	
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	retl = select(new_UDP_socket+1,&selectset,NULL,NULL,(struct timeval *) &tv);
	if(retl){
		if(FD_ISSET(new_UDP_socket,&selectset)){
			fflush(stdout);
			recvfrom(new_UDP_socket,&buffer,84, MSG_WAITALL,(struct sockaddr *) &cl_addrs, (socklen_t *) &len);
			printf("Tipus nou paquet %i tipus reg_info %i\n",buffer.tipus,REG_INFO);
			/*Rebut paquet REG_INFO, contestar amb el paquet que toqui*/
			if(buffer.tipus != REG_INFO){
				print_debug("Tipus de paquet no esperat, no es contestarà");
				update_client(buffer.id,DISCONNECTED);
			}else{
				printf("aleatori %i client %i\n",atoi(buffer.aleatori),a.random);
				if(atoi(buffer.aleatori) == a.random){
					printf("Iguals\n");
				}
			}
		}else{
			char debug_msg[128];
			sprintf(debug_msg,"El client %s no ha contestat el REG_ACK",a.id);
			print_debug(debug_msg);
			update_client(a.id,DISCONNECTED);
		}
	}
	return NULL;
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

void *register_handler_fun(void *argvs){
    struct sockaddr_in serv_addrs,cl_addrs;
    int server_UDP_socket;
    int alive_handlers_counter = 0;
    struct client *arg;
    pthread_t alive_handlers[16];
	fd_set selectset;
	int rand,new_udp_port;
	char str_rand[8],str_new_udp_port[4];
	int len = sizeof(cl_addrs);
    int recved,retl;
    struct PDU_UDP buffer;
    struct PDU_UDP REG_ACK_packet;
    struct PDU_UDP REG_NACK_packet;
    struct PDU_UDP REG_REJ_packet;
	
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
				recvfrom(server_UDP_socket,&buffer,84, MSG_WAITALL,(struct sockaddr *) &cl_addrs, (socklen_t *) &len);
				print_debug("S'ha rebut un paquet");
				if((recved = is_REG_REQ_correct(buffer)) == 0){
					print_debug("El paquet REG_REQ es correcte"); /* El client passa a WAIT_ACK_REG*/
					buffer.id[12] = '\0';
					print_debug("Client envia REG_REQ, passa a WAIT_ACK_REG");
					update_client(buffer.id,WAIT_ACK_REG);
					set_client_address(buffer.id,cl_addrs);
					rand = generate_random();
					new_udp_port = generate_udp_port();
					set_client_random(buffer.id,rand);
					set_client_udp_port(buffer.id,new_udp_port);
					print_debug("Creant thread per gestionar el registre d'un client\n");
					sprintf((char *) str_rand,"%i",rand);
					printf("String rand %s\n",(char *) str_rand);
					sprintf((char *) str_new_udp_port,"%i",new_udp_port);
					printf("String new udp port %s\n",(char *) str_new_udp_port);
					REG_ACK_packet = create_udp_packet(REG_ACK,server_id,str_rand,str_new_udp_port);
					/*Perque aleatori no es guarda pero dades si?*/
					printf("Paquet aleatori %s\n",(char *) REG_ACK_packet.aleatori);
					printf("Paquet dades %s\n",(char *) REG_ACK_packet.dades);
					sendto(server_UDP_socket,(struct PDU_UDP *) &REG_ACK_packet,84,MSG_CONFIRM,(struct sockaddr *) &cl_addrs, len);
					arg = malloc(sizeof(*arg));
					*arg = managing_client(buffer.id);
					pthread_create(&alive_handlers[alive_handlers_counter],NULL,manage_client_register,(void *) arg);
					client_manager_alive[alive_handlers_counter] = 1;
					alive_handlers_counter++;
				}else if(recved == 1){
					REG_NACK_packet = create_udp_packet(REG_NACK,server_id,"00000000","Alguna cosa no quadra entre estats o dades");
					sendto(server_UDP_socket,(struct PDU_UDP *) &REG_NACK_packet,84,MSG_CONFIRM,(struct sockaddr *) &cl_addrs, len);
					update_client(buffer.id,DISCONNECTED);
				}else if(recved == 2){
					REG_REJ_packet = create_udp_packet(REG_REJ,server_id,"00000000","Error d'identificació");
					sendto(server_UDP_socket,(struct PDU_UDP *) &REG_REJ_packet,84,MSG_CONFIRM,(struct sockaddr *) &cl_addrs, len);
					update_client(buffer.id,DISCONNECTED);
				}
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
