#include "graph.hpp"
#include <QtDebug>
#include <vector>
#include <cmath>

using namespace wpn114;

Graph*
Graph::s_instance;

using namespace wpn114;

// ------------------------------------------------------------------------------------------------
sample_t**
wpn114::allocate_buffer(nchannels_t nchannels, vector_t nframes)
// we make no assumption as to how many channels each input/output may have
// by the time the graph is ready/updated, so we can't really template it upfront
// ------------------------------------------------------------------------------------------------
{
    sample_t** block = new sample_t*[nchannels];
    for (nchannels_t n = 0; n < nchannels; ++n)
        block[n] = new sample_t[nframes];

    return block;
}

// ------------------------------------------------------------------------------------------------
Port::Port(Node* parent, QString name, Type type, Polarity polarity,
           bool is_default, uint8_t nchannels) :
// C++ constructor, called from the macro-declarations
// we immediately store a pointer in parent Node's input/output Port vector
// ------------------------------------------------------------------------------------------------
    m_parent      (parent),
    m_name        (name),
    m_polarity    (polarity),
    m_type        (type),
    m_nchannels   (nchannels),
    m_value       (0),
    m_default     (is_default)
{
    parent->register_port(*this);
}

// ------------------------------------------------------------------------------------------------
void
Port::assign(Port* p)
// ------------------------------------------------------------------------------------------------
{
    switch(p->polarity()) {
        case Polarity::Input: Graph::instance().connect(*this, *p); break;
        case Polarity::Output: Graph::instance().connect(*p, *this);
    }
}

// ------------------------------------------------------------------------------------------------
WPN_TODO void
Port::set_nchannels(nchannels_t nchannels)
// todo: async call when graph is running
// ------------------------------------------------------------------------------------------------
{
    m_nchannels = nchannels;
}

// ------------------------------------------------------------------------------------------------
void
Port::set_mul(qreal mul)
// this overrides the mul for all connections based on this Port
// ------------------------------------------------------------------------------------------------
{
    m_mul = mul;
    for (auto& connection : m_connections)
         connection->set_mul(mul);
}

// ------------------------------------------------------------------------------------------------
void
Port::set_add(qreal add)
// this overrides the add for all connections based on this Port
// ------------------------------------------------------------------------------------------------
{
    m_add = add;
    for (auto& connection : m_connections)
         connection->set_add(add);
}

// ------------------------------------------------------------------------------------------------
WPN_EXAMINE QVariantList
Port::routing() const noexcept
{
    return m_routing;
}

// ------------------------------------------------------------------------------------------------
WPN_EXAMINE void
Port::set_routing(QVariantList r) noexcept
{
    m_routing = r;
}

// ------------------------------------------------------------------------------------------------
void
Port::set_muted(bool muted)
// mutes all connections, zero output
// ------------------------------------------------------------------------------------------------
{
    for (const auto& connection : m_connections)
         connection->m_muted = muted;
}

// ------------------------------------------------------------------------------------------------
WPN_CLEANUP bool
Port::connected(Port const& s) const noexcept
// ------------------------------------------------------------------------------------------------
{
    for (const auto& connection : m_connections)
    {
         if (m_polarity == Polarity::Input &&
             connection->source() == &s)
             return true;
         else if (m_polarity == Polarity::Output &&
              connection->dest() == &s)
             return true;
    }

    return false;
}

// ------------------------------------------------------------------------------------------------
WPN_CLEANUP bool
Port::connected(Node const& n) const noexcept
// ------------------------------------------------------------------------------------------------
{
    for (const auto& connection: m_connections)
    {
        if (m_polarity == Polarity::Input &&
            &connection->source()->parent_node() == &n)
            return true;
        else if (m_polarity == Polarity::Output &&
            &connection->dest()->parent_node() == &n)
            return true;
    }

    return false;
}

// ------------------------------------------------------------------------------------------------
// these are implemented here as not to break ODR

template<> audiobuffer_t
Port::buffer() noexcept { return m_buffer.audio; }

