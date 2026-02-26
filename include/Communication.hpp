#ifndef COMMUNICATION_HPP
#define COMMUNICATION_HPP

#include <cstring>

/**
 * @file Communication.hpp
 * @brief Define estruturas de comunicação entre processos (gerador e
 * visualizador).
 *
 * Usa memória compartilhada POSIX para transferência de amostras e um FIFO
 * para passagem de comandos.
 */

// Constantes de configuração
constexpr int SAMPLES_PER_FRAME =
    1000;  ///< Número de amostras geradas por quadro
constexpr int FRAME_INTERVAL_MS = 50;  ///< Intervalo entre quadros (20 fps)
constexpr double MAX_AMPLITUDE = 1.0;  ///< Amplitude máxima normalizada

constexpr int BUFFER_SIZE =
    16384;  ///< Tamanho do buffer circular (potência de 2 para wrap)
const char* const FIFO_COMMAND =
    "/tmp/sine_commands";  ///< Pipe nomeado para comandos
const char* const SHARED_MEMORY_NAME =
    "/sine_buffer";  ///< Nome do objeto de memória compartilhada

// Tipos de comando enviados do controlador para o gerador
enum CommandType {
  CMD_NONE,      ///< Nenhum comando
  CMD_START,     ///< Iniciar geração
  CMD_STOP,      ///< Parar geração
  CMD_SET_FREQ,  ///< Ajustar frequência (value = frequência em Hz)
  CMD_SET_AMP,   ///< Ajustar amplitude (value = amplitude)
  CMD_QUIT       ///< Encerrar processo gerador
};

struct Command {
  CommandType type;  ///< Tipo do comando
  double value;      ///< Parâmetro associado (quando aplicável)

  Command() : type(CMD_NONE), value(0.0) {}
  Command(CommandType t, double v = 0.0) : type(t), value(v) {}
};

/**
 * @struct SharedBuffer
 * @brief Buffer circular em memória compartilhada para transferência de
 * amostras.
 *
 * O gerador escreve amostras na posição writePos, o visualizador lê de readPos.
 * newDataAvailable indica que novos dados foram escritos após um quadro
 * completo.
 */
struct SharedBuffer {
  double samples[BUFFER_SIZE];  ///< Amostras no buffer circular
  int writePos;                 ///< Posição atual de escrita (produtor)
  int readPos;                  ///< Posição atual de leitura (consumidor)
  bool newDataAvailable;        ///< Sinaliza novo quadro disponível
  int totalProduced;            ///< Total de amostras produzidas (estatística)

  SharedBuffer()
      : writePos(0), readPos(0), newDataAvailable(false), totalProduced(0) {
    memset(samples, 0, sizeof(samples));
  }
};

#endif  // COMMUNICATION_HPP
