#include <ctime>
#include <iostream>
#include <WS2tcpip.h>
#include <string>
#include <vector>
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

enum class Results
{
	Player1Won,
	Player2Won,
	Tie,
	KeepPlaying,
	MAX
};

struct GameState
{
	char state[3][3] = { { '+','+','+' }, { '+','+','+' }, { '+','+','+' } };
};

struct Player
{
	int port = -1;
	int num = -1;
	string alias = "";
	int gameID = -1;
	Player* enemy;
	sockaddr_in client;
	int wantsRematch = -1;
};

struct Game
{
	int turn = 0;
	Player* p[2];
	GameState gameState;
};

struct message
{
	byte cmd;
	char data[MAX_SIZE];
};

char CheckState(GameState n)
{
	for (int i = 0; i < 3; i++)
	{
		if (n.state[i][0] == n.state[i][1] && n.state[i][0] == n.state[i][2] && n.state[i][0] != '+')
			return n.state[i][0];
	}
	for (int i = 0; i < 3; i++)
	{
		if (n.state[0][i] == n.state[1][i] && n.state[0][i] == n.state[2][i] && n.state[0][i] != '+')
			return n.state[0][i];
	}
	
	if (n.state[0][0] == n.state[1][1] && n.state[0][0] == n.state[2][2] && n.state[0][0] != '+')
		return n.state[0][0];
	if (n.state[0][2] == n.state[1][1] && n.state[0][2] == n.state[2][0] && n.state[0][2] != '+')
		return n.state[0][2];

	bool canMakeMove = false;
	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			if (n.state[i][j] == '+')
				canMakeMove = true;
		}
	}

	if (!canMakeMove)
		return '*';

	return '+';
}

vector<Game*> games;
SOCKET listening;

Player* SearchPlayer(int port, bool& found)
{
	for (int i = 0; i < games.size(); i++)
	{
		for (int j = 0; j < 2; j++)
		{
			if (games[i]->p[j] != nullptr)
			{
				if (port == games[i]->p[j]->port)
				{
					found = true;
					return games[i]->p[j];
				}
			}
		}
	}
	return nullptr;
}

Game* SearchAvailableGame(int& game, int& player)
{
	int count = 0;
	for (int i = 0; i < games.size(); i++, count++)
	{
		for (int j = 0; j < 2; j++)
		{
			if (games[i]->p[j] == nullptr)
			{
				game = i;
				player = j;
				return games[i];
			}
		}
	}

	Game* g = new Game();
	game = count;
	player = 0;
	games.push_back(g);

	return g;
}

Game* SearchAvailableGame(int& game, int& player, int enemyGameID)
{
	int count = 0;
	for (int i = 0; i < games.size(); i++, count++)
	{
		if (i != enemyGameID)
		{
			for (int j = 0; j < 2; j++)
			{
				if (games[i]->p[j] == nullptr)
				{
					game = i;
					player = j;
					return games[i];
				}
			}
		}
	}

	Game* g = nullptr;
	return g;
}

int Turn(Game* g, Player p, string position)
{
	// stoi converts string to int
	int aux = stoi(position);

	int rowNumber = aux / 10;
	int colNumber = aux % 10;

	byte currentTurn;

	if (p.num == 0)
		currentTurn = 'O';
	else
		currentTurn = 'X';

	if (g->gameState.state[rowNumber][colNumber] == '+')
		g->gameState.state[rowNumber][colNumber] = currentTurn;
	else
		return (int)ServerMessage::InvalidMove;

	char status = CheckState(g->gameState);

	if (status == 'O')
		return (int)Results::Player1Won;
	if (status == 'X')
		return (int)Results::Player2Won;
	if (status == '*')
		return (int)Results::Tie;

	return (int)Results::KeepPlaying;
}

void ResetBoard(Game* g)
{
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++)
			g->gameState.state[i][j] = '+';
}

void DrawBoard(int g[][3])
{
	cout << g[0][0] << g[0][1] << g[0][2] << endl;
	cout << g[1][0] << g[1][1] << g[1][2] << endl;
	cout << g[2][0] << g[2][1] << g[2][2] << endl;
	cout << endl;
}

char* ArrayToString(char g[][3])
{
	char* result = new char[9];
	char* aux = result;

	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			*result = g[i][j];
			result++;
		}
	}
	*result = '\0';
	result = aux;

	return result;
}

void StringToChar(string string, char charArray[])
{
	char* aux = new char[MAX_SIZE];
	strcpy_s(aux, MAX_SIZE, string.c_str());
	strcpy_s(charArray, MAX_SIZE, aux);
}

