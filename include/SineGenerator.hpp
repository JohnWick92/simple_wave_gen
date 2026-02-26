#ifndef SINE_GENERATOR_HPP
#define SINE_GENERATOR_HPP

#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>

#include "ISignalGenerator.hpp"

class SineGenerator : public ISignalGenerator {
 private:
  // Parâmetros atômicos para acesso thread-safe entre UI e áudio
  std::atomic<double> m_frequency;  // Frequência da onda em Hz
  std::atomic<double> m_amplitude;  // Amplitude (0.0 a 1.0)
  std::atomic<bool> m_running;      // Se a thread de áudio está rodando
  double m_phase;  // Fase acumulada (não atômica, só acessada pela áudio)

  // Constantes para controle visual da onda
  static constexpr double BASE_FREQUENCY =
      100.0;  // Frequência de referência para cálculos
  static constexpr double BASE_CYCLES =
      2.0;  // Ciclos na tela na frequência base
  static constexpr double BASE_VELOCITY = 0.005;  // Velocidade base de rolagem
  static constexpr double MIN_FREQ = 1.0;      // Limite inferior de frequência
  static constexpr double MAX_FREQ = 22000.0;  // Limite superior de frequência
  static constexpr double MIN_CYCLES =
      0.5;  // Mínimo de ciclos na tela (zoom máximo)
  static constexpr double MAX_CYCLES =
      12.0;  // Máximo de ciclos na tela (zoom mínimo)

  // Parâmetros visuais calculados dinamicamente
  double m_displayCycles;  // Quantos ciclos da onda aparecem na tela (varia com
                           // frequência)
  double m_currentZoom;    // Fator de zoom relativo à base (calculado de
                           // displayCycles)

  /**
   * Mapeia a frequência (escala log) para número de ciclos na tela (linear)
   *
   * Objetivo: Em baixas frequências queremos ver poucos ciclos (zoom in)
   * Em altas frequências queremos ver muitos ciclos (zoom out)
   * Isso mantém a forma de onda sempre visível e legível
   *
   * Processo:
   * 1. Converte frequência para escala log (percepção humana é logarítmica)
   * 2. Calcula onde a frequência está entre min e max (ratio 0..1)
   * 3. Mapeia ratio linearmente entre min_cycles e max_cycles
   * 4. Calcula zoom como fator relativo à base
   */
  void updateDisplayParameters() {
    double freq = m_frequency.load();

    // Log da frequência para mapeamento perceptual
    double logFreq = log10(freq);
    double logMin = log10(MIN_FREQ);
    double logMax = log10(MAX_FREQ);

    // Posição relativa na faixa de frequências (0 = mínima, 1 = máxima)
    double ratio = std::clamp((logFreq - logMin) / (logMax - logMin), 0.0, 1.0);

    // Interpola linearmente entre mínimo e máximo de ciclos
    m_displayCycles = MIN_CYCLES + ratio * (MAX_CYCLES - MIN_CYCLES);

    // Calcula zoom: >1 = mais zoom, <1 = menos zoom
    m_currentZoom = std::clamp(BASE_CYCLES / m_displayCycles, 0.5, 2.0);
  }

 public:
  explicit SineGenerator(double /*sampleRate*/ = 44100.0,
                         double frequency = 100.0, double amplitude = 0.8)
      : m_frequency(frequency),
        m_amplitude(std::clamp(amplitude, 0.0, 1.0)),
        m_running(false),
        m_phase(0.0),
        m_displayCycles(BASE_CYCLES),
        m_currentZoom(1.0) {
    updateDisplayParameters();
  }

  void start() override { m_running = true; }
  void stop() override { m_running = false; }

