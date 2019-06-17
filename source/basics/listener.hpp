#pragma once

#include <source/graph.hpp>

//-------------------------------------------------------------------------------------------------
class Listener : public Node
// listens to the audio/midi thread and report events/data to the qml thread
//-------------------------------------------------------------------------------------------------
{
    Q_OBJECT

    WPN_DECLARE_DEFAULT_AUDIO_INPUT     (audio_in, 0)
    WPN_DECLARE_DEFAULT_MIDI_INPUT      (midi_in, 0)

public:

    //-------------------------------------------------------------------------------------------------
    Listener() { m_name = "Listener"; }

    //-------------------------------------------------------------------------------------------------
    Q_SIGNAL void
    audio(QVector<qreal> const&);

    //-------------------------------------------------------------------------------------------------
    Q_SIGNAL void
    noteOn(unsigned int channel, unsigned int index, unsigned int velocity);

    //-------------------------------------------------------------------------------------------------
    Q_SIGNAL void
    noteOff(unsigned int channel, unsigned int index, unsigned int velocity);

    //-------------------------------------------------------------------------------------------------
    Q_SIGNAL void
    control(unsigned int channel, unsigned int index, unsigned int value);

    //-------------------------------------------------------------------------------------------------
    virtual void
    initialize(const Graph::properties& properties) override { m_rate = properties.rate; }

    //-------------------------------------------------------------------------------------------------
    virtual void
    rwrite(pool& inputs, pool& outputs, vector_t nframes) override
    {
        auto midi_events = inputs.midi[0];

        for (auto& event : midi_events)
        {
            if (event.status < 0x90)
                // this is thread safe, it will pass the signal on to the main gui/qml thread
                QMetaObject::invokeMethod(this, "noteOff", Qt::QueuedConnection,
                                          Q_ARG(unsigned int, 0),
                                          Q_ARG(unsigned int, event.b1),
                                          Q_ARG(unsigned int, event.b2));
            else if (event.status < 0xa0)
                QMetaObject::invokeMethod(this, "noteOn", Qt::QueuedConnection,
                                          Q_ARG(unsigned int, 0),
                                          Q_ARG(unsigned int, event.b1),
                                          Q_ARG(unsigned int, event.b2));
        }

//        QMetaObject::invokeMethod(this, "noteOn", Qt::QueuedConnection, nullptr, 0, 0, 0);

    }

    //-------------------------------------------------------------------------------------------------
    virtual void
    on_rate_changed(sample_t rate) override { m_rate = rate; }

private:

    size_t
    m_phase = 0;

    sample_t
    m_rate = 0;

};
