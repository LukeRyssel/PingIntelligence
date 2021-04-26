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
    void removeItem(String packetID);
    bool searchQueue(String packetID);
    void removeFromQueue(String packetID);
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

void queue::removeItem(String packetID)
{
    String id;
    for(int i = 0; i < size(); i++)
    {
        for(int j = 2; j < 5; j++)
        {
            id += arr[i][j];
        }
        if(packetID == id)
        {
            for(int k = i; k < size() -1 ; k++)
            {
                arr[k] = arr[k + 1]; 
            }
            count--;
            i = size();
        }
    }
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

void queue::removeFromQueue(String packetID)
{
  String id;
  String value;
  for (int i = 0; i < size(); i++)
  {
    value = peek();
    dequeue();
    for (int j = 2; j < 5; j++)
    {
      id += value[j];
    }
    if (packetID != id)
      enqueue(value);
    else
    {
      Serial.print("Removed packetID: ");
      Serial.println(packetID);
    }
  }
}