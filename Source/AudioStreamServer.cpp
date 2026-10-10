#include "AudioStreamServer.h"

#include <cmath>
#include <cstring>

namespace
{
    // Página servida ao celular. Usa fetch streaming + Web Audio (funciona em HTTP puro).
    const char* kPage = R"HTML(<!doctype html>
<html lang="pt-br"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Phone Monitor</title>
<style>
body{margin:0;min-height:100vh;display:flex;flex-direction:column;align-items:center;justify-content:center;background:#15171c;color:#eee;font-family:system-ui,sans-serif;text-align:center}
button{font-size:1.4rem;padding:1rem 2.5rem;border:0;border-radius:999px;background:#3b82f6;color:#fff}
#s{margin-top:1.2rem;opacity:.7}
</style></head><body>
<h2>Phone Monitor</h2>
<button id="b">Ouvir</button>
<div id="s">Toque para começar</div>
<script>
const b=document.getElementById('b'),s=document.getElementById('s');
let ctx=null,abort=null;
function stop(msg){
  if(abort){abort.abort();abort=null;}
  if(ctx){ctx.close();ctx=null;}
  b.textContent='Ouvir';s.textContent=msg;
}
b.onclick=async()=>{
  if(abort){stop('Parado');return;}
  const c=new(window.AudioContext||window.webkitAudioContext)();
  ctx=c;
  await c.resume();
  const ac=new AbortController();
  abort=ac;
  b.textContent='Parar';s.textContent='Conectando...';
  try{
    const r=await fetch('/stream',{signal:ac.signal,cache:'no-store'});
    const sr=+r.headers.get('X-Sample-Rate')||48000;
    const rd=r.body.getReader();
    let left=new Uint8Array(0),next=0;
    s.textContent='Ao vivo';
    for(;;){
      const {value,done}=await rd.read();
      if(done)break;
      const u=new Uint8Array(left.length+value.length);
      u.set(left);u.set(value,left.length);
      const use=u.length-(u.length%4);
      left=u.slice(use);
      if(!use)continue;
      const p=new Int16Array(u.buffer,0,use/2),n=use/4;
      const buf=c.createBuffer(2,n,sr);
      const L=buf.getChannelData(0),R=buf.getChannelData(1);
      for(let i=0;i<n;i++){L[i]=p[2*i]/32768;R[i]=p[2*i+1]/32768;}
      const src=c.createBufferSource();
      src.buffer=buf;src.connect(c.destination);
      const now=c.currentTime;
      if(next<now+0.02)next=now+0.12;
      if(next-now>0.5)continue;
      src.start(next);next+=buf.duration;
    }
    stop('Desconectado');
  }catch(e){
    if(e.name!=='AbortError')stop('Erro: '+e.message);
  }
};
</script></body></html>)HTML";

    inline int16_t toInt16 (float x) noexcept
    {
        x = juce::jlimit (-1.0f, 1.0f, x);
        return (int16_t) std::lrintf (x * 32767.0f);
    }
}

//==============================================================================
AudioStreamServer::AudioStreamServer()
{
    fifoData.assign ((size_t) fifoFrames * 2, 0);
}

AudioStreamServer::~AudioStreamServer()
{
    acceptThread.signalThreadShouldExit();
    sendThread.signalThreadShouldExit();
    listener.close();                 // desbloqueia waitForNextConnection()
    acceptThread.stopThread (2000);
    sendThread.stopThread (2000);

    const juce::ScopedLock sl (clientsLock);
    clients.clear();
}

void AudioStreamServer::start()
{
    if (started.exchange (true))
        return;

    for (int p = 8080; p <= 8099; ++p)
    {
        if (listener.createListener (p))
        {
            port = p;
            break;
        }
    }

    if (port.load() == 0)
    {
        started = false;
        return;
    }

    sendThread.startThread();
    acceptThread.startThread();
}

//==============================================================================
void AudioStreamServer::pushAudio (const juce::AudioBuffer<float>& buffer) noexcept
{
    if (numClients.load (std::memory_order_relaxed) == 0)
        return;

    const int n = buffer.getNumSamples();
    if (n <= 0 || buffer.getNumChannels() == 0)
        return;

    int start1, size1, start2, size2;
    fifo.prepareToWrite (n, start1, size1, start2, size2);

    if (size1 + size2 < n)
        return;   // FIFO cheio: descarta o bloco inteiro

    const float* left  = buffer.getReadPointer (0);
    const float* right = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : left;

    auto copyRange = [&] (int start, int count, int offset)
    {
        for (int i = 0; i < count; ++i)
        {
            fifoData[(size_t) (start + i) * 2]     = toInt16 (left[offset + i]);
            fifoData[(size_t) (start + i) * 2 + 1] = toInt16 (right[offset + i]);
        }
    };

    copyRange (start1, size1, 0);
    copyRange (start2, size2, size1);
    fifo.finishedWrite (size1 + size2);
}

//==============================================================================
void AudioStreamServer::acceptLoop()
{
    while (! juce::Thread::currentThreadShouldExit())
    {
        std::unique_ptr<juce::StreamingSocket> connection (listener.waitForNextConnection());

        if (connection == nullptr)
        {
            if (juce::Thread::currentThreadShouldExit())
                break;

            juce::Thread::sleep (50);
            continue;
        }

        handleConnection (std::move (connection));
    }
}

void AudioStreamServer::handleConnection (std::unique_ptr<juce::StreamingSocket> connection)
{
    char buf[2048] = {};

    if (connection->waitUntilReady (true, 300) != 1)
        return;

    const int n = connection->read (buf, (int) sizeof (buf) - 1, false);
    if (n <= 0)
        return;

    const auto request = juce::String::fromUTF8 (buf, n);

    if (request.startsWith ("GET /stream"))
    {
        juce::String header;
        header << "HTTP/1.1 200 OK\r\n"
               << "Content-Type: application/octet-stream\r\n"
               << "Cache-Control: no-store\r\n"
               << "X-Sample-Rate: " << sampleRate.load() << "\r\n"
               << "Connection: close\r\n\r\n";

        const char* utf8 = header.toRawUTF8();
        const int len = (int) std::strlen (utf8);

        if (connection->write (utf8, len) != len)
            return;

        const juce::ScopedLock sl (clientsLock);
        clients.push_back (std::move (connection));
        numClients = (int) clients.size();
    }
    else if (request.startsWith ("GET / ") || request.startsWith ("GET /?"))
    {
        const juce::String body (juce::CharPointer_UTF8 (kPage));
        const int bodyBytes = (int) body.getNumBytesAsUTF8();

        juce::String header;
        header << "HTTP/1.1 200 OK\r\n"
               << "Content-Type: text/html; charset=utf-8\r\n"
               << "Content-Length: " << bodyBytes << "\r\n"
               << "Connection: close\r\n\r\n";

        const char* h = header.toRawUTF8();
        const int hLen = (int) std::strlen (h);

        if (connection->write (h, hLen) == hLen)
            connection->write (body.toRawUTF8(), bodyBytes);
    }
    else
    {
        const char* notFound = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        connection->write (notFound, (int) std::strlen (notFound));
    }
}

//==============================================================================
void AudioStreamServer::sendLoop()
{
    std::vector<int16_t> chunk;

    while (! juce::Thread::currentThreadShouldExit())
    {
        const int sr = juce::jmax (8000, sampleRate.load());
        int ready = fifo.getNumReady();

        // Sem ouvintes: mantém o FIFO vazio.
        if (numClients.load() == 0)
        {
            if (ready > 0)
                fifo.finishedRead (ready);

            juce::Thread::sleep (20);
            continue;
        }

        // Limita a latência acumulada (~250 ms): descarta o excesso mais antigo.
        const int maxLatencyFrames = sr / 4;
        if (ready > maxLatencyFrames)
        {
            fifo.finishedRead (ready - maxLatencyFrames / 2);
            ready = fifo.getNumReady();
        }

        const int minFrames = sr / 100;   // 10 ms
        if (ready < minFrames)
        {
            juce::Thread::sleep (2);
            continue;
        }

        const int toSend = juce::jmin (ready, sr / 25);   // até 40 ms por envio

        int s1, z1, s2, z2;
        fifo.prepareToRead (toSend, s1, z1, s2, z2);

        chunk.resize ((size_t) (z1 + z2) * 2);
        std::memcpy (chunk.data(), fifoData.data() + (size_t) s1 * 2, (size_t) z1 * 2 * sizeof (int16_t));
        if (z2 > 0)
            std::memcpy (chunk.data() + (size_t) z1 * 2, fifoData.data() + (size_t) s2 * 2,
                         (size_t) z2 * 2 * sizeof (int16_t));

        fifo.finishedRead (z1 + z2);
        broadcast (chunk.data(), (int) (chunk.size() * sizeof (int16_t)));
    }
}

void AudioStreamServer::broadcast (const void* data, int numBytes)
{
    const juce::ScopedLock sl (clientsLock);

    for (int i = (int) clients.size(); --i >= 0;)
        if (clients[(size_t) i]->write (data, numBytes) != numBytes)
            clients.erase (clients.begin() + i);

    numClients = (int) clients.size();
}

//==============================================================================
juce::StringArray AudioStreamServer::getLocalAddresses()
{
    juce::StringArray privateFirst, others;

    for (auto& address : juce::IPAddress::getAllAddresses (false))   // só IPv4
    {
        const auto s = address.toString();

        if (s.startsWith ("127.") || s.startsWith ("169.254."))
            continue;

        if (s.startsWith ("192.168.") || s.startsWith ("10."))
            privateFirst.add (s);
        else
            others.add (s);
    }

    privateFirst.addArray (others);
    return privateFirst;
}