void ResetPlayers()
{
	for (int i = 0; i < games.size(); i++)
	{
		for (int j = 0; j < 2; j++)
		{
			games[i]->p[j]->gameID = i;
			games[i]->p[j]->num = j;
			games[i]->p[j]->wantsRematch = -1;
		}
	}
}

void CheckAvailablePlayer(Player* enemy)
{
	int gameID;
	int player;
	Game* g = SearchAvailableGame(gameID, player, enemy->gameID);

	if (player == 0 || gameID == enemy->gameID || !g )
		return;

	games[enemy->gameID]->p[enemy->num] = nullptr;
	vector<Game*>::iterator iter = games.begin();
	games.erase(iter + enemy->gameID);

	g->p[player] = enemy;
	g->p[player]->enemy = g->p[0];
	g->p[0]->enemy = g->p[player];

	for (int i = 0; i < 2; i++)
	{
		g->p[i]->gameID = gameID;
	}

	ResetBoard(g);

	ResetPlayers();

	message msg;
	msg.cmd = (byte)ServerMessage::BlockInput;
	string s;

	char c[MAX_SIZE] = "Jugador encontrado: ";
	strcat_s(c, enemy->alias.c_str());
	StringToChar(c, msg.data);
	sendto(listening, (char*)&msg, sizeof(message), 0, (sockaddr*)&g->p[0]->client, sizeof(g->p[0]->client));

	char c2[MAX_SIZE] = "Jugador encontrado: ";
	strcat_s(c2, g->p[0]->alias.c_str());
	s = ", es el turno de tu oponente";
	strcat_s(c2, s.c_str());
	StringToChar(c2, msg.data);
	sendto(listening, (char*)&msg, sizeof(message), 0, (sockaddr*)&enemy->client, sizeof(enemy->client));

	msg.cmd = (byte)ServerMessage::YourTurn;
	s = ArrayToString(g->gameState.state);
	StringToChar(s, msg.data);
	sendto(listening, (char*)&msg, sizeof(message), 0, (sockaddr*)&g->p[0]->client, sizeof(g->p[0]->client));

	msg.cmd = (byte)ServerMessage::OpponentTurn;
	sendto(listening, (char*)&msg, sizeof(message), 0, (sockaddr*)&enemy->client, sizeof(enemy->client));
}

