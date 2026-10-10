#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

/**
    Servidor HTTP minimalista:
      GET /        -> página que toca o áudio no navegador do celular
      GET /stream  -> PCM 16-bit estéreo intercalado, sem fim (fetch streaming)

    A thread de áudio só copia amostras para um FIFO lock-free (pushAudio).
    Uma thread própria lê o FIFO e envia para os clientes conectados.
*/
class AudioStreamServer
{
public:
    AudioStreamServer();
    ~AudioStreamServer();

    // Idempotente. Escolhe a primeira porta livre entre 8080 e 8099.
    void start();

    void setSampleRate (double newSampleRate) noexcept { sampleRate = (int) newSampleRate; }

    // Chamar da thread de áudio. Não aloca, não bloqueia.
    void pushAudio (const juce::AudioBuffer<float>& buffer) noexcept;

    int getPort() const noexcept        { return port.load(); }
    int getNumClients() const noexcept  { return numClients.load(); }

    // IPv4 locais úteis (redes privadas primeiro).
    static juce::StringArray getLocalAddresses();

private:
    class Worker : public juce::Thread
    {
    public:
        Worker (const juce::String& name, std::function<void()> f)
            : juce::Thread (name), fn (std::move (f)) {}
        ~Worker() override { stopThread (2000); }
        void run() override { fn(); }

    private:
        std::function<void()> fn;
    };

    void acceptLoop();
    void sendLoop();
    void handleConnection (std::unique_ptr<juce::StreamingSocket> connection);
    void broadcast (const void* data, int numBytes);

    static constexpr int fifoFrames = 65536;

    juce::StreamingSocket listener;
    std::atomic<bool> started { false };
    std::atomic<int> port { 0 };
    std::atomic<int> numClients { 0 };
    std::atomic<int> sampleRate { 48000 };

    juce::AbstractFifo fifo { fifoFrames };
    std::vector<int16_t> fifoData;   // estéreo intercalado

    juce::CriticalSection clientsLock;
    std::vector<std::unique_ptr<juce::StreamingSocket>> clients;

    Worker acceptThread { "PhoneMonitor Accept", [this] { acceptLoop(); } };
    Worker sendThread   { "PhoneMonitor Send",   [this] { sendLoop(); } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioStreamServer)
};
