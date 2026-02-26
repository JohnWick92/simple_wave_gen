// viewer.cpp - Visualizador de Onda Senoidal Contínua
// Exibe uma onda senoidal rolante a partir da memória compartilhada.

#include <fcntl.h>
#include <glibmm.h>
#include <gtkmm.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <deque>
#include <thread>

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

// Área de desenho customizada que renderiza a forma de onda
class WaveformCanvas : public Gtk::DrawingArea {
 public:
  WaveformCanvas() {
    set_content_width(WINDOW_WIDTH);
    set_content_height(WINDOW_HEIGHT);
    set_draw_func(sigc::mem_fun(*this, &WaveformCanvas::onDraw));
  }

  // Chamado pela UI thread quando novas amostras estão disponíveis
  void updateSamples(const std::deque<double>& samples) {
    m_samples = samples;
    queue_draw();  // Solicita redesenho do canvas
  }

 private:
  std::deque<double> m_samples;  // Cópia local das amostras para renderização

  // Função de desenho chamada pelo GTK quando o canvas precisa ser renderizado
  void onDraw(const Cairo::RefPtr<Cairo::Context>& cr, int width, int height) {
    // Fundo preto
    cr->set_source_rgb(0, 0, 0);
    cr->paint();

    // Se não há dados, exibe mensagem de espera
    if (m_samples.size() < 2) {
      cr->set_source_rgb(0, 1, 0);
      auto layout = create_pango_layout("Aguardando sinal...");
      layout->set_font_description(Pango::FontDescription("Monospace 12"));
      int tw, th;
      layout->get_pixel_size(tw, th);
      cr->move_to(static_cast<double>(width - tw) / 2,
                  static_cast<double>(height - th) / 2);
      layout->show_in_cairo_context(cr);
      return;
    }

    // Desenha grade de referência (linhas horizontais e verticais)
    cr->set_source_rgba(0, 0.5, 0, 0.2);  // Verde transparente
    cr->set_line_width(0.5);

    // Linha central (zero)
    int centerY = height / 2;
    cr->move_to(20, centerY);
    cr->line_to(width - 20, centerY);
    cr->stroke();

    // Linhas horizontais de referência (amplitudes ±1/3 e ±2/3)
    for (int i = -2; i <= 2; i += 2) {
      double y = centerY + i * (static_cast<double>(height) / 6);
      cr->move_to(20, y);
      cr->line_to(width - 20, y);
      cr->stroke();
    }

    // Linhas verticais de referência (divisões de tempo)
    for (int i = 0; i <= 4; i++) {
      double x = 20 + i * static_cast<double>(width - 40) / 4;
      cr->move_to(x, 20);
      cr->line_to(x, height - 20);
      cr->stroke();
    }

    // Desenha a forma de onda
    cr->set_source_rgb(0, 1, 0);  // Verde brilhante
    cr->set_line_width(2);

    size_t n = m_samples.size();
    double stepX =
        (double)(width - 40) / (n - 1);  // Espaçamento horizontal entre pontos
    double verticalScale =
        (height - 60) / 2.0;  // Escala vertical (reserva margem de 30px)

    // Conecta os pontos com linhas
    for (size_t i = 0; i < n - 1; ++i) {
      double x1 = 20 + i * stepX;
      double y1 = centerY - m_samples[i] * verticalScale;
      double x2 = 20 + (i + 1) * stepX;
      double y2 = centerY - m_samples[i + 1] * verticalScale;

      // Garante que as linhas não ultrapassem as margens
      y1 = std::clamp(y1, 20.0, (double)(height - 20));
      y2 = std::clamp(y2, 20.0, (double)(height - 20));

      cr->move_to(x1, y1);
      cr->line_to(x2, y2);
      cr->stroke();
    }
  }
};

// Janela principal da aplicação
class ViewerWindow : public Gtk::Window {
 public:
  ViewerWindow(SharedBuffer* buffer) : m_ctx{buffer, {}, true} {
    set_title("Visualizador de Onda Senoidal");
    set_default_size(WINDOW_WIDTH, WINDOW_HEIGHT);
    set_child(m_canvas);

    // Inicia thread que lê dados da memória compartilhada
    m_readerThread = std::thread(&ViewerWindow::readerThreadFunc, this);

    // Configura timer para atualizar a UI periodicamente
    Glib::signal_timeout().connect(
        sigc::mem_fun(*this, &ViewerWindow::updateWaveform),
        UI_UPDATE_INTERVAL_MS);
  }

  ~ViewerWindow() {
    m_ctx.running = false;  // Sinaliza para thread de leitura parar
    if (m_readerThread.joinable()) m_readerThread.join();
  }

 private:
  ViewerContext m_ctx;         // Contexto compartilhado com a thread
  WaveformCanvas m_canvas;     // Área de desenho da onda
  std::thread m_readerThread;  // Thread para leitura de dados

  // Executa em thread separada: lê novos dados da memória compartilhada
  void readerThreadFunc() {
    int lastWritePos = -1;

    while (m_ctx.running) {
      int currentWrite = m_ctx.shmBuffer->writePos;

      // Verifica se há novos dados disponíveis
      if (lastWritePos != currentWrite && m_ctx.shmBuffer->newDataAvailable) {
        int readPos = m_ctx.shmBuffer->readPos;

        // Lê todas as amostras novas do buffer circular
        while (readPos != currentWrite) {
          double sample = m_ctx.shmBuffer->samples[readPos];
          readPos = (readPos + 1) % BUFFER_SIZE;

          // Adiciona ao histórico e mantém tamanho máximo
          m_ctx.sampleHistory.push_back(sample);
          if (m_ctx.sampleHistory.size() > MAX_DISPLAY_POINTS) {
            m_ctx.sampleHistory.pop_front();
          }
        }

        // Atualiza posição de leitura e limpa flag de novo dado
        m_ctx.shmBuffer->readPos = readPos;
        m_ctx.shmBuffer->newDataAvailable = false;
        lastWritePos = currentWrite;
      }

      // Pequena pausa para evitar uso excessivo de CPU
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
  }

  // Chamado pelo timer da UI: atualiza o canvas com novas amostras
  bool updateWaveform() {
    m_canvas.updateSamples(m_ctx.sampleHistory);
    return true;  // Mantém o timer ativo
  }
};
