#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <iostream>

#include "../include/Communication.hpp"
#include "../include/SineGenerator.hpp"

static volatile bool keepRunning = true;

void signalHandler(int) { keepRunning = false; }

int main() {
  signal(SIGINT, signalHandler);
  signal(SIGPIPE, SIG_IGN);

  std::cout << "\n[GENERATOR] Started (PID: " << getpid() << ")\n" << std::endl;

  // Cria gerador senoidal - frequência inicial 100 Hz, amplitude 0.8
  SineGenerator generator(/*sampleRate*/ 1.0, 100.0, 0.8);

  // Cria FIFO para receber comandos
  unlink(FIFO_COMMAND);
  if (mkfifo(FIFO_COMMAND, 0666) < 0) {
    std::cerr << "[GENERATOR] Failed to create FIFO" << std::endl;
    return 1;
  }

  // Abre FIFO em modo não-bloqueante para leitura
  int cmdFd = open(FIFO_COMMAND, O_RDONLY | O_NONBLOCK);
  if (cmdFd < 0) {
    std::cerr << "[GENERATOR] Failed to open FIFO" << std::endl;
    return 1;
  }

  // Cria memória compartilhada para transferência de amostras
  shm_unlink(SHARED_MEMORY_NAME);
  int shmFd = shm_open(SHARED_MEMORY_NAME, O_CREAT | O_RDWR, 0666);
  if (shmFd < 0) {
    std::cerr << "[GENERATOR] Failed to create shared memory" << std::endl;
    close(cmdFd);
    return 1;
  }

  // Ajusta tamanho da memória compartilhada
  if (ftruncate(shmFd, sizeof(SharedBuffer)) < 0) {
    std::cerr << "[GENERATOR] Failed to set shared memory size" << std::endl;
    close(cmdFd);
    close(shmFd);
    return 1;
  }

  // Mapeia memória compartilhada no espaço de endereço do processo
  SharedBuffer* buffer =
      (SharedBuffer*)mmap(nullptr, sizeof(SharedBuffer), PROT_READ | PROT_WRITE,
                          MAP_SHARED, shmFd, 0);

  if (buffer == MAP_FAILED) {
    std::cerr << "[GENERATOR] Failed to map shared memory" << std::endl;
    close(cmdFd);
    close(shmFd);
    return 1;
  }

  // Inicializa o buffer (garante membros zerados)
  new (buffer) SharedBuffer();

  std::cout << "[GENERATOR] Ready. Waiting for commands...\n" << std::endl;

  Command cmd;
  auto lastFrameTime = std::chrono::steady_clock::now();

  while (keepRunning) {
    // Lê comandos recebidos (não-bloqueante)
    ssize_t bytesRead = read(cmdFd, &cmd, sizeof(Command));
    if (bytesRead == sizeof(Command)) {
      switch (cmd.type) {
        case CMD_START:
          generator.start();
          break;
        case CMD_STOP:
          generator.stop();
          break;
        case CMD_SET_FREQ:
          generator.setParameter("frequency", cmd.value);
          break;
        case CMD_SET_AMP:
          generator.setParameter("amplitude", cmd.value);
          break;
        case CMD_QUIT:
          keepRunning = false;
          break;
        default:
          break;
      }
    }

    // Gera novo quadro em intervalos fixos
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - lastFrameTime);

    if (elapsed.count() >= FRAME_INTERVAL_MS) {
      // Gera amostras apenas se o gerador estiver ativo
      if (generator.isRunning()) {
        auto samples = generator.generateSamples(SAMPLES_PER_FRAME);
        if (!samples.empty()) {
          for (double sample : samples) {
            buffer->samples[buffer->writePos] = sample;
            buffer->writePos = (buffer->writePos + 1) % BUFFER_SIZE;
            buffer->totalProduced++;

            // Se buffer cheio, avança readPos (descarta o mais antigo)
            if (buffer->writePos == buffer->readPos) {
              buffer->readPos = (buffer->readPos + 1) % BUFFER_SIZE;
            }
          }
          buffer->newDataAvailable = true;
        }
      }
      lastFrameTime = now;
    }
  }

  // Limpeza
  close(cmdFd);
  munmap(buffer, sizeof(SharedBuffer));
  close(shmFd);
  unlink(FIFO_COMMAND);

  std::cout << "\n[GENERATOR] Shut down" << std::endl;
  return 0;
}
