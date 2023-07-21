#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include <memory>

class TCPSender
{
  Wrap32 isn_;

  bool window_is_nonzero_ = true;
  bool retransmit_flag_ = false;
  bool pre_segment_has_FIN_ = false;
  bool can_use_magic_ = false;
  bool available_to_send_FIN_ = false;
  uint16_t remaining_window_size_ = 1;
  uint64_t absolute_seqno_ = 0; // as checkpoint
  uint64_t pre_unwarped_ackno_ = 0;

  bool no_out_of_win_segment_ = true;
  size_t next_segment_ = 0;
  std::deque<TCPSenderMessage> segments_ {};

  uint64_t sequence_numbers_in_flight_ = 0;
  uint64_t consecutive_retransmissions_ = 0;

  class Timer
  {
  public:
    bool is_running_ = false;
    uint64_t initial_RTO_ms_;
    uint64_t cumulative_time_elapsed_ = 0;
    uint64_t current_RTO_ms_;

    explicit Timer( const uint64_t initial_RTO_ms );

    void run();
    void elapse( const uint64_t time_passed );
    void set_RTO_by_factor( const uint8_t factor );
    void stop();
    void expire();
    void restart();

    bool has_expired() const;
    bool is_running() const;

    ~Timer() = default;

    // no copy or move
    Timer( const Timer& other ) = delete;
    Timer& operator=( const Timer& other ) = delete;
    Timer( Timer&& other ) = delete;
    Timer& operator=( Timer&& other ) = delete;
  };

  std::unique_ptr<Timer> timer_;

public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( uint64_t initial_RTO_ms, std::optional<Wrap32> fixed_isn );

  /* Push bytes from the outbound stream */
  void push( Reader& outbound_stream );

  /* Send a TCPSenderMessage if needed (or empty optional otherwise) */
  std::optional<TCPSenderMessage> maybe_send();

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage send_empty_message() const;

  /* Receive an act on a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called. */
  void tick( uint64_t ms_since_last_tick );

  /* Accessors for use in testing */
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?

private:
  void check_outstanding_segments( const uint64_t current_unwraped_ackno );
  bool no_outstanding_segment() const; // sent but unacked
  bool no_cached_segment() const;      // not yet send but usable
};
