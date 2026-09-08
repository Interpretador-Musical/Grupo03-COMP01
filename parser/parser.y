%{
#include <stdio.h>
#include <stdlib.h>

int yylex(void);
void yyerror(const char *s);
%}

%token NUM PLUS MINUS TIMES DIVIDE LPAREN RPAREN

%%

expressao:
    expressao PLUS expressao
  | expressao MINUS expressao
  | expressao TIMES expressao
  | expressao DIVIDE expressao
  | LPAREN expressao RPAREN
  | NUM
  ;

%%

void yyerror(const char *s) {
    fprintf(stderr, "Erro sintático: %s\n", s);
}

#ifdef __cplusplus
extern "C" {
#endif
    int init_audio_engine();
#ifdef __cplusplus
}
#endif

int main(void) {
    printf("Iniciando Interpretador Musical...\n");
    
    // Inicia a thread de som (vai travar o terminal esperando o Enter)
    init_audio_engine();

    // Depois de apertar Enter, ele libera pro Parser rodar
    printf("Digite expressoes para o Parser:\n");
    yyparse();
    return 0;
}