int main()
{
	WSADATA data;
	WORD ver = MAKEWORD(2, 2);
	int wsOk = WSAStartup(ver, &data);
	if (wsOk != 0)
	{
		cerr << "No pudo iniciar Winsock." << endl;
		return -1;
	}

	listening = socket(AF_INET, SOCK_DGRAM, 0);
	if (listening == INVALID_SOCKET)
	{
		cerr << "Invalid socket." << endl;
		return -1;
	}

	sockaddr_in hint;
	hint.sin_family = AF_INET;
	hint.sin_port = htons(8900);
	inet_pton(AF_INET, "127.0.0.1", &hint.sin_addr); 

	int bindResult = bind(listening, (sockaddr*)&hint, sizeof(hint));
	if (bindResult == SOCKET_ERROR)
	{
		cerr << "No se pudo hacer el bind" << endl;
		return -1;
	}
	
	char buf[1024];
	message received;
	message sent;
	sockaddr_in client;
	int clientSize = sizeof(client);

	memset(buf, 0, sizeof(buf));

	// funcion bloqueante
	bool closeWindow = false;
	while (!closeWindow)
	{
		memset(&received, 0, sizeof(received));
		memset(&sent, 0, sizeof(sent));

		int bytesIn = recvfrom(listening, (char*)&received, sizeof(message), 0, (sockaddr*)&client, &clientSize);

		if (bytesIn == SOCKET_ERROR)
		{
			cerr << "Error al recibir data." << endl;
			return -1;
		}
		
		switch (received.cmd)
		{
		case (byte)States::Connection:
		{
			int gameID = -1;
			int player = -1;
			Game* g = SearchAvailableGame(gameID, player);

			g->p[player] = new Player();

			g->p[player]->gameID = gameID;
			g->p[player]->num = player;
			g->p[player]->port = client.sin_port;
			g->p[player]->client = client;
			g->p[player]->wantsRematch = -1;
			cout << "Se unio al server el jugador " << player << " , ID de partida: " << gameID << endl;

			if (player == 1)
			{
				g->p[0]->enemy = g->p[player];
				g->p[player]->enemy = g->p[0];
			}

			sent.cmd = (byte)ServerMessage::RegistrationComplete;
			string s = "Elige tu alias: ";
			strcpy_s(sent.data, sizeof(sent.data), s.c_str());
			sendto(listening, (char*)&sent, sizeof(message), 0, (sockaddr*)&(client), sizeof(client));
		}
		break;
		case (byte)States::StartMatch:
		{
			bool playerFound = false;
			Player* p = SearchPlayer(client.sin_port, playerFound);
			if (playerFound)
			{
				p->alias = received.data;
				cout << "Jugador " << p->num  << " alias: " << p->alias.c_str() << endl;

				message msg;
				msg.cmd = (byte)ServerMessage::BlockInput;
				string s;
				char* aux = new char[MAX_SIZE];

				if (p->enemy != nullptr)
				{
					s = "Tu oponente es ";

					char c[MAX_SIZE] = "Tu oponente es ";

					string s2 = ", es tu turno eres O";

					Player* auxPlayer = games[p->gameID]->p[0];

					strcat_s(c, MAX_SIZE, auxPlayer->enemy->alias.c_str());
					strcat_s(c, MAX_SIZE, s2.c_str());
					StringToChar(c, msg.data);

					sendto(listening, (char*)&msg, sizeof(message), 0, (sockaddr*)&auxPlayer->client, sizeof(auxPlayer->client));

					msg.cmd = (byte)ServerMessage::YourTurn;

					s = ArrayToString(games[auxPlayer->gameID]->gameState.state);

					StringToChar(s, msg.data);
					sendto(listening, (char*)&msg, sizeof(message), 0, (sockaddr*)&auxPlayer->client, sizeof(auxPlayer->client));

					msg.cmd = (byte)ServerMessage::BlockInput;

					char c2[MAX_SIZE] = "Tu oponente es ";

					s2 = ", es el turno de tu oponente eres X.";

					strcat_s(c2, MAX_SIZE, auxPlayer->alias.c_str());
					strcat_s(c2, MAX_SIZE, s2.c_str());
					StringToChar(c2, msg.data);

					auxPlayer = auxPlayer->enemy;
					sendto(listening, (char*)&msg, sizeof(message), 0, (sockaddr*)&auxPlayer->client, sizeof(auxPlayer->client));

					msg.cmd = (byte)ServerMessage::OpponentTurn;

					s = ArrayToString(games[auxPlayer->gameID]->gameState.state);

					StringToChar(s, msg.data);
					sendto(listening, (char*)&msg, sizeof(message), 0, (sockaddr*)&auxPlayer->client, sizeof(auxPlayer->client));
				}
				else
				{
					msg.cmd = (byte)ServerMessage::WaitingForOpponent;
					s = "Esperando oponente";
					StringToChar(s, msg.data);
					sendto(listening, (char*)&msg, sizeof(message), 0, (sockaddr*)&p->client, sizeof(p->client));
				}
			}
		}
		break;
		case (byte)States::Turns:
		{
			bool playerFound = false;
			Player* p = SearchPlayer(client.sin_port, playerFound);
			message msg;
			char* aux = new char[MAX_SIZE];
			string s;

			Player* enemy = p->enemy;

			msg.cmd = (byte)ServerMessage::YourTurn;

			int winner = Turn(games[p->gameID], *p, received.data);

			s = ArrayToString(games[p->gameID]->gameState.state);

			if (winner != (int)Results::KeepPlaying && winner != (int)Results::Tie && winner != (int)ServerMessage::InvalidMove)
			{
				msg.cmd = (byte)ServerMessage::OpponentTurn;
				StringToChar(s, msg.data);
				sendto(listening, (char*)&msg, sizeof(message), 0, (sockaddr*)&(p->client), sizeof(p->client));
				sendto(listening, (char*)&msg, sizeof(message), 0, (sockaddr*)&(enemy->client), sizeof(enemy->client));

				msg.cmd = (byte)ServerMessage::Rematch;
				s = "Ganaste! \nJugar la revancha?";
				StringToChar(s, msg.data);

				sendto(listening, (char*)&msg, sizeof(message), 0, (sockaddr*)&(p->client), sizeof(p->client));

				s = "Perdiste :( \nJugar la revancha?";
				StringToChar(s, msg.data);
				sendto(listening, (char*)&msg, sizeof(message), 0, (sockaddr*)&(enemy->client), sizeof(enemy->client));
			}
			else if (winner == (int)Results::Tie)
			{
				msg.cmd = (byte)ServerMessage::OpponentTurn;
				StringToChar(s, msg.data);
				sendto(listening, (char*)&msg, sizeof(message), 0, (sockaddr*)&(p->client), sizeof(p->client));
				sendto(listening, (char*)&msg, sizeof(message), 0, (sockaddr*)&(enemy->client), sizeof(enemy->client));

				msg.cmd = (byte)ServerMessage::Rematch;
				s = "Empate. \nJugar la revancha?";
				StringToChar(s, msg.data);
				sendto(listening, (char*)&msg, sizeof(message), 0, (sockaddr*)&(p->client), sizeof(p->client));
				sendto(listening, (char*)&msg, sizeof(message), 0, (sockaddr*)&(enemy->client), sizeof(enemy->client));
			}
			else if (winner == (int)ServerMessage::InvalidMove)
			{
				msg.cmd = (byte)ServerMessage::Error;
				s = "Posicion invalida, intentalo nuevamente";
				StringToChar(s, msg.data);
				sendto(listening, (char*)&msg, sizeof(message), 0, (sockaddr*)&(p->client), sizeof(p->client));
				msg.cmd = (byte)ServerMessage::YourTurn;
				s = ArrayToString(games[p->gameID]->gameState.state);
				StringToChar(s, msg.data);
				sendto(listening, (char*)&msg, sizeof(message), 0, (sockaddr*)&(p->client), sizeof(p->client));
			}
			else
			{
				StringToChar(s, msg.data);
				sendto(listening, (char*)&msg, sizeof(message), 0, (sockaddr*)&(enemy->client), sizeof(enemy->client));

				msg.cmd = (byte)ServerMessage::OpponentTurn;
				sendto(listening, (char*)&msg, sizeof(message), 0, (sockaddr*)&(p->client), sizeof(p->client));
			}
		}
		break;
		case (byte)States::EndGame:
		{
			if (received.data[0] == 'y' || received.data[0] == 'Y')
			{
				bool playerFound = false;
				Player* p = SearchPlayer(client.sin_port, playerFound);
				message msg;
				ResetBoard(games[p->gameID]);
				msg.cmd = (byte)ServerMessage::BlockInput;
				string s;
				bool rematch = false;
				p->wantsRematch = 1;
				
				if (p->enemy != nullptr)
				{
					if (p->enemy->wantsRematch == 1)
						rematch = true;
				}
				else
					CheckAvailablePlayer(p);

				if (rematch)
				{
					msg.cmd = (byte)ServerMessage::BlockInput;
					char c[MAX_SIZE] = "Revancha contra: ";
					strcat_s(c, p->alias.c_str());
					strcat_s(c, s.c_str());
					StringToChar(c, msg.data);
					sendto(listening, (char*)&msg, sizeof(message), 0, (sockaddr*)&p->enemy->client, sizeof(p->enemy->client));
					
					msg.cmd = (byte)ServerMessage::YourTurn;
					s = ArrayToString(games[p->gameID]->gameState.state);
					StringToChar(s, msg.data);
					sendto(listening, (char*)&msg, sizeof(message), 0, (sockaddr*)&p->enemy->client, sizeof(p->enemy->client));

					msg.cmd = (byte)ServerMessage::BlockInput;
					char c2[MAX_SIZE] = "Revancha contra: ";
					strcat_s(c2, p->enemy->alias.c_str());
					strcat_s(c2, s.c_str());
					StringToChar(c2, msg.data);
					sendto(listening, (char*)&msg, sizeof(message), 0, (sockaddr*)&p->client, sizeof(p->client));
					
					msg.cmd = (byte)ServerMessage::OpponentTurn;
					s = ArrayToString(games[p->gameID]->gameState.state);
					StringToChar(s, msg.data);
					sendto(listening, (char*)&msg, sizeof(message), 0, (sockaddr*)&p->client, sizeof(p->client));

					p->wantsRematch = -1;
					p->enemy->wantsRematch = -1;
				}
			}
			else if (received.data[0] == 'n' || received.data[0] == 'N')
			{
				bool playerFound = false;
				Player* p = SearchPlayer(client.sin_port, playerFound);
				
				message msg;
				msg.cmd = (byte)ServerMessage::BlockInput;
				
				string s = "Tu oponente a rechazado la revancha, espera otro contrincante";
				StringToChar(s, msg.data);
				ResetBoard(games[p->gameID]);
				p->wantsRematch = 0;

				if (p->enemy != nullptr)
				{
					sendto(listening, (char*)&msg, sizeof(message), 0, (sockaddr*)&p->enemy->client, sizeof(p->enemy->client));

					p->enemy->num = 0;
					games[p->gameID]->p[0] = p->enemy;
					games[p->gameID]->p[1] = nullptr;
					p->enemy->enemy = nullptr;
					Player* aux = p->enemy;
					if (p->enemy->wantsRematch != -1)
						CheckAvailablePlayer(aux);
					p->enemy = nullptr;
					delete p;
				}
				else if (p->enemy == nullptr)
				{
					vector<Game*>::iterator iter = games.begin();
					games.erase(iter + p->gameID);
				}
			}
		}
		break;
		}
	}

	closesocket(listening);

	//cleanup winsock
	WSACleanup();
	return 0;
}
