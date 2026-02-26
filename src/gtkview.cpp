// viewer.cpp - Visualizador de Onda Senoidal Contínua
// Exibe uma onda senoidal rolante a partir da memória compartilhada.

#include <fcntl.h>
#include <glibmm.h>
#include <gtkmm.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <deque>

#include "../include/Communication.hpp"

// Configuração da janela
const int WINDOW_WIDTH = 800;   // Largura da janela em pixels
const int WINDOW_HEIGHT = 400;  // Altura da janela em pixels
const int MAX_DISPLAY_POINTS =
    800;  // Número máximo de amostras exibidas na tela
const int UI_UPDATE_INTERVAL_MS =
    30;  // Intervalo de atualização da interface (~33 fps)

// Agrupa os dados compartilhados entre a thread de leitura e a thread principal
struct ViewerContext {
  SharedBuffer* shmBuffer;  // Ponteiro para o buffer de memória compartilhada
  std::deque<double> sampleHistory;  // Histórico de amostras para exibição
  bool running;                      // Flag de controle da thread de leitura
};
