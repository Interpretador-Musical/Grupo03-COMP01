#ifndef MUS_INTERPRETER_ARENA_H
#define MUS_INTERPRETER_ARENA_H

/**
 * @file arena.h
 * @brief Alocador em arena para os nós da AST (MAT-10, #55).
 */

#include <cstddef>
#include <memory>
#include <new>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

// Com o AddressSanitizer ligado, a arena marca como inacessível a memória que
// ainda não entregou e a que foi devolvida por `reiniciar()`. Sem isso o ASan
// ficaria cego dentro de um bloco: para ele, o bloco inteiro é uma alocação
// só e válida. GCC anuncia o ASan com `__SANITIZE_ADDRESS__`, e o Clang com
// `__has_feature`.
#if defined(__SANITIZE_ADDRESS__)
#define MUS_ARENA_ASAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define MUS_ARENA_ASAN 1
#endif
#endif

#if defined(MUS_ARENA_ASAN)
#include <sanitizer/asan_interface.h>
#define MUS_ARENA_ENVENENA(endereco, tamanho) ASAN_POISON_MEMORY_REGION((endereco), (tamanho))
#define MUS_ARENA_DESENVENENA(endereco, tamanho) \
    ASAN_UNPOISON_MEMORY_REGION((endereco), (tamanho))
#else
#define MUS_ARENA_ENVENENA(endereco, tamanho) ((void)(endereco), (void)(tamanho))
#define MUS_ARENA_DESENVENENA(endereco, tamanho) ((void)(endereco), (void)(tamanho))
#endif

namespace mus {
namespace ast {

/**
 * @brief Alocador *bump* em blocos: o dono de todos os nós de uma AST.
 *
 * Cada alocação só avança um cursor dentro do bloco atual. Quando o bloco
 * acaba, a arena pede outro ao sistema, com o dobro do tamanho, até
 * `kMaiorBloco`. Nada é devolvido um a um: a memória volta inteira quando a
 * arena é destruída ou reiniciada.
 *
 * Objetos trivialmente destrutíveis (todos os nós da AST, ver o
 * `static_assert` em `ast.h`) não deixam rastro além dos bytes: destruí-los é
 * só soltar os blocos. Quem tem destrutor (as listas `std::vector` de
 * comandos) entra num registro e é destruído na ordem inversa da criação,
 * como objetos numa pilha.
 *
 * Não é reentrante nem segura entre threads, como o lexer e o parser que a
 * usam.
 */
class Arena {
public:
    /// Tamanho do bloco pré-alocado no construtor. Um programa `.mus` escrito à
    /// mão cabe nele inteiro.
    static constexpr std::size_t kPrimeiroBloco = 16 * 1024;

    /// Teto do crescimento geométrico. Um pedido maior que isso ganha um bloco
    /// do próprio tamanho.
    static constexpr std::size_t kMaiorBloco = 1024 * 1024;

    /**
     * @brief Cria a arena com o primeiro bloco já reservado.
     * @param primeiroBloco Tamanho, em bytes, do bloco pré-alocado (mínimo 1).
     */
    explicit Arena(std::size_t primeiroBloco = kPrimeiroBloco);

    /// Destrói os objetos registrados, na ordem inversa, e devolve os blocos.
    ~Arena();

    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;
    Arena(Arena&&) = delete;
    Arena& operator=(Arena&&) = delete;

    /**
     * @brief Constrói um `T` dentro da arena.
     *
     * O objeto vive até a arena ser destruída ou reiniciada; não existe
     * `delete` para ele. Se `T` tem destrutor não trivial, ele é registrado
     * para rodar nesse momento.
     *
     * @param argumentos Repassados ao construtor de `T`.
     * @return Ponteiro para o objeto, alinhado para `T`.
     */
    template <typename T, typename... Argumentos>
    T* criar(Argumentos&&... argumentos) {
        static_assert(alignof(T) <= alignof(std::max_align_t),
                      "a arena só garante o alinhamento fundamental");
        if constexpr (std::is_trivially_destructible_v<T>) {
            return new (alocar(sizeof(T), alignof(T)))
                T(std::forward<Argumentos>(argumentos)...);
        } else {
            // A vaga no registro é garantida antes de construir: se o
            // `push_back` pudesse falhar depois, o objeto já construído nunca
            // seria destruído.
            reservarDestrutor();
            T* objeto = new (alocar(sizeof(T), alignof(T)))
                T(std::forward<Argumentos>(argumentos)...);
            destrutores_.push_back(Destrutor{&destruirComo<T>, objeto});
            return objeto;
        }
    }

