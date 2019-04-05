#include <iostream>
#include <windows.h>
#include <fstream>
#include <string>

using namespace std;

HANDLE deviceHandle; // A handle to the device for example a file.(handle can be anything from an integer index to a pointer to a resource in kernel space)
LPCTSTR portName;  // Porn name for example COM3.(LPCSTR = constant char*)
DCB dcb; // Defines the control setting for a serial communications device
COMMTIMEOUTS commtimeouts; // Contains the time-out parameters for a communications device. The parameters determine the behavior of ReadFile, WriteFile, ReadFileEx, and WriteFileEx operations on the device

unsigned short tmpCRC; // Unsigned short USHORT 2 bytes
char fileName[255];
char charBuf; // Buffer that receives the data read from a file or device.
int charNumber = 1; // The maximum number of bytes to be read/write.
unsigned long charSize = sizeof(charBuf); // The variable that receives the number of bytes read.
char packet[128]; // Packet for part of message
int packetNumber = 1;
bool isCorrect;

const char ACK =(char)6;  // Acknowledge.
const char EOT =(char)4;  // End of Transmission
const char SOH =(char)1;  // Start Of Heading.
const char NAK =(char)21; // Negative Acknowledge.
const char CAN =(char)24; // Cancel (Force receiver to start sending C's).

int calculateCRC(char *wsk, int count)
{
    unsigned int controlSumCRC = 0;

    while (--count >= 0)
    {
        controlSumCRC = controlSumCRC ^ (unsigned int)*wsk << 8;
        wsk++;
        for (int i = 0; i < 8; ++i)
            if (controlSumCRC & 0x8000) controlSumCRC = (controlSumCRC << 1) ^ 0x1021;
            else controlSumCRC = controlSumCRC << 1;
    }
    return controlSumCRC;
}


int checkParity(int x, int y) {
    if (y == 0) return 1;
    if (y == 1) return x;

    int output = x;

    for (int i = 2; i <= y; i++)
        output = output * x;

    return output;
}

// Binary to char
char charCRC(int n, int signNumber)
{
    int x, binary[16];

    for(int z = 0; z < 16; z++) binary[z] = 0;

    for(int i=0; i < 16; i++) {
        x = n % 2;
        if (x == 1) n = (n - 1)/2;
        if (x == 0) n = n/2;
        binary[15 - i] = x;
    }

    x = 0;
    int k;

    if(signNumber == 1) k = 7;
    if(signNumber == 2) k = 15;

    for (int i = 0; i < 8; i++)
        x = x + checkParity(2,i) * binary[k - i];

    return (char)x;
}

