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


signal.signal(signal.SIGINT, cntrlc)


def dt(): return datetime.now().strftime("%d/%m/%Y %H:%M:%S => ")


def dt2(): return datetime.now().strftime("%Y-%m-%d;%H:%M:%S")


packtypes = {"REG_REQ": 0, "REG_INFO": 1, "REG_ACK": 2, "INFO_ACK": 3, "REG_NACK": 4, "INFO_NACK": 5, "REG_REJ": 6,
             "SEND_DATA": int("0x20", base=16), "SET_DATA": int("0x21", base=16), "GET_DATA": int("0x22", base=16),
             "DATA_ACK": int("0x23", base=16), "DATA_NACK": int("0x24", base=16), "DATA_REJ": int("0x25", base=16)}

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


def register():
    global should_send_reg_req, should_child_sleeps_alive
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
    global should_send_reg_req, should_child_sleeps_alive
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
    global attempted_registers, status, num_of_packets, register_socket, should_send_reg_req, should_child_sleeps_alive
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
                    register()
                else:
                    if debug:
                        print(dt() + "Iniciant nou procés de registre")
                    status = "NOT_REGISTERED"
                    print(dt() + "STATUS = " + str(status))
                    attempted_registers += 1
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
                    attempted_registers += 1
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
sent_alives = 0


def alive_thread_communication():
    global client, send_alive_packet, sent_alives, status, should_clock_alive

    def alive():
        return struct.pack("B13s9s61s", int("0x10", base=16), client["Id"].encode(), rand.encode(),
                           "".encode())

    sent_alives = 0

    def send_alive():
        global should_clock_alive
        global status, sent_alives, send_alive_packet
        if debug:
            print(dt() + "Enviant paquet ALIVE")
        if sent_alives == 3:
            if debug:
                print(dt() + "El servidor no ha contestat a 3 ALIVE. Reiniciant")
            status = "NOT_REGISTERED"
            print(dt() + "STATUS = " + str(status))
            os.kill(os.getpid(), signal.SIGUSR1)
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

        rdable, wtable, exceptional = select.select([register_socket], [], [], 0)
        if register_socket in rdable:
            packet_from_server = register_socket.recv(84)
            sent_alives = 0
            data_from_server = decompose(packet_from_server)

            if data_from_server["Tipus"] != 16 or data_from_server["Dades"] != client["Id"] or rand != data_from_server[
                    "Random"]:
                if debug:
                    print(dt() + "Hi ha hagut algun error amb el paquet. Reiniciant")
                os.kill(os.getpid(), signal.SIGUSR1)
                status = "NOT_REGISTERED"
                print(dt() + "STATUS = " + str(status))
                should_clock_alive = False
    if debug:
        print(dt() + "Thread finalitzat")


send_alive_packet = True


def clock_signals():
    global send_alive_packet
    while should_clock_alive:
        send_alive_packet = True
        sleep(2)
    if debug:
        print(dt() + "Thread finalitzat")


def alive_handling():
    if debug:
        print(dt() + "Creant nou thread per mantenir comunicació periòdica amb el servidor")
    alive_thread = threading.Thread(target=alive_thread_communication, args=[], daemon=True)
    alive_thread.start()
    if debug:
        print(dt() + "Creant nou thread per a gestionar la comunicació periòdica amb el servidor")
    alive_clock = threading.Thread(target=clock_signals, args=[], daemon=True)
    alive_clock.start()


alive_handling()


def hand(signum, handler):
    global data, rand, should_clock_alive, should_send_reg_req, should_child_sleeps_alive
    print(dt() + "Intentant tornar a registrar")
    should_send_reg_req = True
    should_child_sleeps_alive = True
    register()
    data, rand = register_waiting()
    should_clock_alive = True
    alive_handling()


signal.signal(signal.SIGUSR1, hand)


def cquit(signum, handler):
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


