#include "PluginEditor.h"

PhoneMonitorEditor::PhoneMonitorEditor (PhoneMonitorProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    processor.getServer().start();   // idempotente
    addresses = AudioStreamServer::getLocalAddresses();

    nextIpButton.onClick = [this]
    {
        if (! addresses.isEmpty())
            addressIndex = (addressIndex + 1) % addresses.size();

        updateQr();
        repaint();
    };
    addAndMakeVisible (nextIpButton);

    setSize (360, 470);
    updateQr();
    startTimerHz (2);
}

PhoneMonitorEditor::~PhoneMonitorEditor()
{
    stopTimer();
}

void PhoneMonitorEditor::updateQr()
{
    const int port = processor.getServer().getPort();

    if (port == 0 || addresses.isEmpty())
    {
        url.clear();
        qr.reset();
        return;
    }

    const auto newUrl = "http://" + addresses[addressIndex] + ":" + juce::String (port);

    if (newUrl == url)
        return;

    url = newUrl;
    qr.emplace (qrcodegen::QrCode::encodeText (url.toRawUTF8(), qrcodegen::QrCode::Ecc::MEDIUM));
}

void PhoneMonitorEditor::timerCallback()
{
    updateQr();
    repaint();
}

void PhoneMonitorEditor::resized()
{
    nextIpButton.setBounds (getLocalBounds().removeFromBottom (60).reduced (110, 14));
}

void PhoneMonitorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff15171c));

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.drawText ("Phone Monitor", 0, 12, getWidth(), 26, juce::Justification::centred);

    const juce::Rectangle<int> qrArea (50, 50, 260, 260);

    if (qr.has_value())
    {
        g.setColour (juce::Colours::white);
        g.fillRect (qrArea);

        const int quiet = 2;
        const int n = qr->getSize();
        const float cell = (float) qrArea.getWidth() / (float) (n + 2 * quiet);

        g.setColour (juce::Colours::black);

        for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x)
                if (qr->getModule (x, y))
                    g.fillRect (juce::Rectangle<float> ((float) qrArea.getX() + (float) (x + quiet) * cell,
                                                        (float) qrArea.getY() + (float) (y + quiet) * cell,
                                                        cell + 0.5f, cell + 0.5f));
    }
    else
    {
        g.setColour (juce::Colours::grey);
        g.drawRect (qrArea);
        g.setFont (juce::FontOptions (14.0f));

        const auto msg = addresses.isEmpty() ? "Nenhuma rede detectada" : "Iniciando servidor...";
        g.drawText (msg, qrArea, juce::Justification::centred);
    }

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (15.0f));
    g.drawText (url.isEmpty() ? juce::String ("-") : url, 0, 322, getWidth(), 22, juce::Justification::centred);

    g.setColour (juce::Colours::lightgrey);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("Ouvintes conectados: " + juce::String (processor.getServer().getNumClients()),
                0, 350, getWidth(), 20, juce::Justification::centred);
    g.drawText ("Celular e PC na mesma rede Wi-Fi", 0, 372, getWidth(), 20, juce::Justification::centred);
}