template<> midibuffer_t*
Port::buffer() noexcept { return &m_buffer.midi; }

void
Port::reset() { m_buffer.midi.clear(); }

// ------------------------------------------------------------------------------------------------
void
Graph::register_node(Node& node) noexcept
// we should maybe add a vectorChanged signal-slot connection to this
{
    qDebug() << "[GRAPH] registering node:" << node.name();

    QObject::connect(s_instance, &Graph::rateChanged, &node, &Node::on_rate_changed);
    m_nodes.push_back(&node);
}

// ------------------------------------------------------------------------------------------------
void
Port::add_connection(Connection* con)
{
    m_connections.push_back(con);
}

// ------------------------------------------------------------------------------------------------
Connection&
Graph::connect(Port& source, Port& dest, Routing matrix)
// ------------------------------------------------------------------------------------------------
{
    assert(source.polarity() == Polarity::Output && dest.polarity() == Polarity::Input);
    assert(source.type() == dest.type());

    m_connections.emplace_back(source, dest, matrix);

    qDebug() << "[GRAPH] connected Node" << source.parent_node().name()
             << "(Port:" << source.name().append(")")
             << "with Node" << dest.parent_node().name()
             << "(Port:" << dest.name().append(")");

    for (nchannels_t n = 0; n < matrix.ncables(); ++n) {
        qDebug() << "[GRAPH] ---- routing: channel" << QString::number(matrix[n][0])
                 << ">> channel" << QString::number(matrix[n][1]);
    }

    return m_connections.back();
}

// ------------------------------------------------------------------------------------------------
WPN_EXAMINE Connection&
Graph::connect(Node& source, Node& dest, Routing matrix)
// might be midi and not audio...
// ------------------------------------------------------------------------------------------------
{
    auto& s_port = *source.default_port(Polarity::Output);
    auto& d_port = *dest.default_port(s_port.type(), Polarity::Input);
    return connect(s_port, d_port, matrix);
}

// ------------------------------------------------------------------------------------------------
WPN_INCOMPLETE Connection&
Graph::connect(Node& source, Port& dest, Routing matrix)
// ------------------------------------------------------------------------------------------------
{
    return connect(*source.default_port(dest.type(), Polarity::Output), dest, matrix);
}

// ------------------------------------------------------------------------------------------------
WPN_INCOMPLETE Connection&
Graph::connect(Port& source, Node& dest, Routing matrix)
// ------------------------------------------------------------------------------------------------
{
    return connect(source, *dest.default_port(source.type(), Polarity::Input), matrix);
}

// ------------------------------------------------------------------------------------------------
WPN_EXAMINE void
Graph::componentComplete()
// Graph is complete, all connections have been set-up
// we send signal to all registered Nodes to proceed to Port/pool allocation
// ------------------------------------------------------------------------------------------------
{   
    for (auto& connection : m_connections) {
        // when pushing in Graph::connect, wild undefined behaviour bugs appear..
        // better to do it in one run here
        connection.source()->add_connection(&connection);
        connection.dest()->add_connection(&connection);
    }

    Graph::debug("component complete, allocating nodes i/o");

    for (auto& node : m_nodes)
        node->on_graph_complete(m_properties);

    Graph::debug("i/o allocation complete");
    emit complete();
}

// ------------------------------------------------------------------------------------------------
WPN_INCOMPLETE WPN_AUDIOTHREAD vector_t
Graph::run(Node& target) noexcept
// the main processing function
// Graph will process itself from target Node and upstream recursively
// ------------------------------------------------------------------------------------------------
{
    // take a little amount of time to process asynchronous graph update requests (TODO)
    WPN_TODO

    // process target, return outputs            
    vector_t nframes = m_properties.vector;
    target.process(nframes);

    for (auto& node : m_nodes)
         node->set_processed(false);

    return nframes;
}

#include "spatial.hpp"

// ------------------------------------------------------------------------------------------------
Node::~Node()
{
    if (m_spatial) delete m_spatial;
    Graph::instance().unregister_node(*this);
}