def cset(param_name, new_value):
    if param_name in client["Params"].keys():
        client["Params"][param_name] = new_value
        print(dt() + "Nou valor de " + param_name + " = " + new_value)
        return True
    else:
        print(dt() + "ERROR => Aquest parametre no es cap dispositiu")
        return False


def send_data_packet(param):
    return struct.pack("B13s9s8s16s80s", int("0x20", base=16), client["Id"].encode(), str(rand).encode(),
                       param.encode(), client["Params"][param].encode(), dt2().encode())


def decompose_TCP(packet_recv):
    global rand
    tup = struct.unpack("B13s9s8s16s80s", packet_recv)
    new_info = ""
    for i in range(12):
        new_info = new_info.__add__(tup[5].decode(errors="ignore")[i])
    if tup[0] != packtypes["DATA_ACK"] or tup[2].decode(errors="ignore") != str(rand) or new_info != client["Id"]:
        print(dt() + "El paquet no ha estat aceptat")
        if tup[0] == packtypes["DATA_NACK"]:
            if debug:
                print(dt() + "S'ha rebut un paquet DATA_NACK")
        elif tup[0] == packtypes["DATA_REJ"]:
            if debug:
                print(dt() + "S'ha rebut un paquet DATA_REJ")
        else:
            if debug:
                print(dt() + "S'ha rebut un paquet no esperat")
    else:
        print(dt() + "El paquet ha estat acceptat (s'ha rebut DATA_ACK)")

    decomposed = {"Tipus": tup[0], "Id": tup[1].decode(), "Random": tup[2].decode(),
                  "Element": tup[3].decode(errors="ignore"), "Valor": tup[4].decode(errors="ignore"),
                  "Info": new_info}
    return decomposed


def send(param_name):
    global data
    if param_name not in client["Params"].keys():
        print(dt() + str(param_name) + " no és cap paràmetre")
        print(dt() + "No s'ha establert cap connexió")
    else:
        send_data_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_TCP_address = (socket.gethostbyname(client["Server"]), int(data["Dades"]))
        send_data_socket.connect(server_TCP_address)
        send_data_socket.send(send_data_packet(param_name), 0)
        rdable, wtable, exceptional = select.select([send_data_socket], [], [], 3)
        if send_data_socket not in rdable:
            print(dt() + "El servidor no ha contestat a l'enviament de dades")
        else:
            pack_from_server = send_data_socket.recv(struct.calcsize("B13s9s8s16s80s"), 0)
            data_from_server = decompose_TCP(pack_from_server)
            print(data_from_server)


def fhelp():
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


def decompose_server(packet_server):
    tup = struct.unpack("B13s9s8s16s80s", packet_server)
    new_info = ""
    for i in range(12):
        new_info = new_info.__add__(tup[5].decode(errors="ignore")[i])
    decomposed = {"Tipus": tup[0], "Id": tup[1].decode(errors="ignore"), "Random": tup[2].decode(errors="ignore"),
                  "Elem": tup[3].decode(errors="ignore"), "Valor": tup[4].decode(errors="ignore"),
                  "Info": new_info}
    return decomposed


def create_get_data(element):
    if element in client["Params"].keys():
        if debug:
            print(dt() + "Enviant DATA_ACK")
        return struct.pack("B13s9s8s16s80s", packtypes["DATA_ACK"], client["Id"].encode(), rand.encode(),
                           element.encode(),
                           client["Params"][element].encode(), "".encode())
    else:
        if debug:
            print(dt() + "Enviant DATA_NACK")
        return struct.pack("B13s9s8s16s80s", packtypes["DATA_NACK"], client["Id"].encode(), rand.encode(),
                           element.encode(),
                           "NONE".encode(), (str(element) + " no és un dispositiu del client").encode())


def create_set_data_ack(element):
    print("Data_ack element", element)
    return struct.pack("B13s9s8s16s80s", packtypes["DATA_ACK"], client["Id"].encode(), rand.encode(), element.encode(),
                       client["Params"][element].encode(), dt2().encode())


