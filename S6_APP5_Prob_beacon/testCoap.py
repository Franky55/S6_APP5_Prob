import socket


# Mets ici l'adresse IP affichée par ton ESP32
ESP32_IP = "10.158.122.227"

COAP_PORT = 5683


def send_command(command):

    sock = socket.socket(
        socket.AF_INET,
        socket.SOCK_DGRAM
    )


    # Création d'un paquet CoAP GET minimal
    packet = bytearray([
        0x40,   # Version 1 + CON + Token length 0
        0x01,   # GET
        0x12,   # Message ID
        0x34
    ])


    # URI Path : led

    packet += bytes([
        0xB3,   # option URI-Path longueur 3
        ord('l'),
        ord('e'),
        ord('d')
    ])



    # URI Path : on/off

    if command == "on":

        packet += bytes([
            0x02,
            ord('o'),
            ord('n')
        ])

    elif command == "off":

        packet += bytes([
            0x03,
            ord('o'),
            ord('f'),
            ord('f')
        ])



    sock.sendto(
        packet,
        (
            ESP32_IP,
            COAP_PORT
        )
    )


    print(
        "Commande envoyée : LED",
        command.upper()
    )


    sock.close()



while True:

    print()
    print("1 - Allumer LED")
    print("2 - Éteindre LED")
    print("q - Quitter")


    choix = input("> ")


    if choix == "1":

        send_command("on")


    elif choix == "2":

        send_command("off")


    elif choix == "q":

        break