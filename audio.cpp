#include <iostream>
#include <cmath>

#define MINIAUDIO_IMPLEMENTATION
#include "include/miniaudio.h"

const double PI = 3.14159265358979323846;
const double FREQUENCY = 440.0; // Frequência do Lá (A4)
const int SAMPLE_RATE = 44100;

// Variável global para rastrear a fase da onda contínua
double global_phase = 0.0;

// Função Callback: A placa de som chama essa função quando precisa de mais áudio
void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    float* pOutputF32 = (float*)pOutput;
    double phaseIncrement = (2.0 * PI * FREQUENCY) / SAMPLE_RATE;

    for (ma_uint32 i = 0; i < frameCount; ++i) {
        // Gera o sample da onda senoidal
        float sample = (float)std::sin(global_phase);
        
        // Envia para o canal de saída (mono)
        pOutputF32[i] = sample;

        // Avança a fase e a mantém dentro do limite de 2*PI para evitar overflow
        global_phase += phaseIncrement;
        if (global_phase >= 2.0 * PI) {
            global_phase -= 2.0 * PI;
        }
    }
}

// Função pública para inicializar o áudio
extern "C" int init_audio_engine() {
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format   = ma_format_f32;
    config.playback.channels = 1; // Áudio Mono
    config.sampleRate        = SAMPLE_RATE;
    config.dataCallback      = data_callback;
    
    // NFR: Configuração de baixa latência exigida nos Critérios de Aceitação
    config.periodSizeInFrames = 256; 

    ma_device device;
    if (ma_device_init(NULL, &config, &device) != MA_SUCCESS) {
        std::cerr << "Falha ao inicializar o dispositivo de áudio." << std::endl;
        return -1;
    }

    std::cout << "Motor de áudio iniciado. Tocando senoidal de 440Hz..." << std::endl;
    ma_device_start(&device);

    // Mantém o áudio tocando até o usuário pressionar Enter no terminal
    std::cout << "Pressione ENTER para parar o som." << std::endl;
    getchar();

    ma_device_uninit(&device);
    return 0;
}