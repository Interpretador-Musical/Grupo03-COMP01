#ifndef MUS_AUDIO_LIMITER_H
#define MUS_AUDIO_LIMITER_H

#include <cmath>
#include <cstdint>

namespace mus {

// Limitador do master bus — DSP-10 (#57). Última etapa antes do dispositivo:
// nenhuma amostra sai com módulo acima de 1.0, venha de onde vier — acorde
// grande, `volume 9999`, ganho absurdo no --test-tone.
//
// Até aqui o único limite era um corte seco em ±1.0 no `renderRealTime`, e o
// caminho do `TimelinePlayer` não tinha nenhum. Corte seco transforma o pico
// em quina, e quina no sinal é harmônico agudo que o ouvido escuta como
// estalo. Aqui a curva entra suave:
//
// - Até `kSoftClipThreshold` a amostra passa intacta. O sinal normal não é
//   colorido pelo limitador.
// - Acima dele, a sobra é comprimida por tanh e se aproxima de 1.0 sem nunca
//   passar. A derivada vale 1 dos dois lados do limiar, então a curva não tem
//   quina nem ali.
//
// Header-only e sem estado: roda dentro do callback de áudio, onde não se pode
// alocar nem bloquear.
constexpr float kSoftClipThreshold = 0.8f;

inline float softClip(float amostra) {
    // NaN passaria por toda a conta e chegaria à placa como ruído. Infinito
    // não precisa de caso: tanh(inf) é 1.
    if (std::isnan(amostra)) {
        return 0.0f;
    }

    const float modulo = std::fabs(amostra);
    if (modulo <= kSoftClipThreshold) {
        return amostra;
    }

    const float folga = 1.0f - kSoftClipThreshold;
    const float comprimido =
        kSoftClipThreshold + folga * std::tanh((modulo - kSoftClipThreshold) / folga);
    return std::copysign(comprimido, amostra);
}

inline void softClipBuffer(float* amostras, std::uint32_t quantidade) {
    for (std::uint32_t i = 0; i < quantidade; ++i) {
        amostras[i] = softClip(amostras[i]);
    }
}

// Limitador de pico do master bus — BUG-AUDIO (#109). Fica antes do softClip.
//
// O softClip sozinho é uma curva sem memória: cada amostra é dobrada pela
// mesma tanh, não importa o que veio antes. Isso basta para um pico ocasional,
// mas o caso normal do projeto não é ocasional. O volume vai até 1 por nota, e
// as vozes se somam sem atenuação nenhuma. Um acorde de três notas no volume
// padrão chega a 3.0, e 39% das amostras saem da tanh achatadas perto de 1.0.
// A senoide vira quase uma onda quadrada, e os harmônicos agudos dessa quina
// são o chiado da #109 (medições em docs/chiados.md).
//
// Este limitador não mexe na forma da onda: ele baixa o ganho da mixagem
// inteira. Um detector segue o pico do sinal, e o ganho é o que traz esse pico
// para `kLimitadorTeto`. Como o ganho muda devagar perto do período das
// notas, a senoide sai inteira, só mais baixa.
//
// - Detector com ataque instantâneo: se a amostra passa do pico guardado, o
//   pico passa a ser ela.
// - Ganho com ataque de 1 ms: o ganho desce até o alvo (teto / pico) por uma
//   exponencial de 1 ms, e não num degrau. Num acorde, os pares de frequências
//   não têm razão exata, e o pico da soma oscila devagar. Cada pico novo
//   baixaria o ganho de uma vez, e esse degrau no meio da onda também gera
//   agudo. Medido no acorde de volume 0.33 da docs/chiados.md: com o degrau,
//   a energia acima de 4 kHz ficava uns 15 dB acima da do softClip sozinho.
//   O preço é um estouro curto acima do teto enquanto o ganho desce. O
//   softClip logo atrás dobra esse estouro, e ele só acontece quando o nível
//   sobe. Um look-ahead evitaria o estouro, mas atrasaria a saída inteira.
// - Retenção de 50 ms: o pico guardado não cai entre dois ciclos da mesma
//   nota. O dó1 (32,7 Hz) tem período de 31 ms. Sem a retenção, o ganho
//   subiria e desceria a cada ciclo de um grave, e essa modulação também é
//   distorção.
// - Recuperação exponencial de 150 ms: depois da retenção, o ganho volta a 1
//   devagar, para que a saída de uma nota não pareça um degrau de volume.
//
// O pico guardado nunca desce abaixo do teto. Abaixo dele o ganho é
// exatamente 1 e o sinal passa intacto, bit a bit. Isso também impede que o detector decaia
// até números subnormais no silêncio, que são lentos em x86.
//
// O teto é o próprio limiar do softClip. Com o ganho já ajustado, nenhuma
// amostra de um som sustentado chega à parte curva da tanh. Só chega lá o
// estouro do ataque. O softClip continua no fim da cadeia como rede de
// segurança: dobra esse estouro, troca NaN por silêncio e infinito por 1.0. O
// limitador deixa NaN e infinito passarem sem tocar no próprio estado.
//
// Sem alocação nem bloqueio: roda dentro do callback de áudio.
constexpr float kLimitadorTeto = kSoftClipThreshold;
constexpr double kLimitadorAtaqueSegundos = 0.001;
constexpr double kLimitadorRetencaoSegundos = 0.050;
constexpr double kLimitadorRecuperacaoSegundos = 0.150;
constexpr float kLimitadorFracaoDeRenovacao = 0.98f;

class LimitadorDePico {
public:
    explicit LimitadorDePico(std::uint32_t taxaDeAmostragem)
        : framesDeRetencao_(static_cast<std::uint32_t>(
              kLimitadorRetencaoSegundos * static_cast<double>(taxaDeAmostragem))),
          coeficienteDeAtaque_(static_cast<float>(std::exp(
              -1.0 / (kLimitadorAtaqueSegundos *
                      static_cast<double>(taxaDeAmostragem))))),
          coeficienteDeRecuperacao_(static_cast<float>(std::exp(
              -1.0 / (kLimitadorRecuperacaoSegundos *
                      static_cast<double>(taxaDeAmostragem))))) {}

