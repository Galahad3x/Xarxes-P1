#include "common.h"

int register_handler;
int allowed_disps_pids[16];
struct client clients[16];
int status_controller[2];

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
    printf("%s\n",(char *) buff.id);
    for(i = 0; i < 16; i++){
        printf("%s\n",(char *) clients[i].id);
        if(strcmp(buff.id,(char *) clients[i].id) == 0){
            break;
        }
        if(i == 15){
            return 2;
        }
    }
    if(clients[i].status != DISCONNECTED && clients[i].status != NOT_REGISTERED){
        return 1;
    }
    if(strcmp(buff.aleatori,"00000000\0") != 0 || strcmp(buff.dades,"") != 0){
        return 1;
    }else{
        return 0;
    }
}

void handle_cntrc(){
    print_debug("Sortint per ^C");
    kill(register_handler,SIGTERM);
    wait(NULL);
    exit(0);
}

void update_clients(struct pipe_info received){
    int i = 0;
    for(i = 0;i < 16;i++){
        if(strcmp(clients[i].id,received.affected_id) == 0){
            clients[i].status = received.new_status;
        }
    }
}

void exit_safely(){
    int i = 0;
    for(i = 0;i < 16;i++){
        if(allowed_disps_pids[i] != 0){
            kill(allowed_disps_pids[i],SIGTERM);
        }
    }
}

void list(){
    int i = 0,j = 0;
    printf("**********DADES DISPOSITIUS**********\n");
    printf("Id\t\tStatus\t\tDispositius\n");
    for(i = 0; i < 15;i++){
        printf("%s\t\t%i\t\t",(char *) clients[i].id,clients[i].status);
        for(j = 0; j < 15;j++){
            printf("%s;",(char *) clients[i].dispositius[j] -> nom);
        }
        printf("\n");
        printf("\t\t\t\t\t\t");
        j = 0;
        for(j = 0; j < 15;j++){
            printf("%s;",(char *) clients[i].dispositius[j] -> valor);
        }
    }
}

int main(int argc,char *argv[]){
    FILE *cfg_file,*dat_file;
    int i,j,debug = 0;
    char filename[64] = "",datab_name[64] = "",server_id[32];
    char server_UDP_port_read[16],server_UDP_port_arr[4];
    char server_TCP_port_read[16],server_TCP_port_arr[4];
    int server_UDP_port = 0,server_TCP_port = 0;
    pipe(status_controller);
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
    register_handler = fork();
    if (register_handler == 0){
        signal(SIGTERM,exit_safely);
        struct sockaddr_in serv_addrs,cl_addrs;
        int server_UDP_socket,allowed_disps_counter;

        serv_addrs.sin_family = AF_INET;
        serv_addrs.sin_port = htons(server_UDP_port);
        serv_addrs.sin_addr.s_addr = INADDR_ANY;

        server_UDP_socket = socket(AF_INET,SOCK_DGRAM,0);

        if(bind(server_UDP_socket,(const struct sockaddr *)&serv_addrs,sizeof(serv_addrs))<0){
            print_debug("ERROR => No s'ha pogut bindejar el socket");
            exit(-1);
        }
        print_debug("Socket bindejat correctament");

        int len = sizeof(cl_addrs);
        int recved;
        struct PDU_UDP buffer;
        struct pipe_info send_status_update;
        while(0 < 1){
            recvfrom(server_UDP_socket,&buffer,84, MSG_WAITALL,(struct sockaddr *) &cl_addrs, (socklen_t *) &len);
            print_debug("S'ha rebut un paquet");
            if((recved = is_REG_REQ_correct(buffer)) == 0){
                print_debug("El paquet REG_REQ es correcte"); /* El client passa a WAIT_ACK_REG*/
                send_status_update.pid = getpid();
                strcpy(send_status_update.affected_id,buffer.id);
                send_status_update.new_status = WAIT_ACK_REG;
                strcpy(send_status_update.info,"Client envia REG_REQ, passa a WAIT_ACK_REG");
                /*Fork per generar random, nou port UDP i enviar i rebre REG_ACK i INFO*/
                /*allowed_disps_pids[allowed_disps_counter] = fork();
                if(allowed_disps_pids[allowed_disps_counter] == 0){

                }
                */
                allowed_disps_counter++;
                write(status_controller[1],&send_status_update,sizeof(struct pipe_info));
            }else{
                print_debug("Paquet no correcte");
            }
        }
    }else{
        signal(SIGINT,handle_cntrc);
    }
    while(0 < 1){
        list();
        struct pipe_info receive_status_update;
        read(status_controller[0],&receive_status_update,sizeof(struct pipe_info));
        printf("Rebut pel pipe %i %s %i %s\n",receive_status_update.pid,(char *) receive_status_update.affected_id,
            receive_status_update.new_status,(char *)receive_status_update.info);
        update_clients(receive_status_update);
        sleep(1);
        list();
    }
    exit(0);
}
