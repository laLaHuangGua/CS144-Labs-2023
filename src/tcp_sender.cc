#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <random>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) ), timer_( make_unique<Timer>( initial_RTO_ms ) )
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return sequence_numbers_in_flight_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retransmissions_;
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  if ( retransmit_flag_ && !no_outstanding_segment() ) {
    timer_->run();
    retransmit_flag_ = false;
    return segments_.front();
  }
  if ( !no_cached_segment() ) {
    timer_->run();
    return segments_[next_segment_++];
  }
  return {};
}

void TCPSender::push( Reader& outbound_stream )
{
  uint64_t window_size = remaining_window_size_;
  if ( remaining_window_size_ == 0 && can_use_magic_ ) {
    can_use_magic_ = false;
    window_size = 1;
  }
  while ( window_size > 0 ) {
    TCPSenderMessage msg {};
    // With SYN
    if ( absolute_seqno_ == 0 ) {
      window_size -= 1;
      msg.SYN = true;
    }
    // With Payload and pop bytes
    uint64_t payload_size = min(
      { static_cast<uint64_t>( TCPConfig::MAX_PAYLOAD_SIZE ), window_size, outbound_stream.bytes_buffered() } );
    string payload {};
    while ( payload_size > 0 ) {
      auto view = outbound_stream.peek();
      payload += view;
      outbound_stream.pop( view.size() );
      payload_size--;
    }
    window_size -= payload.size();
    msg.payload = std::move( payload );
    // With FIN
    if ( window_size > 0 && outbound_stream.is_finished() && !pre_segment_has_FIN_ ) {
      msg.FIN = true;
      pre_segment_has_FIN_ = true;
      window_size--;
    }
    if ( msg.sequence_length() == 0 ) {
      break;
    }
    if ( msg.FIN && !available_to_send_FIN_ ) {
      pre_segment_has_FIN_ = false;
      break;
    }
    // set seqno
    msg.seqno = Wrap32::wrap( absolute_seqno_, isn_ );
    absolute_seqno_ += msg.sequence_length();
    // cashed segment
    sequence_numbers_in_flight_ += msg.sequence_length();
    segments_.push_back( std::move( msg ) );
    // set window
    remaining_window_size_ = window_size;
  }
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  return { Wrap32::wrap( absolute_seqno_, isn_ ) };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  uint64_t current_unwraped_ackno = 0;
  if ( msg.ackno.has_value() ) {
    current_unwraped_ackno = msg.ackno.value().unwrap( isn_, absolute_seqno_ );
  }
  if ( current_unwraped_ackno > absolute_seqno_ ) {
    return;
  }
  available_to_send_FIN_ = msg.window_size + current_unwraped_ackno > absolute_seqno_;
  if ( msg.window_size == 0 ) {
    available_to_send_FIN_ = msg.window_size + current_unwraped_ackno >= absolute_seqno_;
  }
  remaining_window_size_ = msg.window_size;
  window_is_nonzero_ = static_cast<bool>( msg.window_size );
  if ( !window_is_nonzero_ ) {
    can_use_magic_ = true;
  }

  if ( pre_unwarped_ackno_ < current_unwraped_ackno ) {
    timer_->set_RTO_by_factor( 1 );
    if ( !no_outstanding_segment() ) {
      timer_->restart();
    }
    consecutive_retransmissions_ = 0;
    pre_unwarped_ackno_ = current_unwraped_ackno;
    check_outstanding_segments( current_unwraped_ackno );
  }
}

void TCPSender::check_outstanding_segments( const uint64_t current_unwraped_ackno )
{
  while ( !no_outstanding_segment() ) {
    const auto it = segments_.begin();
    const uint64_t end_abs_seqno = it->seqno.unwrap( isn_, absolute_seqno_ ) + it->sequence_length();
    if ( current_unwraped_ackno < end_abs_seqno ) {
      break;
    }
    sequence_numbers_in_flight_ -= it->sequence_length();
    segments_.pop_front();
    next_segment_ -= 1;
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick )
{
  if ( no_outstanding_segment() && no_cached_segment() ) {
    timer_->stop();
  }
  timer_->elapse( ms_since_last_tick );
  if ( timer_->has_expired() ) {
    retransmit_flag_ = true;
    if ( window_is_nonzero_ ) {
      consecutive_retransmissions_ += 1;
      timer_->set_RTO_by_factor( 2 );
    } else {
      timer_->set_RTO_by_factor( 1 );
    }
    timer_->restart();
  }
}

bool TCPSender::no_outstanding_segment() const
{
  return next_segment_ == 0;
}

bool TCPSender::no_cached_segment() const
{
  if ( no_out_of_win_segment_ ) {
    return next_segment_ == segments_.size();
  }
  return segments_.size() - 1 == next_segment_;
}

// Timer implementation
TCPSender::Timer::Timer( const uint64_t initial_RTO_ms )
  : initial_RTO_ms_( initial_RTO_ms ), current_RTO_ms_( initial_RTO_ms )
{}

void TCPSender::Timer::run()
{
  is_running_ = true;
}

void TCPSender::Timer::elapse( const uint64_t time_passed )
{
  if ( is_running() ) {
    cumulative_time_elapsed_ += time_passed;
  }
  if ( current_RTO_ms_ <= cumulative_time_elapsed_ ) {
    expire();
  }
}

void TCPSender::Timer::set_RTO_by_factor( const uint8_t factor )
{
  if ( factor == 1 ) {
    current_RTO_ms_ = initial_RTO_ms_;
  } else {
    current_RTO_ms_ *= factor;
  }
}

void TCPSender::Timer::stop()
{
  is_running_ = false;
}

void TCPSender::Timer::expire()
{
  is_running_ = false;
  cumulative_time_elapsed_ = 0;
}

void TCPSender::Timer::restart()
{
  is_running_ = true;
  cumulative_time_elapsed_ = 0;
}

bool TCPSender::Timer::has_expired() const
{
  return !is_running_ && cumulative_time_elapsed_ == 0;
}

bool TCPSender::Timer::is_running() const
{
  return is_running_;
}