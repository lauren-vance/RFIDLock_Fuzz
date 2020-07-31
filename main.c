#include "mbed.h"
#include "TextLCD.h"
#include "MFRC522.h"
#include "Servo.h"
#include "EthernetInterface.h"
#include <stdlib.h>
#include <stdio.h>

//inputs
InterruptIn button(USER_BUTTON);
InterruptIn motion(D2);

//outputs
Serial              pc(USBTX, USBRX); // tx, rx
DigitalOut          led(LED3);
DigitalOut          ledR(D3);
DigitalOut          ledG(D4);
TextLCD             lcd(D0,D1,D6,D7,D8,D9);
MFRC522             RfChip(PC_12, PC_11, PC_10, PC_9, D10); //MOSI,MISO,SCK,SDA,RST
Servo               servo(D5);
EthernetInterface   eth;
TCPServer           srv;
TCPSocket           client;
SocketAddress       client_addr;

//variables
char    message[] = "    Welcome!      Door Locked";
double  delay = 0.5; // 500 ms
char    tagID;
char    storedKeys[100]={0};
char    deleteKey[5]={0};
int     deleteInt = 0;
int     numStored = 0;
int     timeOut;
int     numUnlocks = 0;
int     numAccessDenied = 0;
int     numDetections = 0;
int     index = 0;
enum    doorLock {
  LOCKED, //==0
  UNLOCKED    //==1
};
//initialize to locked
doorLock lockState = LOCKED;


/*===========================================================================*/
//card read function; stores UID associated with card in "tagID" if present
//input: none
//return: true if card is present, -1 otherwise

bool cardRead(void)
{  
    //check for new card
    if ( ! RfChip.PICC_IsNewCardPresent())
    {   
        return false;
    }

    //select one of the cards
    if ( ! RfChip.PICC_ReadCardSerial())
    {
        return false;
    }
     
    //for debugging   
    pc.printf("\n\rRFID Detected\n\r");
    
    //print card UID (for debugging)
    pc.printf("\nCard UID: ");
        
    for (uint8_t i = 0; i < RfChip.uid.size; i++)
    {
        //pc.printf(" %X02", RfChip.uid.uidByte[i]);
        pc.printf(" %X", RfChip.uid.uidByte[i]);
    }
    
    //store card UID (convert to int-based tagID) to check against for access
    tagID = '\0';
        
    for (uint8_t i = 0; i < RfChip.uid.size; i++)
    {
        tagID = RfChip.uid.uidByte[i]+ tagID;
    }
        
    //print tagID conversion (for debugging)    
    pc.printf("\n\rCard Tag ID: %d\n\r",tagID);

    wait_ms(1000);
    
    return true;
}

/*===========================================================================*/
//function to search through array of stored RFID keys
//input: array of keys to be searched
//return: index of stored key if a match is found and -1 otherwise
int searchKeys(char* storedKeys, int tagID)
{
    for(int j = 0; j<sizeof(storedKeys);j++)
    {
        if(tagID==storedKeys[j])
        {
            return j;
        }
    }     
    return -1;
}

/*===========================================================================*/
//function to unlock door (only entered when UID is a match)
//input: none 
//returns: none

void unlockDoor(void)
{
    lcd.cls();
    lcd.printf(" Access Granted");
    lcd.printf("   Now Unlocked");
    pc.printf("Card Accepted...\n\n\r");
    ledG = 1;
    ledR = 0;
    
    servo = UNLOCKED;
    lockState = UNLOCKED;
    numUnlocks++;
    
    wait(5);
}

/*===========================================================================*/
//function to store new RFID card; first gets ID from scanned key (cardRead()), then stores that ID into array of stored keys
//input: array of keys to be searched/stored into
//returns: success int (1 if success, 0 if duplicate, -1 if timeout)

