import socket
import mysql.connector


localIP     = "192.168.1.7"

localPort   = 1337

bufferSize  = 1024

db = mysql.connector.connect(
    host = "localhost",
    user = "user",
    passwd = "12345Word###",
    database = "Capstone"
)
mycursor = db.cursor()

def runScript(myScript):
    mycursor.execute(myScript)
    db.commit();
    for x in mycursor:
        print(x)

msgFromServer       = "Hello UDP Client"

bytesToSend         = str.encode(msgFromServer)



# Create a datagram socket

UDPServerSocket = socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)


# Bind to address and ip

UDPServerSocket.bind((localIP, localPort))



print("UDP server up and listening")



# Listen for incoming datagrams

while(True):

   bytesAddressPair = UDPServerSocket.recvfrom(bufferSize)

    message = bytesAddressPair[0]

    address = bytesAddressPair[1]

    clientMsg = "Message from Client:{}".format(message)
    clientIP  = "Client IP Address:{}".format(address)

    print(clientMsg)
    print(clientIP)
    runScript(message)
