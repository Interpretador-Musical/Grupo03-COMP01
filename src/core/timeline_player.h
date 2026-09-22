#ifndef MUS_CORE_TIMELINE_PLAYER_H
#define MUS_CORE_TIMELINE_PLAYER_H

#include <cstdint>
#include <vector>

#include "core/sound_event.h"

namespace mus {

// Executa uma Timeline já compilada amostra a amostra — a etapa de "rodar o
// programa" da INT-03 (#19), separada da etapa de "compilar" (o
// Interpretador, que só produz a lista de SoundEvent).
//
// Não sabe nada de dispositivo de áudio, thread ou I/O: quem o possui (ex.
// AudioEngine) chama render() de dentro do próprio callback de áudio
// real-time, uma vez por bloco de frames. Isso é deliberado — nada de
// alocação, lock ou I/O aqui dentro, pela mesma regra que já vale para
// AudioEngine::renderSine.
class TimelinePlayer {
public:
    explicit TimelinePlayer(std::uint32_t sampleRate) : sampleRate_(sampleRate) {}

    // Troca o programa a ser executado. Só é seguro chamar fora de uma
    // renderização em andamento (thread de áudio parada) — não há
    // sincronização para trocar o vetor com render() rodando ao mesmo tempo.
    // Espera `eventos` já ordenado por startTime (Timeline::sortByStartTime);
    // não reordena por conta própria.
    void load(std::vector<SoundEvent> eventos);

    // Instante em que o último evento carregado termina de soar; 0 se load()
    // não recebeu nenhum evento.
    double totalDuration() const;

    // Sintetiza `frameCount` amostras mono a partir de onde parou, avançando
    // o cursor interno pelos eventos e a fase da senoide. Fora dos limites da
    // Timeline (antes do primeiro evento por causa de um gap, ou depois do
    // último): silêncio, nunca acesso fora do vetor — inclusive se chamado
    // repetidas vezes após o fim.
    void render(float* output, std::uint32_t frameCount);

private:
    std::uint32_t sampleRate_;
    std::vector<SoundEvent> eventos_;

    // Só tocados de dentro de render(), sempre na mesma thread (a de
    // áudio) — não precisam ser atômicos.
    std::size_t cursor_ = 0;
    std::uint64_t framesRenderizados_ = 0;
    double fase_ = 0.0;
};

}  // namespace mus

#endif  // MUS_CORE_TIMELINE_PLAYER_H
