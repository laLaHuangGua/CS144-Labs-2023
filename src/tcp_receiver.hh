#pragma once

#include "reassembler.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#define MAX_RWND_SIZE ( ( 1UL << 16 ) - 1 )

class TCPReceiver
{
private:
  std::optional<Wrap32> isn_ {};
  std::optional<Wrap32> FIN_seqno_ {};
  Wrap32 ackno_ { 0 };

public:
  /*
   * The TCPReceiver receives TCPSenderMessages, inserting their payload into the Reassembler
   * at the correct stream index.
   */
  void receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream );

  /* The TCPReceiver sends TCPReceiverMessages back to the TCPSender. */
  TCPReceiverMessage send( const Writer& inbound_stream ) const;
};
