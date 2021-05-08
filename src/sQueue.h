// code used from https://www.techiedelight.com/queue-implementation-cpp/

#include <Arduino.h>
#include <string.h>

// Define the default capacity of a queue
#define SIZE 10

// A class to store a queue
class queue
{
    String *arr;     // array to store queue elements
    int capacity; // maximum capacity of the queue
    int front;    // front points to the front element in the queue (if any)
    int rear;     // rear points to the last element in the queue
    int count;    // current size of the queue

public:
    queue(int size = SIZE); // constructor
    ~queue();               // destructor

    void dequeue();
    void enqueue(String x);
    String peek();
    int size();
    bool isEmpty();
    bool isFull();
    bool searchQueue(String packetID);
    String removeFromQueue(String packetID);
    void printQueue(String name);
};

// Constructor to initialize a queue
queue::queue(int size)
{
    arr = new String[size];
    capacity = size;
    front = 0;
    rear = -1;
    count = 0;
}

// Destructor to free memory allocated to the queue
queue::~queue()
{
    delete[] arr;
}

// Utility function to dequeue the front element
void queue::dequeue()
{
    // check for queue underflow
    if (isEmpty())
    {
        Serial.println("Queue is empty");
    }

    front = (front + 1) % capacity;
    count--;
}

// Utility function to add an item to the queue
void queue::enqueue(String item)
{
    // check for queue overflow
    if (isFull())
    {
        Serial.println("Queue is full");
    }
    //Serial.print("Successfully added packet: ");
    //Serial.println(item);
    rear = (rear + 1) % capacity;
    arr[rear] = item;
    count++;
}

// Utility function to return the front element of the queue
String queue::peek()
{
    if (isEmpty())
    {
        Serial.println("Queue is empty");
    }
    return arr[front];
}

// Utility function to return the size of the queue
int queue::size()
{
    return count;
}

// Utility function to check if the queue is empty or not
bool queue::isEmpty()
{
    return (size() == 0);
}

// Utility function to check if the queue is full or not
bool queue::isFull()
{
    return (size() == capacity);
}

bool queue::searchQueue(String packetID)
{
    String id;
    if(size() == 0)
        return false;
    for(int i = 0; i < size(); i++)
    {
        id = "";
        for(int j = 2; j < 5; j++)
            id += arr[i][j];
        if(packetID == id)
            return true;
    }
    return false;
}

String queue::removeFromQueue(String packetID)
{
  String id;
  String value;
  String packet;
  for (int i = 0; i < size(); i++)
  {
    value = peek();
    dequeue();
    id = "";
    for (int j = 2; j < 5; j++)
    {
      id += value[j];
    }
    if (packetID != id || value[0] == '1')
      enqueue(value);
    else
    {
      packet = value;
    }
  }
  return packet;
}

// ugly function but nice print
void queue::printQueue(String name)
{
    String packet;
    Serial.println("-------------------------------------------------------------------------------");
    Serial.print("|");
    int k;
    for(k = 0; k < 79 / 2 - name.length(); k++)
        Serial.print(" ");
    Serial.print(name);
    for(k = k + name.length(); k < 79 - 2; k++)
        Serial.print(" ");
    Serial.println("|");
    Serial.println("-------------------------------------------------------------------------------");
    Serial.println("| Packet Type | Packet ID | Src Addr | Dest Addr | Payload | Origin Packet ID |");
    Serial.println("-------------------------------------------------------------------------------");
    for (int i = 0; i < size(); i++)
    {
        packet = peek();
        // data packet
        if(packet[0] == '0')
        {
            Serial.print("|      ");
            Serial.print(packet[0]);
            Serial.print("      |    ");
            // packet ID
            for(int j = 2; j < 5; j++)
                Serial.print(packet[j]);
            Serial.print("    |   ");
            // Src addr
            for(int j = 6; j < 10; j++)
                Serial.print(packet[j]);
            Serial.print("   |   ");
            // dest addr
            for(int j = 11; j < 15; j++)
                Serial.print(packet[j]);
            Serial.print("    |  ");
            // temp
            for(int j = 21; j < 25; j++)
                Serial.print(packet[j]);
            Serial.print("   | ");
            Serial.println("                 |");
        }
        // node receipt
        else if(packet[0] == '1')
        {
            Serial.print("|      ");
            Serial.print(packet[0]);
            Serial.print("      |    ");
            for(int j = 2; j < 5; j++)
                Serial.print(packet[j]);
            Serial.print("    |   ");
            Serial.print("       |   ");
            for(int j = 6; j < 10; j++)
                Serial.print(packet[j]);
            Serial.print("    |  ");
            Serial.print("       | ");
            Serial.println("                 |");
        }
        // end receipt
        else if(packet[0] == '2')
        {
            Serial.print("|      ");
            Serial.print(packet[0]);
            Serial.print("      |    ");
            for(int j = 2; j < 5; j++)
                Serial.print(packet[j]);
            Serial.print("    |          |           |         |       ");
            for(int j = 6; j < 9; j++)
                Serial.print(packet[j]);
            Serial.println("        |");
        }
        // add to back, will loop around
        dequeue();
        enqueue(packet);
    }
    Serial.println("-------------------------------------------------------------------------------");
    Serial.println();
}
