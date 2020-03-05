#!/usr/bin/env python3.7

import os
import select
import sys
import signal
import socket
import struct
import threading
from time import sleep
from datetime import datetime

def cntrlc(signum, handler):
    print("Sortint per CNTRL+C")
    raise SystemExit

signal.signal(signal.SIGINT,cntrlc)

def dt(): return datetime.now().strftime("%d/%m/%Y %H:%M:%S => ")


def dt2(): return datetime.now().strftime("%Y-%m-%d;%H:%M:%S")


packtypes = {"REG_REQ": 0, "REG_INFO": 1, "REG_ACK": 2, "INFO_ACK": 3, "REG_NACK": 4, "INFO_NACK": 5, "REG_REJ": 6}

# ,
# "DISCONNECTED": int("0xa0", base=16), "NOT_REGISTERED": int("0xa1", base=16),
# "WAIT_ACK_REG": int("0xa2", base=16), "WAIT_INFO": int("0xa3", base=16),
# "WAIT_ACK_INFO": int("0xa4", base=16), "REGISTERED": int("0xa5", base=16),
# "SEND_ALIVE": int("0xa6", base=16)

debug = False
configfile = "client.cfg"
status = "DISCONNECTED"
print(dt() + "STATUS = " + status)

if len(sys.argv) > 1:
    for argcounter, arg in enumerate(sys.argv):
        try:
            if arg == "-d":
                debug = True
            elif arg.endswith(".cfg") and sys.argv[argcounter - 1] == "-c":
                try:
                    f = open(arg, "r")
                    f.close()
                    configfile = arg
                except FileNotFoundError:
                    print("No s'ha pogut trobar el fitxer de configuració")
                    exit(-1)
            elif arg == "-c":
                if sys.argv[argcounter + 1].endswith(".cfg"):
                    try:
                        f = open(sys.argv[argcounter + 1], "r")
                        f.close()
                        configfile = sys.argv[argcounter + 1]
                    except FileNotFoundError:
                        print("No s'ha pogut trobar el fitxer de configuració")
                        exit(-1)
        except IndexError:
            print("Ús: ./cl.py {-d} {-c <nom_arxiu>}")
            exit(-1)

if debug:
    print(dt() + "Llegint dades del fitxer de configuració " + configfile)

client = {}

with open(configfile, "r") as f:
    for line in f.readlines():
        if line.split("=")[0][:-1] == "Params":
            client["Params"] = {}
            for param in line.split("=")[1][1:-1].split(";"):
                client["Params"][param] = "NONE"
        else:
            client[line.split("=")[0][:-1]] = line.split("=")[1][1:-1]

should_send_reg_req = True
should_child_sleeps_alive = False
should_restart = False


def register():
    global should_send_reg_req,should_child_sleeps_alive
    if debug:
        print(dt() + "Creant un thread per a contar l'enviament de paquets REG_REQ")
    should_child_sleeps_alive = True
    child_sleeps_thread = threading.Thread(target=child_sleeps, args=[], daemon=True)
    child_sleeps_thread.start()


def create_reg_req():
    return struct.pack("B13s9s61s", int("0x00", base=16), client["Id"].encode(), "00000000".encode(), "".encode())


def send_reg_req():
    global num_of_packets, register_socket
    num_of_packets += 1
    server_UDP_address = (socket.gethostbyname(client["Server"]), int(client["Server-UDP"]))
    if debug:
        print(dt() + "Enviant paquet REG_REQ al servidor amb adreça " + str(server_UDP_address))
    register_socket.sendto(create_reg_req(), server_UDP_address)


def child_sleeps():
    global should_send_reg_req,should_child_sleeps_alive
    time = 1
    sleep(time)
    for i in range(3):
        if should_child_sleeps_alive:
            should_send_reg_req = True
            sleep(time)
        else:
            break
    while time < 3:
        if should_child_sleeps_alive:
            should_send_reg_req = True
            time += 1
            sleep(time)
        else:
            break
    while True:
        if should_child_sleeps_alive:
            should_send_reg_req = True
            sleep(time)
        else:
            break


if debug:
    print(dt() + "Creant socket UDP per a registrar-se al servidor")
register_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

if debug:
    print(dt() + "Iniciant registre")
register()
status = "WAIT_ACK_REG"
print(dt() + "STATUS = " + str(status))

num_of_packets = 0
attempted_registers = 0