int storeNew(char* storedKeys)
{   
    //disable motion detector from interrupting storage process
    __disable_irq();
    
    lcd.cls();
    lcd.locate(2,0);
    lcd.printf("Please scan");
    lcd.locate(3,1);
    lcd.printf("RFID card");
    
    timeOut = 0;
    
    //for debugging
    pc.printf("\n\rStore New Card Request\n\r");
        
    //loop until card is presented or timeout, get UID in cardRead() if presented
    while(timeOut<1000 && !cardRead())
    {   
        timeOut++;
        wait(0.001); //wait 1ms
    }
    
    //if while loop does not exit due to timeout (card found)
    if(timeOut<1000)
    {
        if(searchKeys(storedKeys,tagID) < 0)
        {
            pc.printf("\n\rCard presented, now storing UID...\n\n\r");
        
            int i = 0;
        
            //find first empty spot, set i equal to corresponding index
            while(storedKeys[i]!=0 && storedKeys[i]!='-')
                i++;
            
            //store current tagID from cardRead in this spot (3 char long)    
            storedKeys[i] = tagID;
        
            //for debugging to ensure stored ID matches detected ID
            pc.printf("The stored key tag ID is: %d\n\r",storedKeys[i]);
            pc.printf("Key Registered Successfully at index: storedKeys[%d]\n\r",i);
        
            numStored++;
            
            //for returning back to main
            lcd.cls();
            lcd.printf(message);
    
            //re-enable motion ISR
            __enable_irq();          
                 
            return 1;
        }
        else
        {
            pc.printf("Card is a duplicate, no new card stored.\n\r");
            
            //for returning back to main
            lcd.cls();
            lcd.printf(message);
    
            //re-enable motion ISR
            __enable_irq();        
                   
            return 0;
        }
    }
    
    else //if while loop does exit due to timeout (no card presented)
    {
        pc.printf("\n\rSystem timeout, no new card stored\n\r"); 
        
        //for returning back to main
        lcd.cls();
        lcd.printf(message);
    
        //re-enable motion ISR
        __enable_irq();    
        
        return -1;
    }
    
}
    
/*===========================================================================*/
//function to delete RFID card from list of keys accepted

bool dltKey(char* storedKeys, int tagID)
{
    //disable motion detector from interrupting function
    __disable_irq();
    
    //for debugging
    pc.printf("\n\rDelete Card Request\n\r");

    pc.printf("\n\rNow searching UID list...\n\n\r");
        
    //search list
    index = searchKeys(storedKeys,tagID);
    
    if(index >= 0)
    {
        storedKeys[index] = '-';
        pc.printf("Card tagID %d successfully removed from list at index: %d\n\r",tagID,index);
        //pc.printf("Check: storedKeys[n] = %d\n\r",storedKeys[index]);
        //pc.printf("Check: storedKeys[n+1] = %d\n\r",storedKeys[index+1]);
        numStored--;
        //re-enable motion ISR
        __enable_irq(); 
        return true;
    }

    else
    {
        pc.printf("Error: Match for %d not found\n\r",tagID);
        //re-enable motion ISR
        __enable_irq(); 
        return false;
    }
    
}

/*===========================================================================*/
//ISR called after motion detected

void motionDetected()
{
    lcd.cls();
    lcd.locate(2,0);
    lcd.printf("Please scan");
    lcd.locate(3,1);
    lcd.printf("RFID card");
    
    timeOut = 0;
    
    //loop until card is presented or timeout, get UID in cardRead() if presented
    while(timeOut<500 && !cardRead())
    {   
        timeOut++;
        wait(0.001); //wait 1ms
    }
    
    //if while loop does not exit due to timeout (card found)
    if(timeOut<500)
    {
        numDetections++;
            
        //check UID: unlock if match, "access denied" if no match
        //iterate through the storedKeys array until match is found
        
        if(searchKeys(storedKeys,tagID) >= 0)
        {
            unlockDoor();
        }
        
        else
        {       
            lcd.cls();
            lcd.printf(" Access Denied!");
            numAccessDenied++;
            pc.printf("Card Denied...\n\n\r");
            for (int i = 0; i<10; i++)
            {
                ledR = !ledR;
                wait(delay/2);
            }
        }
        
        pc.printf("=====RFID Data=====\n\r");
        pc.printf("Current Number of Cards Stored: %d\n\n\r",numStored);        
        pc.printf("Total Number of Cards Detected: %d\n\r",numDetections);
        pc.printf("Successful Unlocks:             %d\n\r",numUnlocks);
        pc.printf("Number of Accesses Denied:      %d\n\n\r",numAccessDenied);

        pc.printf("\n\n\rWaiting for new connection...\n\n\r");
    }
    
    //for returning back to main
    lcd.cls();
    lcd.printf(message);
    ledR = 1;
    ledG = 0;
    servo = LOCKED;
    lockState = LOCKED;    
}