void receiveFile() {

    ofstream file;
    bool transmission = false;
    bool packetCorrect = true;
    int controlSumCRC[2];
    string sign; // For sending sign
    char complement225; // For complement to 255

    cout << "Podaj nazwe pliku do ktorego ma zostac zapisana wiadomosc" << endl;
    cin >> fileName;

    cout << "Wpisz NAK lub C" << endl;
    cin >> sign;

    if (sign == "NAK") charBuf = NAK;
    else charBuf = 'C';

    cout << " Wysylanie " << endl;

    for (int i = 0; i < 6; i++) {
        // Sending NAK or C
        WriteFile(deviceHandle, & charBuf, charNumber, &charSize, NULL);
        cout << "Wysylanie NAK | C " << endl;
        cout << "Oczekiwanie na komunikat SOH..." << endl;
        // Reading SOH  from file
        ReadFile(deviceHandle, & charBuf, charNumber, &charSize, NULL);
        if (charBuf == SOH) {
            cout << "Otrzymano SOH" << endl;
            transmission = true;
            break;
        }
    }

    if (!transmission) {
        cout << "Cos poszlo nie tak" << endl;
        exit(1);
    }

    file.open(fileName, ios::binary);
    if (!file.good()) {
        cout << "Plik nie istnieje" << endl;
        exit(1);
    }

    while (true) {

        if (charBuf != SOH) {
            cout << "Oczekiwanie na komunikat SOH..." << endl;
            // Reading SOH  from file
            ReadFile(deviceHandle, & charBuf, charNumber, &charSize, NULL);
            cout << "Otrzymano SOH" << endl;
        }

        // Read packet number
        cout << "Odczytano numer pakietu" << endl;
        ReadFile(deviceHandle, & charBuf, charNumber, &charSize, NULL);
        packetNumber = (int) charBuf;
        // Read complement to 255
        cout << "Odczytano dopelnienie" << endl;
        ReadFile(deviceHandle, & charBuf, charNumber, &charSize, NULL);
        complement225 = charBuf;

        // Read data block
        for (int i = 0; i < 128; i++) {
            ReadFile(deviceHandle, & charBuf, charNumber, &charSize, NULL);
            packet[i] = charBuf;
        }
        cout << "Odczutano blok danych" << endl;

        ReadFile(deviceHandle, & charBuf, charNumber, &charSize, NULL);
        controlSumCRC[0] = charBuf; // First CRT byte
        // If CRC control sum has 2 bytes
        if (sign == "C") {
            ReadFile(deviceHandle, & charBuf, charNumber, &charSize, NULL);
            controlSumCRC[1] = charBuf; // Second CRC byte
        }

        cout << "Odczytanos sume kontrolna" << endl;

        // Check if correct packet was send
        if ((char) (255 - packetNumber) != complement225) {
            cout << "Otrzymano niepoprawny pakiet" << endl;
            WriteFile(deviceHandle, & NAK, charNumber, &charSize, NULL);
            packetCorrect = false;
        } else if (sign == "NAK") {
            char controlSum = 0;
            for (int i = 0; i < 128; i++) {
                controlSum += packet[i];
            }
            if (controlSum != controlSumCRC[0]) {
                cout << "Niepoprawna suma kontrolna" << endl;
                WriteFile(deviceHandle, & NAK, charNumber, &charSize, NULL);
                packetCorrect = false;
            }
        } else {
            tmpCRC = calculateCRC(packet, 128);

            if (charCRC(tmpCRC, 1) != controlSumCRC[0] || charCRC(tmpCRC, 2) != controlSumCRC[1]) {
                cout << "Niepoprawna suma kontrolna" << endl;
                WriteFile(deviceHandle, &NAK, charNumber, &charSize, NULL);
                packetCorrect = false;
            }
        }

        // Write message to file
        if (packetCorrect) {
            for (int i = 0; i < 128; i++) {
                if (packet[i] != 26)
                    file << packet[i];
            }
            WriteFile(deviceHandle, &ACK, charNumber, &charSize, NULL);
        }
        ReadFile(deviceHandle, & charBuf, charNumber, &charSize, NULL);
        if (charBuf == EOT || charBuf == CAN)  {
            file.close();
            CloseHandle(deviceHandle);
            if (charBuf == CAN) cout << "Otrzymano CAN przesylanie zakonczone przerwaniem" << endl;
            else {
                cout << "Otrzymanie EOT" << endl;
                cout << "Przesylanie zakonczone poprawnie" << endl;
            }
            break;
        }
    }
}

void sendingFile() {

    ifstream file; // File to send.
    bool transmission = false; // If transmission correct
    int code = 0; // Error code

    cout << "Nazwa pliku do wyslania" << endl;
    cin >> fileName;


    for (int i = 0; i < 6; i++) {
        cout << "Oczekiwanie na NAC lub C" << endl;
        ReadFile(deviceHandle, & charBuf, charNumber, & charSize, NULL); // Reads data from the specified file or input/output (I/O) device
        // For CRC version
        if (charBuf == 'C') {
            cout << "Otrzymany znak: " << charBuf << endl;
            code = 1;
            transmission = true;
            break;
        } else if (charBuf == NAK) {
            cout << "Otrzymany znak: NAK" << endl;
            code = 2;
            transmission = true;
            break;
        }
    }

    if (!transmission) {
        cout << "Cos poszlo nie tak" << endl;
        exit(1);
    }

    file.open(fileName, ios::binary);
    if(!file.good()) {
        cout << "Plik nie istnieje" << endl;
        exit(1);
    }

    while(!file.eof()) {
        for (int i = 0; i < 128; i++) {
            // Control character
            packet[i] = (char)26;
        }

        int j = 0;

        while( j < 128 && !file.eof()) {
            packet[j] = file.get();
            if (file.eof()) packet[j] = (char)26;
            j++;
        }

        isCorrect = false;

        while(!isCorrect) {
            cout << "Wysylanie SOH" << endl;
            WriteFile(deviceHandle, & SOH, charNumber, & charSize, NULL); // Sending SOH sign.
            charBuf = (char)packetNumber;
            cout << "Wysylanie numeru pakietu" << endl;
            WriteFile(deviceHandle, & charBuf, charNumber, & charSize, NULL); // Sending package number.
            charBuf = (char)255 - packetNumber;
            cout << "Wysylanie dopelnienia" << endl;
            WriteFile(deviceHandle, & charBuf, charNumber, & charSize, NULL); // Sending complement.

            cout << "Wysylanie bloku danych" << endl;
            for (int i = 0; i < 128; i++) {
                charBuf = packet[i];
                WriteFile(deviceHandle, & charBuf, charNumber, & charSize, NULL);
            }

            //Control sum.
            cout << "Wysylanie sumy kontrolnej" << endl;
            if (code == 2) {
                char controlSum = 0;
                for (int i = 0; i < 128; i++) {
                    controlSum += packet[i];
                }

                WriteFile(deviceHandle, & controlSum, charNumber, & charSize, NULL);

            } else { // CRC
                tmpCRC = calculateCRC(packet, 128);
                charBuf = charCRC(tmpCRC, 1);
                WriteFile(deviceHandle, & charBuf, charNumber, & charSize, NULL);
                charBuf = charCRC(tmpCRC, 2);
                WriteFile(deviceHandle, & charBuf, charNumber, & charSize, NULL);

            }

            while(true) {

                cout << "Oczekiwanie na odpowiedz" << endl;

                ReadFile(deviceHandle, & charBuf, charNumber, & charSize, NULL);

                if (charBuf == ACK) {
                    isCorrect = true;
                    cout << "Przeslano poprawnie otrzymano ACK" << endl;
                    break;
                }

                if (charBuf == NAK) {
                    cout << "Otrzymano NAK" << endl;
                    break;
                }

                if(charBuf == CAN) {
                    cout << "Poloczenie zostalo przerwane otrzymano CAN" << endl;
                    exit(1);
                }
            }
        }

        if(packetNumber == 255) packetNumber = 1;
        else packetNumber++;
    }
    file.close();

    cout << "Wysylanie EOT" << endl;

    while(true) {
        charBuf = EOT;
        WriteFile(deviceHandle, & charBuf, charNumber, & charSize, NULL);
        ReadFile(deviceHandle, & charBuf, charNumber, & charSize, NULL);
        if(charBuf == ACK) break;
    }

    CloseHandle(deviceHandle);
    cout << "Wysylanie zakonczone poprawnie" << endl;
}