def create_set_data_nack(element, typeof):
    if typeof == 0:
        return struct.pack("B13s9s8s16s80s", packtypes["DATA_NACK"], client["Id"].encode(), rand.encode(),
                           element.encode(),
                           "NONE".encode(), "Hi ha hagut un error al fer el set".encode())
    else:
        return struct.pack("B13s9s8s16s80s", packtypes["DATA_NACK"], client["Id"].encode(), rand.encode(),
                           element.encode(),
                           "NONE".encode(), "L'element és un sensor per tant no es pot configurar".encode())


def create_data_rej(element):
    return struct.pack("B13s9s8s16s80s", packtypes["DATA_REJ"], client["Id"].encode(), rand.encode(), element.encode(),
                       "NONE".encode(), "Error d'identificació".encode())


def waiting_for_server(new_socket):
    if debug:
        print(dt() + "S'ha rebut una connexió del servidor")
    packet_from_server = new_socket.recv(struct.calcsize("B13s9s8s16s80s"))
    data_from_server = decompose_server(packet_from_server)
    if data_from_server["Info"][:12] == client["Id"][:12] and data_from_server["Random"] == str(rand):
        if data_from_server["Tipus"] == packtypes["GET_DATA"]:
            new_socket.send(create_get_data(data_from_server["Elem"][:-1]), struct.calcsize("B13s9s8s16s80s"))
        elif data_from_server["Tipus"] == packtypes["SET_DATA"]:
            if data_from_server["Elem"][-2] == "I":
                set_result = cset(data_from_server["Elem"][:-1], data_from_server["Valor"])
                if set_result:
                    if debug:
                        print(dt() + "Enviant DATA_ACK")
                    new_socket.send(create_set_data_ack(data_from_server["Elem"][:-1]))
                else:
                    if debug:
                        print(dt() + "Enviant DATA_NACK")
                    new_socket.send(create_set_data_nack(data_from_server["Elem"][:-1], 0))
            else:
                if debug:
                    print(dt() + "Enviant DATA_NACK")
                new_socket.send(create_set_data_nack(data_from_server["Elem"][:-1], 1))
        else:
            if debug:
                print(dt() + "Rebut paquet no esperat via TCP. No es contestarà")
            new_socket.close()
    else:
        if debug:
            print(dt() + "Rebut paquet amb identificació incorrecta. Reiniciant")
        new_socket.send(create_data_rej(data_from_server["Elem"][:-1]))
        new_socket.close()
        os.kill(os.getpid(), signal.SIGUSR1)


def start_waiting_thread(data_socket):
    if debug:
        print(dt() + "Creant thread per a esperar una connexió amb el servidor")
    receive_data_thread = threading.Thread(target=waiting_for_server, args=[data_socket], daemon=True)
    receive_data_thread.start()
    return receive_data_thread


def prepare_server_connection():
    receive_data_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        receive_data_socket.bind(("", int(client["Local-TCP"])))
    except OSError:
        print(dt() + "No s'ha pogut bindejar el socket TCP")
        os.kill(os.getpid(), signal.SIGTERM)
        raise SystemExit
    receive_data_socket.listen(5)
    while True:
        new_socket, addr = receive_data_socket.accept()
        start_waiting_thread(new_socket)


if debug:
    print(dt() + "Creant thread per atendre peticions dels servidor")
server_connection_thread = threading.Thread(target=prepare_server_connection, args=[], daemon=True)
server_connection_thread.start()

signal.signal(signal.SIGTERM, cquit)

while True:
    command = input("Introdueix una comanda: ")
    try:
        if command.split()[0] == "quit":
            cquit(0, 0)
        elif command.split()[0] == "stat":
            stat()
        elif command.split()[0] == "set":
            cset(command.split()[1], command.split()[2])
        elif command.split()[0] == "send":
            send(command.split()[1])
        elif command.split()[0] == "?":
            fhelp()
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
