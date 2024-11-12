#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <semaphore>
#include <atomic>
#include <chrono>
#include <random>

constexpr int TOTAL_JOGADORES = 4;
std::counting_semaphore<TOTAL_JOGADORES> semaforo_cadeiras(TOTAL_JOGADORES - 1);
std::condition_variable cond_var_musica;
std::mutex mutex_musica;
std::atomic<bool> musica_em_pausa{false};
std::atomic<bool> jogo_ativo{true};
int contador_cadeira = 1;

class JogoCadeiras
{
public:
    JogoCadeiras(int total_jogadores)
        : cadeiras(total_jogadores - 1) {}

    void iniciar_nova_rodada(int jogadores_em_jogo)
    {
        cadeiras--;
        contador_cadeira = 1;
        while (semaforo_cadeiras.try_acquire());
        semaforo_cadeiras.release(cadeiras);
        musica_em_pausa.store(false);

        if (jogadores_em_jogo > 1)
        {
            std::cout << "\nNova rodada com " << jogadores_em_jogo << " jogadores e " << cadeiras << " cadeiras.\n";
            std::cout << "A música está tocando... 🎵.\n\n";
        }
    }

    void pausar_musica()
    {
        std::unique_lock<std::mutex> lock(mutex_musica);
        musica_em_pausa.store(true);
        cond_var_musica.notify_all();
        std::cout << "> A música parou! Jogadores tentando se sentar...\n\n";
        std::cout << "-----------------------------------------------\n";
    }

    void mostrar_status()
    {
        std::cout << "Rodada atual com " << cadeiras << " cadeiras disponíveis.\n";
    }

    bool jogo_continua(int jogadores_em_jogo) const
    {
        return jogadores_em_jogo > 1;
    }

private:
    int cadeiras;
};

class Jogador
{
public:
    Jogador(int id)
        : identificador(id), ativo(true), tentou_na_rodada(false) {}

    bool esta_ativo() const
    {
        return ativo;
    }

    int get_identificador() const
    {
        return identificador;
    }

    void tentar_sentarse()
    {
        std::unique_lock<std::mutex> lock(mutex_musica);
        cond_var_musica.wait(lock, []
                      { return musica_em_pausa.load(); });

        if (ativo && !tentou_na_rodada)
        {
            tentou_na_rodada = true;
            if (semaforo_cadeiras.try_acquire())
            {
                std::cout << "[Cadeira " << contador_cadeira++ << "]: Ocupada por Jogador " << identificador << "\n";
            }
            else
            {
                ativo = false;
                std::cout << "\nJogador " << identificador << " não conseguiu uma cadeira e foi eliminado!\n";
            }
        }
    }

    void resetar_rodada()
    {
        tentou_na_rodada = false;
    }

    void participar_jogo()
    {
        while (ativo)
        {
            tentar_sentarse();
            if (!ativo)
            {
                std::this_thread::yield();
            }
        }
    }

private:
    int identificador;
    bool ativo;
    bool tentou_na_rodada;
};

class Controlador
{
public:
    Controlador(JogoCadeiras &jogo, std::vector<Jogador> &jogadores)
        : jogo(jogo), jogadores(jogadores) {}

    void iniciar_jogo()
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(1000, 3000);

        while (jogo.jogo_continua(jogadores_em_jogo()))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(dist(gen)));
            jogo.pausar_musica();

            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            liberar_jogadores_eliminados();
            jogo.iniciar_nova_rodada(jogadores_em_jogo());
            resetar_jogadores_para_rodada();
        }

        std::cout << "\n🏆 Vencedor: Jogador " << encontrar_vencedor() << "! Parabéns! 🏆\n";
    }

private:
    JogoCadeiras &jogo;
    std::vector<Jogador> &jogadores;

    int jogadores_em_jogo() const
    {
        int ativos = 0;
        for (const auto &jogador : jogadores)
        {
            if (jogador.esta_ativo())
                ativos++;
        }
        return ativos;
    }

    int encontrar_vencedor() const
    {
        for (const auto &jogador : jogadores)
        {
            if (jogador.esta_ativo())
            {
                return jogador.get_identificador();
            }
        }
        return -1;
    }

    void liberar_jogadores_eliminados()
    {
        semaforo_cadeiras.release(TOTAL_JOGADORES - 1);
    }

    void resetar_jogadores_para_rodada()
    {
        for (auto &jogador : jogadores)
        {
            jogador.resetar_rodada();
        }
    }
};

int main()
{
    JogoCadeiras jogo(TOTAL_JOGADORES);
    std::vector<Jogador> jogadores;
    for (int i = 1; i <= TOTAL_JOGADORES; ++i)
    {
        jogadores.emplace_back(i);
    }

    Controlador controlador(jogo, jogadores);
    std::vector<std::thread> threads_jogadores;

    for (auto &jogador : jogadores)
    {
        threads_jogadores.emplace_back(&Jogador::participar_jogo, &jogador);
    }

    std::thread thread_controlador(&Controlador::iniciar_jogo, &controlador);

    for (auto &t : threads_jogadores)
    {
        if (t.joinable())
        {
            t.join();
        }
    }

    if (thread_controlador.joinable())
    {
        thread_controlador.join();
    }

    std::cout << "Jogo das Cadeiras finalizado.\n";
    return 0;
}