int main() {
    int choice;
    //default port name
    portName = "COM3";
    cout << "Rozpoczynainie transmisji na porcie COM3" << endl;

    //CreateFile()
    //pornName, access, share mode 0 if can't be shared, address of security descriptor NULL, file attributes 0, handle of file with attributes to copy 0
    //Creating a file for communication
    deviceHandle = CreateFile(portName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

    if (deviceHandle != INVALID_HANDLE_VALUE) {
        dcb.DCBlength = sizeof(dcb); // The length of the structure, in bytes. The caller must set this member to sizeof(DCB)
        dcb.BaudRate = CBR_9600; // 9600 bps The baud rate at which the communications device operates
        dcb.fBinary = TRUE; // If this member is TRUE, binary mode is enabled. Windows does not support nonbinary mode transfers, so this member must be TRUE.
        dcb.ByteSize = 8; // The number of bits in the bytes transmitted and received.
        dcb.Parity = NOPARITY; // The parity scheme to be used.
        dcb.StopBits = ONESTOPBIT; // The number of stop bits to be used.

        commtimeouts.ReadIntervalTimeout = 10000; // 10s The maximum time allowed to elapse before the arrival of the next byte on the communications line, in milliseconds.
        commtimeouts.ReadTotalTimeoutMultiplier = 10000; // The multiplier used to calculate the total time-out period for read operations, in milliseconds. For each read operation, this value is multiplied by the requested number of bytes to be read.
        commtimeouts.ReadTotalTimeoutConstant = 10000; // A constant used to calculate the total time-out period for read operations, in milliseconds. For each read operation, this value is added to the product of the ReadTotalTimeoutMultiplier member and the requested number of bytes.
        commtimeouts.WriteTotalTimeoutMultiplier = 100; // The multiplier used to calculate the total time-out period for write operations, in milliseconds.
        commtimeouts.WriteTotalTimeoutConstant = 100; // A constant used to calculate the total time-out period for write operations, in milliseconds.

        SetCommState(deviceHandle, & dcb); // Configures a communications device according to the specifications in a device-control block (a DCB structure).
        SetCommTimeouts(deviceHandle, & commtimeouts);// Sets the time-out parameters for all read and write operations on a specified communications device.
    } else {
        cout << "Nie udalo sie nawiazac polonczenia" << endl;
    }

    cout << "Uruchom jako odbiornik 1 lub jako nadajnik 2" << endl;
    cin >> choice;
    if(choice == 1) {
        receiveFile();
    } else if(choice == 2){
        sendingFile();
    } else {
        cout << "Nie ma takiego numeru" << endl;
    }
    return 0;
}
