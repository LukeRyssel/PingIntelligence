import socket
import mysql.connector


localIP     = "192.168.1.6"

localPort   = 12345

bufferSize  = 1024
print("Do the thing")
db = mysql.connector.connect(
    host = "192.168.1.3",
    port = 1433,
    user = "User",
    passwd = "Password",
    database = "Capstone"
)
print("Do the thing 2")
mycursor = db.cursor()

def runScript(myScript):
    mycursor.execut(myScript)
    for x in mycursor:
        print(x)
runscript("Put query here")