def decompose(packet_to_decompose):
    if debug:
        print(dt() + "Llegint la informació d'un paquet rebut")
    tup = struct.unpack("B13s9s61s", packet_to_decompose)
    decomposed = {"Tipus": tup[0], "Id": tup[1].decode(), "Random": tup[2].decode(),
                  "Dades": tup[3].decode(errors="ignore")}
    filtered_dades = ""
    for c in decomposed["Dades"]:
        if c != '\0':
            filtered_dades = filtered_dades.__add__(c)
        else:
            break
    decomposed["Dades"] = filtered_dades
    if debug:
        print(dt() + "Dades del paquet rebut: " + str(decomposed))
    return decomposed


def create_reg_info(cl, data):
    if debug:
        print(dt() + "Preparant per enviar un paquet REG_INFO")
    dades = cl["Local-TCP"]
    dades = dades.__add__(",")
    for elem in cl["Params"].keys():
        dades = dades.__add__(elem + ";")
    return struct.pack("B13s9s61s", int("0x01", base=16), cl["Id"].encode(), data["Random"].encode(), dades.encode())


def register_waiting():
    global attempted_registers, status, num_of_packets, register_socket, should_send_reg_req
    while True:
        rdable, wtable, exceptional = select.select([register_socket], [], [], 0)
        if len(rdable) > 0:
            if debug:
                print(dt() + "S'ha rebut un paquet pel port UDP")
            should_child_sleeps_alive = False
            pack_from_server = register_socket.recv(84)
            data_server = decompose(pack_from_server)
            new_dades = ""
            for c in data_server["Dades"]:
                if c.isdigit() and c != '\0':
                    new_dades = new_dades.__add__(c)
                if c == '\0':
                    break
            data_server["Dades"] = new_dades
            aleatori = data_server["Random"]
            if data_server["Tipus"] != packtypes["REG_ACK"]:
                if debug:
                    print(dt() + "Tipus de paquet no esperat")
                if data_server["Tipus"] == packtypes["REG_NACK"]:
                    if debug:
                        print(dt() + "S'ha rebut un paquet REG_NACK")
                        print(dt() + "Reprenent enviament de REG_REQ")
                    status = "NOT_REGISTERED"
                    print(dt() + "STATUS = " + str(status))
                    attempted_registers += 1
                    num_of_packets = 0
                    register()
                else:
                    if debug:
                        print(dt() + "Iniciant nou procés de registre")
                    status = "NOT_REGISTERED"
                    print(dt() + "STATUS = " + str(status))
                    attempted_registers = 0
                    num_of_packets = 0
                    register()
            else:
                reg_info = create_reg_info(client, data_server)
                server_UDP_address = (socket.gethostbyname(client["Server"]), int(data_server["Dades"]))
                register_socket.sendto(reg_info, server_UDP_address)
                if debug:
                    print(dt() + "Enviat paquet REG_INFO")
                status = "WAIT_ACK_INFO"
                print(dt() + "STATUS = " + str(status))
                rdable, wtable, exceptional = select.select([register_socket], [], [], 2)
                if len(rdable) == 0:
                    if debug:
                        print(dt() + "No s'ha rebut el paquet INFO_ACK")
                    status = "NOT_REGISTERED"
                    print(dt() + "STATUS = " + str(status))
                    attempted_registers = 0
                    num_of_packets = 0
                    register()
                else:
                    pack_from_server = register_socket.recv(84)
                    data_server = decompose(pack_from_server)
                    if data_server["Tipus"] != packtypes["INFO_ACK"]:
                        if debug:
                            print(dt() + "Tipus de paquet no esperat")
                        status = "NOT_REGISTERED"
                        print(dt() + "STATUS = " + str(status))
                        attempted_registers += 1
                        num_of_packets = 0
                        register()
                    else:
                        if debug:
                            print(dt() + "S'ha rebut el paquet INFO_ACK")
                        status = "REGISTERED"
                        print(dt() + "STATUS = " + str(status))
                        should_child_sleeps_alive = False
                        return data_server, aleatori

        if should_send_reg_req:
            should_send_reg_req = False
            send_reg_req()

        if num_of_packets == 7:
            attempted_registers += 1
            num_of_packets = 0
            should_child_sleeps_alive = False
            if attempted_registers < 3:
                sleep(2)
                register()
            else:
                print(dt() + "No s'ha pogut connectar al servidor. Sortint")
                should_child_sleeps_alive = False
                exit(-1)


if debug:
    print(dt() + "Començant el procés de registre ")
data, rand = register_waiting()

# FINAL FASE DE REGISTRE
should_clock_alive = True