    // Volta ao estado inicial, sem redução de ganho. Chamar fora do callback,
    // com o dispositivo parado.
    void reiniciar() {
        pico_ = kLimitadorTeto;
        ganho_ = 1.0f;
        retencao_ = 0;
    }

    float processar(float amostra) {
        // NaN e infinito ficam com o softClip. Atualizar o pico com eles
        // deixaria o ganho em zero (teto / infinito) até a reprodução acabar.
        if (!std::isfinite(amostra)) {
            return amostra;
        }

        if (retencao_ > 0) {
            --retencao_;
        } else if (pico_ > kLimitadorTeto) {
            pico_ = std::fmax(pico_ * coeficienteDeRecuperacao_, kLimitadorTeto);
        }

        // Renova a retenção também quando a amostra só chega perto do pico. Num
        // tom sustentado, o pico de cada ciclo é o mesmo do anterior, não
        // maior. Sem esta renovação, a retenção venceria a cada 50 ms, o ganho
        // subiria um pouco e voltaria de uma vez no ciclo seguinte. Medido com
        // uma senoide de amplitude 2: sem a renovação, o ganho ondulava 6% no
        // dó1 e 1% no dó4.
        //
        // A renovação só vale acima do teto. Sem essa condição, depois de um
        // trecho alto, um sinal entre 98% do teto e o teto renovaria a retenção
        // a cada ciclo e prenderia o ganho em até 0.98, sem nunca voltar a 1.
        const float modulo = std::fabs(amostra);
        if (modulo > pico_) {
            pico_ = modulo;
            retencao_ = framesDeRetencao_;
        } else if (modulo > kLimitadorTeto &&
                   modulo >= pico_ * kLimitadorFracaoDeRenovacao) {
            retencao_ = framesDeRetencao_;
        }

        // Descida suave e subida direta: na subida, quem já suaviza é a
        // recuperação exponencial do pico. Com o pico no teto, o alvo é 1 e o
        // ganho também. Aí a amostra sai idêntica, sem a multiplicação.
        const float alvo = pico_ > kLimitadorTeto ? kLimitadorTeto / pico_ : 1.0f;
        if (alvo < ganho_) {
            ganho_ = alvo + (ganho_ - alvo) * coeficienteDeAtaque_;
        } else {
            ganho_ = alvo;
        }
        return ganho_ < 1.0f ? amostra * ganho_ : amostra;
    }

    void processarBuffer(float* amostras, std::uint32_t quantidade) {
        for (std::uint32_t i = 0; i < quantidade; ++i) {
            amostras[i] = processar(amostras[i]);
        }
    }

    // Ganho aplicado na última amostra, entre 0 e 1. Para testes.
    float ganho() const { return ganho_; }

private:
    std::uint32_t framesDeRetencao_;
    float coeficienteDeAtaque_;
    float coeficienteDeRecuperacao_;
    float pico_ = kLimitadorTeto;
    float ganho_ = 1.0f;
    std::uint32_t retencao_ = 0;
};

}  // namespace mus

#endif  // MUS_AUDIO_LIMITER_H