// ------------------------------------------------------------------------------------------------
Spatial*
Node::spatial()
// if qml requests access to spatial, we have to create it
// ------------------------------------------------------------------------------------------------
{
    if (!m_spatial) {
        qDebug() << m_name << "creating spatial attributes";

        m_spatial = new Spatial;
        // we make an implicit connection
        m_subnodes.push_front(m_spatial);
    }

    return m_spatial;
}

// ------------------------------------------------------------------------------------------------
void
Node::allocate_pools()
// ------------------------------------------------------------------------------------------------
{
    for (auto& port : m_input_ports)
        if (port->type() == Port::Audio)
             m_input_pool.audio.push_back(port->buffer<audiobuffer_t>());
        else m_input_pool.midi.push_back(port->buffer<midibuffer_t*>());

    for (auto& port : m_output_ports)
        if (port->type() == Port::Audio)
             m_output_pool.audio.push_back(port->buffer<audiobuffer_t>());
        else m_output_pool.midi.push_back(port->buffer<midibuffer_t*>());
}

// ------------------------------------------------------------------------------------------------
// CONNECTION
//-------------------------------------------------------------------------------------------------
Connection::Connection(Port& source, Port& dest, Routing matrix) :
    m_source   (&source),
    m_dest     (&dest),
    m_routing  (matrix)
{
    componentComplete();
}

// ------------------------------------------------------------------------------------------------
void
Connection::setTarget(const QQmlProperty& target)
// qml property binding, e. g.:
// Connection on 'Port' { level: db(-12); routing: [0, 1] }
// ------------------------------------------------------------------------------------------------
{
    auto port = target.read().value<Port*>();

    switch(port->polarity()) {
        case Polarity::Output: m_source = port; break;
        case Polarity::Input: m_dest = port;
    }

    componentComplete();
    Graph::instance().add_connection(*this);
}

// ------------------------------------------------------------------------------------------------
void
Connection::componentComplete()
{
    m_nchannels = std::min(m_source->nchannels(), m_dest->nchannels());
    m_mul = m_source->mul() * m_dest->mul();
    m_add = m_source->add() + m_dest->add();
    m_muted = m_source->muted() || m_dest->muted();
}

// ------------------------------------------------------------------------------------------------
WPN_AUDIOTHREAD void
Connection::pull(vector_t nframes) noexcept
// ------------------------------------------------------------------------------------------------
{       
    auto& source = m_source->parent_node();

    if (!source.processed())
        source.process(nframes);

    if (m_source->type() == Port::Midi_1_0)
    {
        if(m_muted)
           return;

        auto& sbuf = *m_source->buffer<midibuffer_t*>();
        auto& dbuf = *m_dest->buffer<midibuffer_t*>();

        if (sbuf.size())
            qDebug() << "sbuf nevents:" << QString::number(sbuf.size());

        // move events to dest buffer
        // TODO: forbid reallocation
        dbuf.insert(dbuf.end(), sbuf.begin(), sbuf.end());
        sbuf.clear();
        return;
    }

    auto dbuf = m_dest->buffer<audiobuffer_t>();

    if (m_muted) {
        for (nchannels_t c = 0; c < m_dest->nchannels(); ++c)
             memset(dbuf, 0, sizeof(sample_t)*nframes);
        return;
    }

    auto sbuf = m_source->buffer<audiobuffer_t>();
    sample_t mul = m_mul, add = m_add;
    Routing routing = m_routing;

    if (routing.null())
        for (nchannels_t c = 0; c < m_nchannels; ++c)
            for (vector_t f = 0; f < nframes; ++f)
                dbuf[c][f] += sbuf[c][f] * mul + add;
    else
        for (nchannels_t c = 0; c < routing.ncables(); ++c)
            for (vector_t f = 0; f < nframes; ++f)
                dbuf[routing[c][1]][f] += sbuf[routing[c][0]][f] * mul + add;
}