  /**
   * @brief Gera um bloco de amostras da onda senoidal contínua.
   *
   * O algoritmo implementa um oscilador de fase acumulada (phase accumulator)
   * para sintetizar uma onda senoidal contínua sem descontinuidades entre
   * blocos.
   *
   * Funcionamento matemático:
   *
   * 1. Relação fundamental: Uma onda senoidal contínua é definida por:
   *    y(t) = A * sin(2π * f * t + φ)
   *
   *    Onde:
   *    - A = amplitude
   *    - f = frequência (Hz)
   *    - t = tempo (segundos)
   *    - φ = fase inicial
   *
   * 2. No domínio discreto (amostras), o tempo é t = n / sampleRate:
   *    y[n] = A * sin(2π * f * n / sampleRate + φ)
   *
   * 3. Fase acumulada: Em vez de calcular 2π*f*n/sampleRate para cada amostra,
   *    mantém-se um acumulador de fase θ que é incrementado linearmente:
   *    θ[n] = θ[n-1] + Δθ
   *
   *    Onde Δθ = 2π * f / sampleRate (incremento de fase por amostra)
   *
   * 4. Implementação atual - separação em duas escalas:
   *
   *    a) Incremento rápido (phaseStep) para os ciclos na tela:
   *       phaseStep = 2π * displayCycles / count
   *
   *       Isto NÃO segue a fórmula tradicional 2π*f/sampleRate!
   *       Em vez disso, displayCycles é uma abstração visual que varia com f:
   *       - displayCycles = f(f) mapeado em escala log
   *       - Garante que a forma de onda seja sempre visível na tela
   *
   *    b) Fase base (m_phase) para continuidade entre blocos:
   *       m_phase[n+1] = m_phase[n] + velocity
   *
   *       Onde velocity ~ 2π * f / frameRate, mas com fator logarítmico
   *       para controle de rolagem visual
   *
   * 5. Geração contínua:
   *    currentPhase = m_phase + i * phaseStep
   *    sample[i] = amplitude * sin(currentPhase)
   *
   *    Isto garante que:
   *    - A fase não "reseta" entre blocos (evita clicks)
   *    - A frequência instantânea é mantida
   *    - O scrolling visual é independente da taxa de amostragem
   *
   * 6. Tratamento de wrap-around:
   *    m_phase = fmod(m_phase, 2π) evita overflow numérico
   *    mantendo a continuidade da fase
   *
   * O resultado é uma onda matematicamente contínua através dos blocos,
   * com parâmetros visuais ajustáveis independentemente da geração do áudio.
   */
  std::vector<double> generateSamples(size_t count) override {
    std::vector<double> samples;
    if (!m_running) return samples;

    updateDisplayParameters();

    // Fase total para percorrer displayCycles ciclos na tela
    double totalPhase = 2.0 * M_PI * m_displayCycles;

    // Incremento de fase por amostra (taxa de variação da fase)
    double phaseStep = totalPhase / count;

    // Fase corrente = fase acumulada global + avanço intra-bloco
    double currentPhase = m_phase;

    // Gera amostras aplicando seno à fase acumulada
    for (size_t i = 0; i < count; ++i) {
      samples.push_back(m_amplitude * std::sin(currentPhase));
      currentPhase += phaseStep;  // Avança fase para próxima amostra
    }

    // Avança fase global para próximo bloco (garante continuidade)
    double freq = m_frequency.load();
    double velocity =
        BASE_VELOCITY * (1.0 + log10(freq / BASE_FREQUENCY + 1.0));
    velocity = std::min(velocity, 0.03);
    m_phase += velocity;

    // Wrap-around para evitar perda de precisão
    while (m_phase > 2.0 * M_PI) m_phase -= 2.0 * M_PI;

    return samples;
  }

  void setParameter(const std::string& name, double value) override {
    if (name == "frequency") {
      value = std::clamp(value, MIN_FREQ, MAX_FREQ);
      m_frequency = value;
      updateDisplayParameters();
    } else if (name == "amplitude") {
      m_amplitude = std::clamp(value, 0.0, 1.0);
    }
  }

  bool isActive() const override { return m_running; }

  // Public getters (usados pelo controller)
  double getFrequency() const { return m_frequency; }
  double getAmplitude() const { return m_amplitude; }
  double getPhase() const { return m_phase; }
  double getDisplayCycles() const { return m_displayCycles; }
  double getCurrentZoom() const { return m_currentZoom; }
  bool isRunning() const { return m_running; }

  void resetPhase() { m_phase = 0.0; }
};

#endif  // SINE_GENERATOR_HPP
