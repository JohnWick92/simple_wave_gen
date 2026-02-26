#ifndef ISIGNAL_GENERATOR_HPP
#define ISIGNAL_GENERATOR_HPP

#include <memory>
#include <string>
#include <vector>

// Interface genérica para qualquer fonte de sinal (gerador sintético, leitor de
// arquivo, stream, etc.)
class ISignalGenerator {
 public:
  ISignalGenerator() = default;
  virtual ~ISignalGenerator() = default;

  // Gera e retorna um bloco de `count` amostras.
  virtual std::vector<double> generateSamples(size_t count) = 0;

  // Define um parâmetro nomeado (ex.: "frequency", "amplitude", "filename").
  virtual void setParameter(const std::string& name, double value) = 0;

  virtual void start() = 0;
  virtual void stop() = 0;

  // Não copiável (base polimórfica).
  ISignalGenerator(const ISignalGenerator&) = delete;
  ISignalGenerator& operator=(const ISignalGenerator&) = delete;
};

// Alias de conveniência.
using GeneratorPtr = std::unique_ptr<ISignalGenerator>;

#endif
