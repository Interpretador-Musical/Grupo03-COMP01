---
title: Documentação
layout: default
nav_order: 2
has_children: true
---

# Documentação

Tudo que o grupo produz de documentação do projeto vive aqui. Conforme a linguagem musical foi desenhada e o compilador estruturado, separamos o nosso acervo entre atas de decisão e os guias técnicos oficiais do sistema.

## Manuais e Especificações Técnicas

Documentação estrutural do compilador, já definida e entregue:

* [Visão de Arquitetura]({{ site.baseurl }}{% link arquitetura.md %}) — detalhamento do compilador musical em lote e a divisão estrita de responsabilidades com o motor de áudio (miniaudio).
* [ADR 001: Modelo Batch]({{ site.baseurl }}{% link decisao_modelo_batch.md %}) — registro da decisão de 16/09 pelo programa finito compilado antes de tocar, as alternativas, as consequências e a matriz das issues mantidas, reformuladas e encerradas.
* [Arquitetura e Design da AST]({{ site.baseurl }}{% link ast.md %}) — especificação técnica da Árvore Sintática Abstrata, decisões de C++17 (variantes vs. polimorfismo), catálogo de nós e rastreamento de posições.
* [Representação Intermediária]({{ site.baseurl }}{% link representacao_intermediaria.md %}) — a `Timeline` de `SoundEvent` como IR do compilador: estrutura, invariantes, tradução da AST e relação com o código de três endereços.
* [Cálculos e Cobertura]({{ site.baseurl }}{% link calculos.md %}) — inventário das contas do pipeline (altura, tempo, frames), matriz de casos normais, de borda e inválidos, MC/DC onde se justifica e a cobertura medida.
* [Teste de Estresse]({{ site.baseurl }}{% link estresse.md %}) — cargas `.mus` com contagem de eventos conhecida, de 60 a 6000 BPM e até 10 milhões de eventos: tempo de cada fase, o que sai do motor de áudio, o envelope aceito e as falhas encontradas.
* [Gramática EBNF]({{ site.baseurl }}{% link gramatica_ebnf.md %}) — definição formal do léxico e da sintaxe aceitos pelo `lexer.l` e pelo `parser.y`, com precedência, exemplos válidos e inválidos, e a verificação por teste de que a EBNF e o Bison reconhecem a mesma linguagem.
* [Especificação de Requisitos]({{ site.baseurl }}{% link especificacao.md %}) — definição oficial do objetivo, léxico, sintaxe da linguagem musical e escopo de lógicas para as entregas (P1 e P2).
* [Cronograma]({{ site.baseurl }}{% link cronograma.md %}) — as seis sprints do semestre, com as issues de cada uma e o que já foi concluído.
* [Guia de Ambiente (Windows)]({{ site.baseurl }}{% link guia_ambiente.md %}) — tutorial passo a passo para instalar dependências (C++, Flex, Bison, CMake) e configurar o WSL.
* [Notas da Release M1]({{ site.baseurl }}{% link release_m1.md %}) — validação da primeira versão contínua funcional (Ponto de Controle 2).
* [Relatório e Evidências do P2]({{ site.baseurl }}{% link relatorio_p2.md %}) — consolidação do progresso entre P1 e P2, matriz de rastreabilidade (requisitos → código → testes → resultado), comprovação dos testes automatizados e evidências de execução.

## Atas de reunião

Registro cronológico das decisões de projeto — o que foi decidido, quando, e por quê.

| Data | Assunto principal |
|---|---|
| [19/08/2026]({{ site.baseurl }}{% link atas/2026-08-19-ata-reuniao.md %}) | Concepção do projeto, aprovação do professor, escolha da stack e definição do pipeline. |
| [26/08/2026]({{ site.baseurl }}{% link atas/2026-08-26-ata-reuniao.md %}) | Mudança para Flex + Bison, fluxo entre repositórios e panorama de pendências. |

## Em breve

Documentos previstos, mas ainda em construção:

- **Guia de Sintaxe** — manual prático de programação para o usuário, contendo todas as construções válidas.
- **Exemplos `.mus`** — programas musicais completos que servirão como testes de aceitação definitivos.

Acompanhe o [roadmap]({{ site.baseurl }}{% link roadmap.md %}) para saber o status do desenvolvimento técnico que viabiliza estes últimos documentos.