    /**
     * @brief Copia um texto para a arena.
     * @param texto Bytes a copiar; não precisa terminar em `'\0'`.
     * @return Vista sobre a cópia, válida enquanto a arena viver.
     */
    std::string_view copiarTexto(std::string_view texto);

    /**
     * @brief Reserva `tamanho` bytes alinhados a `alinhamento`.
     *
     * Caminho rápido inline: só avança o cursor. Quando o bloco atual não
     * comporta o pedido, abre um bloco novo.
     *
     * @param tamanho Bytes pedidos.
     * @param alinhamento Potência de 2, no máximo `alignof(std::max_align_t)`.
     * @return Início da região reservada.
     */
    void* alocar(std::size_t tamanho, std::size_t alinhamento) {
        void* inicio = cursor_;
        std::size_t espaco = static_cast<std::size_t>(fim_ - cursor_);
        if (std::align(alinhamento, tamanho, inicio, espaco) == nullptr) {
            inicio = alocarEmBlocoNovo(tamanho, alinhamento);
        }
        cursor_ = static_cast<std::byte*>(inicio) + tamanho;
        bytesUsados_ += tamanho;
        MUS_ARENA_DESENVENENA(inicio, tamanho);
        return inicio;
    }

    /**
     * @brief Esvazia a arena sem devolver o primeiro bloco.
     *
     * Roda os destrutores registrados, devolve os blocos extras e volta o
     * cursor ao começo do primeiro bloco: a próxima alocação reusa o mesmo
     * endereço da primeira. Todo ponteiro entregue antes deixa de valer (com
     * ASan, usá-lo é um erro acusado na hora).
     */
    void reiniciar();

    /// Bytes entregues por `alocar` desde a criação ou o último `reiniciar()`.
    std::size_t bytesUsados() const { return bytesUsados_; }

    /// Bytes reservados ao sistema, somando todos os blocos.
    std::size_t bytesReservados() const;

    /// Quantos blocos a arena tem agora.
    std::size_t quantidadeDeBlocos() const { return blocos_.size(); }

    /// Quantos objetos vivos têm destrutor registrado.
    std::size_t objetosComDestrutor() const { return destrutores_.size(); }

    /**
     * @brief Diz se um endereço está dentro de algum bloco da arena.
     * @param endereco Endereço a testar.
     * @return true se pertence a um dos blocos.
     */
    bool contem(const void* endereco) const;

private:
    struct Bloco {
        std::byte* inicio = nullptr;
        std::size_t tamanho = 0;
    };

    struct Destrutor {
        void (*destruir)(void*);
        void* objeto;
    };

    template <typename T>
    static void destruirComo(void* objeto) {
        static_cast<T*>(objeto)->~T();
    }

    void* alocarEmBlocoNovo(std::size_t tamanho, std::size_t alinhamento);
    void abrirBloco(std::size_t tamanho);
    void reservarDestrutor();
    void destruirObjetos();
    static void devolver(const Bloco& bloco);

    std::vector<Bloco> blocos_;
    std::vector<Destrutor> destrutores_;
    std::byte* cursor_ = nullptr;
    std::byte* fim_ = nullptr;
    std::size_t primeiroBloco_;
    std::size_t proximoBloco_;
    std::size_t bytesUsados_ = 0;
};

}  // namespace ast
}  // namespace mus

#endif  // MUS_INTERPRETER_ARENA_H
