#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  if ( zero_point_.has_value() && message.seqno == zero_point_ ) {
    return;
  }
  if ( message.SYN ) {
    zero_point_ = message.seqno;
    ackno_ = zero_point_.value() + message.sequence_length();
    reassembler.insert( 0, message.payload, message.FIN, inbound_stream );
  } else if ( zero_point_.has_value() ) {
    const uint64_t first_index = message.seqno.unwrap( zero_point_.value(), inbound_stream.bytes_pushed() ) - 1;
    const uint64_t pre_bytes_pushed = inbound_stream.bytes_pushed();
    reassembler.insert( first_index, message.payload, message.FIN, inbound_stream );
    ackno_ = ackno_ + ( inbound_stream.bytes_pushed() - pre_bytes_pushed );
  }
  if ( message.FIN && zero_point_.has_value() ) {
    FIN_seqno = message.seqno + ( message.sequence_length() - 1 );
  }
  if ( FIN_seqno.has_value() && FIN_seqno == ackno_ ) {
    ackno_ = ackno_ + 1;
    inbound_stream.close();
  }
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  const uint16_t window_size = min( static_cast<uint64_t>( UINT16_MAX ), inbound_stream.available_capacity() );
  if ( !zero_point_.has_value() ) {
    return { {}, window_size };
  }
  return { ackno_, window_size };
}