/*===========================================================================*/
//Function to send message to client, uses delimiter to parse the message

void sendMessage(char* message)
{
    strcat(message,".");
    pc.printf("Sending message to Client: '%s' (%d Bytes)\n\n\r",message,strlen(message));

    int sent = 0;
    
    while(sent != strlen(message))
    {
        sent += client.send(message+sent,strlen(message)-sent);
    }
    
    //pc.printf("Sent %d bytes to client\n\n\r",sent);
}

/*===========================================================================*/
//Function to receive message from client, uses delimiter to parse the message

void recvRequest(void)
{
    char delim = '.';
    char recv_char;
    char request[256]={0};
    char statusUpdate[256]= {0};
    int count = 0;
    pc.printf("\n\rNow receiving client request: \"");
    while (true) 
    {           
        int n = client.recv(&recv_char, 1);
        
        //stream terminated
        if (n <= 0) 
            break;
            
        //delimiter found
        if(recv_char == delim)
        {
            request[count] = 0;
            pc.printf("\" (%d bytes)\n\r",strlen(request));
            break;
        }
        
        //buffer!=delim, get next char/byte
        else
        {
            request[count] = recv_char;
            pc.printf("%c",request[count]);
            count++;
        }
    }   
    
    if(strlen(request)==1)
    {
        switch(request[0])
        {
            case 'a':
            case 'A':
            {
                pc.printf("\n\rSend total number of cards detected...\n\r");
                sprintf(statusUpdate,"%d",numDetections);
                strcat(statusUpdate," Total Cards Detected");
                sendMessage(statusUpdate);  
                break;
            }
            
            case 'b':
            case 'B':
            {
                pc.printf("Send total number of unlocks...\n\r");
                sprintf(statusUpdate,"%d",numUnlocks);
                strcat(statusUpdate," Total Unlocks Performed");
                sendMessage(statusUpdate);
                break;
            }
        
            case 'c':
            case 'C':
            {
                pc.printf("Send total number of accesses denied...\n\r");
                sprintf(statusUpdate,"%d",numAccessDenied);
                strcat(statusUpdate," Total Accesses Denied");
                sendMessage(statusUpdate);            
                break;
            }
            
            case 'd':
            case 'D':
            {
                pc.printf("\n\rSend number of currently stored keys...\n\r"); 
                strcpy(statusUpdate,"List of current stored keys: ");
                for(int k = 0; k < strlen(storedKeys); k++)
                {
                    if(storedKeys[k] == '-')
                    {
                        //do not add to list
                    }
                    else
                    {
                        char keyList[20]={0};
                        sprintf(keyList,"%d ", storedKeys[k]);
                        strcat(statusUpdate,keyList);
                    }
                }
                strcat(statusUpdate,"; End of list");  
                
                sendMessage(statusUpdate);  
                break;   
            }      
        
            case 'e':
            case 'E':
            {
                int success = storeNew(storedKeys);
        
                if(success == 1)
                {
                    //send updated list of keys (store in statusUpdate, send to sendMessage)
                    
                    strcpy(statusUpdate,"New KeyCard Registered Successfully; List of current stored keys: ");
                    for(int k = 0; k < strlen(storedKeys); k++)
                    {
                        if(storedKeys[k] == '-')
                        {
                            //do not add to list
                        }
                        else
                        {
                            char keyList[20]={0};
                            sprintf(keyList,"%d ", storedKeys[k]);
                            strcat(statusUpdate,keyList);
                        }
                    }
                    strcat(statusUpdate,"; End of list");   
                    
                    //pc.printf("=== List of current stored keys === \n\r-----------------------------------\n\r");
                    //for(int k = 0; k < numStored; k++)
                    //{
                        //pc.printf("Key %d: %d\n\r",k+1,storedKeys[k]);
                    //}
                    //pc.printf("End of list !\n\n\r");

                }
        
                else
                {
                    if(success == 0)
                        strcpy(statusUpdate,"Card is a duplicate; no new card stored");
                    else
                        strcpy(statusUpdate,"System timeout, no new card stored");
                }
                
                sendMessage(statusUpdate);
                break;   
            } 
        
            default:    
            {
                sendMessage("Request unrecognized; Please try again.");
            }
        } 
    }
    
    else if(request[0] == 'f' || request[0] == 'F')
    {
        //check if request is an integer(for dltKey)
        memset(deleteKey,0,sizeof(deleteKey));
        int n = 0;

        for(int m = 1; m < strlen(request); m++)
        {
            if(request[m] >= 48 && request[m] <= 57)
            {
                  //get tagID
                  deleteKey[n] = request[m];
                  n++;
            }
            else //if some other unrecognized message was sent (no numbers following 'f')
            {
                break;
            }
        }
        
        tagID = atoi(deleteKey);
        
        if(tagID > 0) //(eliminate anything that ==0)
        {
            //Search for given tagID:
            int success = dltKey(storedKeys,tagID);
                
            if(success == 1)    //if tagID exists in stored key array and was deleted
            {
                strcpy(statusUpdate,"KeyCard Deleted Successfully; List of current stored keys: ");
                for(int k = 0; k < strlen(storedKeys); k++)
                {
                    if(storedKeys[k] == '-')
                    {
                        //do not add to list
                    }
                    else
                    {
                        char keyList[20]={0};
                        sprintf(keyList,"%d ", storedKeys[k]);
                        strcat(statusUpdate,keyList);
                    }
                }
                strcat(statusUpdate,"; End of list");            
            }
            
            else
            {
                if(success==0)
                    strcpy(statusUpdate,"Error: No Match Found");
                else    //if success == -1
                    strcpy(statusUpdate,"System timeout, no new card stored");
            }   

            sendMessage(statusUpdate); 
        }
        else
        {
            sendMessage("Request unrecognized; Please try again.");
        }
    }
    
    //request > 1 character and does not begin with 'f' (unrecognized)
    else if(strstr(request,"List of Keys"))
    {
        //get each key and overwrite storedKeys
        char keyWrite[100] = {0};
        char * split = strtok(request,":");
        while(split != NULL)
        {
            strcpy(keyWrite,split);
            split = strtok(NULL, " ");              
        }
            
        split = strtok(keyWrite,",");
        int p = 0;
        while(split!=NULL)
        {
            //put in storedKeys
            tagID = atoi(split);
            storedKeys[p] = tagID;
            split = strtok(NULL, " ");
            p++;
        }
        strcat(statusUpdate,"Confirmed");
        sendMessage(statusUpdate);
    }
    
    //request > 1 character, does not begin with 'f', and does not contain list of keys    
    else
        sendMessage("Request unrecognized; Please try again.");
}

/*=============================================================================
=============================================================================*/

int main()
{
    //Ethernet/server inits
    eth.connect();  //*
    srv.open(&eth); 
    
    //Listen for request from client (computer)
    srv.bind(eth.get_ip_address(),7); //port 7 = echo *
    srv.listen();   //*
    
    //for debugging
    pc.printf("\n\r===== Lock Program Begin =====\n\n\r");
    pc.printf("Board IP Address: %s",eth.get_ip_address());
    
    //initialize LCD to welcome msg
    lcd.cls();
    wait(0.01);
    lcd.printf(message);
    ledR = 1;
    ledG = 0;
    
    //initialize to locked
    servo = LOCKED;
    
    //initialize RFID Reader
    RfChip.PCD_Init();
    
    //assign interrupt functions to inputs
    motion.rise(&motionDetected);
    
    //loop and blink on-board red led while waiting for input
    while (1) 
    {
        pc.printf("\n\n\rWaiting for new connection...\n\n\r");
        srv.accept(&client,&client_addr);
        
        pc.printf("***Connection from: %s***\n\r", client_addr.get_ip_address());
        recvRequest();
        
    }
}
