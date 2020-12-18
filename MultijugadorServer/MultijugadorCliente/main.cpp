#include <iostream>
#include <WS2tcpip.h>
#include <string>
#define MAX_SIZE 255

#pragma comment (lib, "ws2_32.lib")

using namespace std;

enum class States
{
	Connection,
	StartMatch,
	Turns,
	EndGame,
	MAX
};

enum class ServerMessage
{
	RegistrationComplete,
	BlockInput,
	WaitingForOpponent,
	YourTurn,
	OpponentTurn,
	Error,
	Rematch,
	InvalidMove,
	Exit,
	MAX
};

struct message
{
	byte cmd;
	char data[MAX_SIZE];
};

void DrawBoard(char g[][3])
{
	cout << g[0][0] << g[0][1] << g[0][2] << endl;
	cout << g[1][0] << g[1][1] << g[1][2] << endl;
	cout << g[2][0] << g[2][1] << g[2][2] << endl;
	cout << endl;
}

message msgReceived;
message msgSent;
string msgtest = "";

void StringToChar(string string, char charArray[])
{
	char* aux = new char[MAX_SIZE];
	strcpy_s(aux, MAX_SIZE, string.c_str());
	strcpy_s(charArray, MAX_SIZE, aux);
}

void ProcessInput(byte cmd)
{
	memset(&msgSent, 0, sizeof(msgSent));
	msgSent.cmd = cmd;
	switch (msgSent.cmd)
	{
	case (byte)States::Turns:
	{
		cout << "Elegir fila (de 0 a 2) y columna (de 0 a 2). Ej: 20, 11, 02" << endl;
	}
	break;
	case (byte)States::EndGame:
	{
		cout << "Presiona Y para seguir jugando o N para salir." << endl;
		if (msgSent.cmd == 'n')
			msgtest = "close";
	}
	break;
	}
	fflush(stdin);
	cin.getline((char*)msgSent.data, MAX_SIZE);
}

int main()
{
	WORD version = MAKEWORD(2, 2);
	WSADATA data;

	if (WSAStartup(version, &data) == 0)
	{
		string ipToUse("127.0.0.1");
		int portToUse = 8900;

		sockaddr_in server;
		server.sin_family = AF_INET;
		server.sin_port = htons(portToUse);
		inet_pton(AF_INET, ipToUse.c_str(), &server.sin_addr);

		SOCKET out = socket(AF_INET, SOCK_DGRAM, 0);
		if (out == INVALID_SOCKET)
		{
			cout << "Invalid socket: " << WSAGetLastError() << endl;
			return -1;
		}
		else
		{
			cout << "Bienvenido al server de TaTeTi!" << endl;
		}

		int serverSize = sizeof(server);

		message connectionMessage;
		connectionMessage.cmd = (byte)States::Connection;
		string mes = "";
		strcpy_s(connectionMessage.data, sizeof(string), mes.c_str());

		sendto(out, (char*)&connectionMessage, sizeof(message), 0, (sockaddr*)&server, sizeof(server));

		bool registered = false;

		do
		{
			memset(&msgSent, 0, sizeof(msgSent));
			memset(&msgReceived, 0, sizeof(msgReceived));

			int bytesIn = recvfrom(out, (char*)&msgReceived, sizeof(message), 0, (sockaddr*)&server, &serverSize);

			if (msgReceived.cmd == (byte)ServerMessage::RegistrationComplete)
			{
				registered = true;
				cout << msgReceived.data << endl;
				msgSent.cmd = (byte)States::StartMatch;
				cin.getline((char*)msgSent.data, MAX_SIZE);
				sendto(out, (char*)&msgSent, sizeof(message), 0, (sockaddr*)&server, sizeof(server));
			}

			if (registered)
			{
				switch (msgReceived.cmd)
				{
				case (byte)ServerMessage::BlockInput:
				{
					cout << msgReceived.data << endl;
				}
				break;
				case (byte)ServerMessage::YourTurn:
				{
					char g[3][3];
					int aux = 0;
					for (int i = 0; i < 3; i++)
					{
						for (int j = 0; j < 3; j++)
						{
							g[i][j] = msgReceived.data[i + j + aux];
						}
						aux += 2;
					}
					DrawBoard(g);

					cout << "Es tu turno." << endl;

					ProcessInput((byte)States::Turns);
					sendto(out, (char*)&msgSent, sizeof(message), 0, (sockaddr*)&server, sizeof(server));
					cout << "Espera a que el otro jugador realice su jugada." << endl;
				}
				break;
				case (byte)ServerMessage::Rematch:
				{
					system("cls");
					cout << msgReceived.data << endl;


					ProcessInput((byte)States::EndGame);
					sendto(out, (char*)&msgSent, sizeof(message), 0, (sockaddr*)&server, sizeof(server));
					if (msgSent.data[0] == 'n' || msgSent.data[0] == 'N')
						msgtest = "close";
				}
				break;
				case (byte)ServerMessage::WaitingForOpponent:
				{
					cout << msgReceived.data << endl;
				}
				break;
				case (byte)ServerMessage::Error:
				{
					cout << "Error: " << msgReceived.data << endl;
				}
				break;
				case (byte)ServerMessage::OpponentTurn:
				{
					char g[3][3];
					int aux = 0;
					for (int i = 0; i < 3; i++)
					{
						for (int j = 0; j < 3; j++)
						{
							g[i][j] = msgReceived.data[i + j + aux];
						}
						aux += 2;
					}
					DrawBoard(g);
				}
				break;
				case (byte)ServerMessage::Exit:
				{
					return 0;
				}
				break;
				}
			}
		} while (msgtest != "close");

		// cerrar el socket
		closesocket(out);
	}
	else
	{
		return -1;
	}

	fflush(stdin);

	return 0;

	// cerrar winsock
	WSACleanup();
}
