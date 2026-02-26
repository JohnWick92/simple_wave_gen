### Componentes e Hierarquia
É utilizado uma classe abstrata para que sejam feito o contrato dos geradores de onda.
A comunicação usa um header que define que o gerador se comunicará com o controlador através de named pipe
e o viewer receberá as ondas geradas através de memória compartilada.
Para a GUI foi usado gtk com a lib gtkmm 4

## 🛠️ Como Compilar
O projeto inclui um `Makefile`, o que facilita a compilação.

### Pré-requisitos
*   Um compilador C++ com suporte a C++11 ou superior (como `g++` ou `clang++`).
*   `gtkmm4` instalado no sistema
*   `make` instalado no sistema.
*   Sistema operacional Linux

### Passos para Compilar
1.  **Clone o repositório**:
    ```bash
    git clone https://github.com/JohnWick92/simple_wave_gen.git
    cd simple_wave_gen
    ```
2.  **Execute o Makefile**:
    ```bash
    make
    ```
    Este comando compilará o código-fonte em `src/` e gerará um executável, provavelmente dentro de uma pasta `bin/`.

3.  **Limpar e remover os arquivos compilados**:
    ```bash
    make clean
    ```

## 🚀 Como Executar
Após a compilação bem-sucedida, o 3 binários serão criados: generator, controller e viewer.
Devem ser executados nessa ordem para funcionar generator -> controller -> viewer
A interação do usuário é feita através do controlador que tem a lista de comandos possíveis.
