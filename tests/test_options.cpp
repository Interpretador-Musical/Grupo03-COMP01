#include "doctest.h"

#include <string>
#include <vector>

#include "cli/options.h"

namespace {

// parseArguments recebe `char**` porque é a assinatura que o main() entrega.
// Este auxiliar monta o argv a partir de literais sem const_cast espalhado
// pelos casos, e reserva a posição 0 para o nome do programa.
mus::Options parse(std::vector<std::string> args) {
    args.insert(args.begin(), "compilador");

    std::vector<char*> argv;
    argv.reserve(args.size());
    for (std::string& arg : args) {
        argv.push_back(&arg[0]);
    }

    return mus::parseArguments(static_cast<int>(argv.size()), argv.data());
}

}  // namespace

TEST_CASE("sem argumentos devolve os padrões") {
    const mus::Options options = parse({});

    CHECK(options.ok);
    CHECK(options.error.empty());
    CHECK(options.inputPath.empty());
    CHECK(options.logLevel == mus::LogLevel::Info);
    CHECK_FALSE(options.showHelp);
    CHECK_FALSE(options.showVersion);
    CHECK_FALSE(options.noAudio);
    CHECK_FALSE(options.testTone);
}

TEST_CASE("o primeiro argumento posicional vira o caminho de entrada") {
    const mus::Options options = parse({"musica.mus"});

    CHECK(options.ok);
    CHECK(options.inputPath == "musica.mus");
}

TEST_CASE("as flags booleanas são reconhecidas nas duas formas") {
    CHECK(parse({"-h"}).showHelp);
    CHECK(parse({"--help"}).showHelp);
    CHECK(parse({"--version"}).showVersion);
    CHECK(parse({"--no-audio"}).noAudio);
    CHECK(parse({"--test-tone"}).testTone);
}

TEST_CASE("verbose e quiet movem o nível de log em direções opostas") {
    CHECK(parse({"-v"}).logLevel == mus::LogLevel::Debug);
    CHECK(parse({"--verbose"}).logLevel == mus::LogLevel::Debug);
    CHECK(parse({"-q"}).logLevel == mus::LogLevel::Error);
    CHECK(parse({"--quiet"}).logLevel == mus::LogLevel::Error);
}

TEST_CASE("a última flag de nível de log é a que vale") {
    // Não há precedência entre elas: quem escreve a linha de comando manda, e
    // a leitura é da esquerda para a direita.
    CHECK(parse({"-v", "-q"}).logLevel == mus::LogLevel::Error);
    CHECK(parse({"-q", "-v"}).logLevel == mus::LogLevel::Debug);
}

TEST_CASE("flags e caminho podem vir em qualquer ordem") {
    const mus::Options antes = parse({"--no-audio", "musica.mus"});
    const mus::Options depois = parse({"musica.mus", "--no-audio"});

    CHECK(antes.ok);
    CHECK(depois.ok);
    CHECK(antes.inputPath == "musica.mus");
    CHECK(depois.inputPath == "musica.mus");
    CHECK(antes.noAudio);
    CHECK(depois.noAudio);
}

TEST_CASE("opção desconhecida é erro, e o erro vem preenchido") {
    const mus::Options options = parse({"--nao-existe"});

    CHECK_FALSE(options.ok);
    CHECK_FALSE(options.error.empty());
    // A mensagem precisa citar a opção recusada — é o que a torna útil.
    CHECK(options.error.find("--nao-existe") != std::string::npos);
}

TEST_CASE("dois arquivos de entrada é erro") {
    const mus::Options options = parse({"a.mus", "b.mus"});

    CHECK_FALSE(options.ok);
    CHECK(options.error.find("a.mus") != std::string::npos);
    CHECK(options.error.find("b.mus") != std::string::npos);
}

TEST_CASE("o parsing para no primeiro erro") {
    // `--nao-existe` falha antes de `--no-audio` ser lido: o que vem depois de
    // uma linha de comando inválida não deve ter efeito nenhum.
    const mus::Options options = parse({"--nao-existe", "--no-audio"});

    CHECK_FALSE(options.ok);
    CHECK_FALSE(options.noAudio);
}

TEST_CASE("um hífen sozinho é tratado como argumento posicional") {
    // `-` é a convenção de entrada padrão no Unix, então não pode cair no ramo
    // de opção desconhecida. A guarda `arg.size() > 1` no parser existe para
    // isso, e este caso é o que a mantém viva.
    const mus::Options options = parse({"-"});

    CHECK(options.ok);
    CHECK(options.inputPath == "-");
}

TEST_CASE("a versão do programa não é vazia") {
    CHECK(std::string(mus::kProgramVersion).size() > 0);
}

TEST_CASE("--demo é reconhecido") {
    CHECK(parse({"--demo"}).demo);
    CHECK_FALSE(parse({}).demo);
}

TEST_CASE("--bpm lê o valor seguinte") {
    const mus::Options options = parse({"--bpm", "140"});

    CHECK(options.ok);
    CHECK(options.bpm == doctest::Approx(140.0));
}

TEST_CASE("o BPM padrão é 120") {
    CHECK(parse({}).bpm == doctest::Approx(120.0));
}

TEST_CASE("--bpm sem valor é erro") {
    const mus::Options options = parse({"--bpm"});

    CHECK_FALSE(options.ok);
    CHECK(options.error.find("--bpm") != std::string::npos);
}

TEST_CASE("--bpm não positivo ou ilegível é erro") {
    // strtod devolve 0.0 para texto que não é número, e 0 BPM é tão inválido
    // quanto: o mesmo teste `!(bpm > 0.0)` cobre os dois.
    for (const char* valor : {"0", "-60", "abc", ""}) {
        const mus::Options options = parse({"--bpm", valor});
        CAPTURE(valor);
        CHECK_FALSE(options.ok);
        CHECK_FALSE(options.error.empty());
    }
}

TEST_CASE("--bpm aceita andamento fracionário") {
    CHECK(parse({"--bpm", "137.5"}).bpm == doctest::Approx(137.5));
}

TEST_CASE("o valor do --bpm não é confundido com o arquivo de entrada") {
    // O `++i` dentro do ramo do --bpm existe para isso: sem ele, "140" cairia
    // no ramo posicional e viraria o caminho do .mus.
    const mus::Options options = parse({"--bpm", "140", "musica.mus"});

    CHECK(options.ok);
    CHECK(options.bpm == doctest::Approx(140.0));
    CHECK(options.inputPath == "musica.mus");
}

TEST_CASE("--loop é reconhecido e é independente do --demo") {
    CHECK(parse({"--loop"}).loop);
    CHECK_FALSE(parse({}).loop);
    CHECK_FALSE(parse({"--demo"}).loop);

    const mus::Options ambos = parse({"--loop", "--demo"});
    CHECK(ambos.loop);
    CHECK(ambos.demo);
}

TEST_CASE("--tokens é reconhecida e não conflita com as demais flags") {
    CHECK(parse({"--tokens"}).showTokens);
    CHECK_FALSE(parse({}).showTokens);

    // Convive com o arquivo de entrada e com o nível de log: a tabela de tokens
    // vai para stdout, os logs para stderr.
    const mus::Options options = parse({"--tokens", "-q", "musica.mus"});
    CHECK(options.ok);
    CHECK(options.showTokens);
    CHECK(options.inputPath == "musica.mus");
    CHECK(options.logLevel == mus::LogLevel::Error);
}
