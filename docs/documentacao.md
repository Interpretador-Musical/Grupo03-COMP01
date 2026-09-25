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
* [Especificação de Requisitos]({{ site.baseurl }}{% link especificacao.md %}) — definição oficial do objetivo, léxico, sintaxe da linguagem musical e escopo de lógicas para as entregas (P1 e P2).
* [Cronograma]({{ site.baseurl }}{% link cronograma.md %}) — as seis sprints do semestre, com as issues de cada uma e o que já foi concluído.
* [Guia de Ambiente (Windows)]({{ site.baseurl }}{% link guia_ambiente.md %}) — tutorial passo a passo para instalar dependências (C++, Flex, Bison, CMake) e configurar o WSL.

## Atas de reunião

Registro cronológico das decisões de projeto — o que foi decidido, quando, e por quê.

| Data | Assunto principal |
|---|---|
| [19/08/2026]({{ site.baseurl }}{% link atas/2026-08-19-ata-reuniao.md %}) | Concepção do projeto, aprovação do professor, escolha da stack e definição do pipeline. |
| [26/08/2026]({{ site.baseurl }}{% link atas/2026-08-26-ata-reuniao.md %}) | Mudança para Flex + Bison, fluxo entre repositórios e panorama de pendências. |

## Em breve

Documentos previstos, mas ainda em construção:

- **Gramática EBNF** — a formalização teórica completa da gramática livre de contexto da linguagem, base do nosso `parser.y`.
- **Guia de Sintaxe** — manual prático de programação para o usuário, contendo todas as construções válidas.
- **Exemplos `.mus`** — programas musicais completos que servirão como testes de aceitação definitivos.

Acompanhe o [roadmap]({{ site.baseurl }}{% link roadmap.md %}) para saber o status do desenvolvimento técnico que viabiliza estes últimos documentos.