def alive_thread_communication():
    global client,send_alive_packet,sent_alives,status,should_clock_alive
    def alive():
        return struct.pack("B13s9s61s", int("0x10", base=16), client["Id"].encode(), rand.encode(),
                           "".encode())

    sent_alives = 0

    def send_alive():
        global should_clock_alive
        global status, sent_alives, send_alive_packet, should_restart
        if debug:
            print(dt() + "Enviant paquet ALIVE")
        if sent_alives == 3:
            if debug:
                print(dt() + "El servidor no ha contestat a 3 ALIVE. Reiniciant")
                os.kill(os.getpid(),signal.SIGUSR1)
                should_clock_alive = False
        register_socket.sendto(alive(), (socket.gethostbyname(client["Server"]), int(client["Server-UDP"])))
        send_alive_packet = False
        sent_alives += 1
        if should_clock_alive:
            status = "SEND_ALIVE"
            if debug:
                print(dt() + "STATUS = " + str(status))

    while should_clock_alive:
        if send_alive_packet and should_clock_alive:
            send_alive()

        rdable,wtable,exceptional = select.select([register_socket],[],[],0)
        if register_socket in rdable:
            packet_from_server = register_socket.recv(84)
            sent_alives = 0
            data_from_server = decompose(packet_from_server)

            if data_from_server["Tipus"] != 16 or data_from_server["Dades"] != client["Id"] or rand != data_from_server[
                "Random"]:
                if debug:
                    print(dt() + "Hi ha hagut algun error amb el paquet. Reiniciant")
                    os.kill(os.getpid(),signal.SIGUSR1)
                    should_clock_alive = False

send_alive_packet = True

def clock_signals():
    global send_alive_packet
    while should_clock_alive:
        send_alive_packet = True
        sleep(2)

def alive_handling():
    if debug:
        print(dt() + "Creant nou thread per mantenir comunicació periòdica amb el servidor")
    alive_thread = threading.Thread(target=alive_thread_communication,args=[],daemon=True)
    alive_thread.start()
    if debug:
        print(dt() + "Creant nou thread per a gestionar la comunicació periòdica amb el servidor")
    alive_clock = threading.Thread(target=clock_signals, args=[], daemon=True)
    alive_clock.start()

alive_handling()

def hand(signum,handler):
    global data,rand,should_clock_alive
    print(dt() + "Intentant tornar a registrar")
    data, rand = register_waiting()
    should_clock_alive = True
    alive_handling()

signal.signal(signal.SIGUSR1,hand)

def quit():
    raise SystemExit

def stat():
    global status
    print("**********DADES DISPOSITIU**********")
    print("   Id: " + client["Id"])
    print("   Status: " + status)
    print("\nParametre\tValor")
    for key in client["Params"].keys():
        print(str(key) + "\t\t" + str(client["Params"][key]))
    print("")
    print("************************************")


def cset(param_name,new_value):
    if param_name in client["Params"].keys():
        client["Params"][param_name] = new_value
        print(dt() + "Nou valor de " + param_name + " = " + new_value)
    else:
        print(dt() + "ERROR => Aquest parametre no es cap dispositiu")


def send(param_name):
    pass

def help():
    print("**********AJUDA COMANDES**********")
    print("Comanda  Ús \t\t\tUtilitat")
    print("")
    print("quit \t quit \t\t\tSortir del programa")
    print("stat \t stat \t\t\tMostrar informació del client")
    print("debug \t debug \t\t\tActiva o desactiva debug")
    print("set \t set <param> <valor> \tModifica el valor d'un paràmetre <param> amb el valor <valor>")
    print("send \t send <param> \t\tEnvia el valor de <param> al servidor via TCP")
    print("? \t ? \t\t\tMostra aquesta ajuda")
    print("**********************************")

while True:
    command = input("Introdueix una comanda: ")
    try:
        if command.split()[0] == "quit":
            quit()
        elif command.split()[0] == "stat":
            stat()
        elif command.split()[0] == "set":
            cset(command.split()[1],command.split()[2])
        elif command.split()[0] == "send":
            send(command.split()[1])
        elif command.split()[0] == "?":
            help()
        elif command.split()[0] == "debug":
            if debug:
                debug = False
                print(dt() + "Mode debug desactivat")
            else:
                print(dt() + "Mode debug activat")
                debug = True
        else:
            print("Comanda errònea, escriu ? per ajuda")
    except IndexError:
        print("Comanda errònea, escriu ? per ajuda")
