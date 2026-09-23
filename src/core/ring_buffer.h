#ifndef MUS_CORE_RING_BUFFER_H
#define MUS_CORE_RING_BUFFER_H

#include <atomic>
#include <cstddef>
#include <type_traits>
#include <vector>

namespace mus {

// Fila circular Single-Producer/Single-Consumer sem lock — a "moeda de
// troca" entre quem calcula os SoundEvent (thread principal, ao interpretar
// a AST) e a thread de áudio, que só pode consumi-los (SCH-03, #17). push()
// e pop() nunca bloqueiam e nunca alocam: o buffer inteiro é alocado uma
// única vez, na construção — antes da thread de áudio começar a rodar. É
// essa garantia que atende ao NFR da issue: nada de malloc/new ou mutex
// disparado de dentro da thread de áudio.
//
// SPSC, não MPMC: só é seguro chamar push() de uma única thread produtora e
// pop() de uma única thread consumidora ao mesmo tempo. Duas threads
// chamando push() entre si (ou duas chamando pop() entre si) é uma corrida
// de dados — os índices atômicos só sincronizam produtor com consumidor,
// nunca vários produtores ou consumidores entre si.
template <typename T>
class SpscRingBuffer {
public:
    static_assert(std::is_trivially_copyable<T>::value,
                  "SpscRingBuffer só aceita tipos trivialmente copiáveis: um "
                  "construtor de cópia não-trivial poderia alocar memória "
                  "escondida dentro da thread de áudio, violando o NFR da "
                  "SCH-03 (#17). É a mesma garantia que SoundEvent já promete "
                  "em core/sound_event.h.");

    // `capacity` é o número de itens que cabem simultaneamente no buffer.
    // Internamente aloca capacity+1 posições: o slot extra é o que permite
    // distinguir buffer cheio de buffer vazio só com os dois índices, sem um
    // contador atômico à parte — que seria mais uma variável disputada entre
    // produtor e consumidor.
    explicit SpscRingBuffer(std::size_t capacity) : buffer_(capacity + 1) {}

    SpscRingBuffer(const SpscRingBuffer&) = delete;
    SpscRingBuffer& operator=(const SpscRingBuffer&) = delete;

    // Só a thread produtora chama. Devolve false (sem escrever nada) se o
    // buffer estiver cheio — quem chama decide o que fazer com o evento
    // descartado (dropar é uma escolha válida em áudio: travar não é).
    bool push(const T& item) {
        const std::size_t writeIndex = writeIndex_.load(std::memory_order_relaxed);
        const std::size_t nextWriteIndex = advance(writeIndex);

        // acquire: sincroniza com o release de pop(), garantindo que o slot
        // prestes a ser sobrescrito já não está sendo lido pelo consumidor.
        if (nextWriteIndex == readIndex_.load(std::memory_order_acquire)) {
            return false;  // cheio
        }

        buffer_[writeIndex] = item;

        // release: publica o item para o consumidor — só depois deste store
        // é seguro pop() enxergar esta posição como disponível.
        writeIndex_.store(nextWriteIndex, std::memory_order_release);
        return true;
    }

    // Só a thread consumidora chama. Devolve false (sem tocar em `*item`) se
    // o buffer estiver vazio.
    bool pop(T* item) {
        const std::size_t readIndex = readIndex_.load(std::memory_order_relaxed);

        // acquire: sincroniza com o release de push(), garantindo que o item
        // já está totalmente escrito antes de lermos.
        if (readIndex == writeIndex_.load(std::memory_order_acquire)) {
            return false;  // vazio
        }

        *item = buffer_[readIndex];

        // release: publica a posição de volta para o produtor reutilizar.
        readIndex_.store(advance(readIndex), std::memory_order_release);
        return true;
    }

    // Quantos itens cabem simultaneamente — não conta o slot de folga interno.
    std::size_t capacity() const { return buffer_.size() - 1; }

private:
    std::size_t advance(std::size_t index) const {
        return (index + 1) % buffer_.size();
    }

    std::vector<T> buffer_;
    std::atomic<std::size_t> writeIndex_{0};
    std::atomic<std::size_t> readIndex_{0};
};

}  // namespace mus

#endif  // MUS_CORE_RING_BUFFER_H
