#include "reassembler.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  const uint64_t end_index = first_index + data.size();
  if ( end_index < next_seq_num_ ) {
    return;
  }
  if ( is_last_substring ) {
    last_substring_end_index_ = end_index;
  }

  if ( first_index > next_seq_num_ ) {
    // 1. check occupied_range
    if ( !check_range( data, first_index ) ) {
      return;
    }
    // 2. check space
    if ( data.size() > space( output ) ) {
      return;
    }
    // 3. update bytes_pending and occupied_range
    bytes_pending_ += data.size();
    occupied_range_.emplace( first_index, first_index + data.size() );
    // 4. emplace
    substrings_.emplace( first_index, std::move( data ) );

  } else {
    // 1. check space
    const auto max_space = static_cast<int64_t>( min( space( output ), end_index - next_seq_num_ ) );
    // 2. push substr
    const auto start = static_cast<int64_t>( next_seq_num_ - first_index );
    output.push( { data.begin() + start, data.begin() + start + max_space } );
    // 3. update next_seq_sum
    next_seq_num_ += max_space;
    // 4. scan_storage
    scan_storage( output );
  }

  check_last_byte_is_pushed( output );
}

uint64_t Reassembler::bytes_pending() const
{
  return bytes_pending_;
}

uint64_t Reassembler::space( const Writer& writer ) const
{
  return writer.available_capacity() - bytes_pending_;
}

bool Reassembler::check_range( std::string& data, uint64_t& first_index )
{
  auto [lb, ub] = occupied_range_.equal_range( first_index );
  const auto end_index = data.size() + first_index;
  // Contains by data

  // Overlapping data tails
  if ( ub->first_index_ > end_index ) {
    data = data.substr( first_index, ub->first_index_ );
    if ( data.empty() ) {
      return false;
    }
  }
  // Overlapping data heads
  if ( lb != ub && lb->end_index_ < first_index ) {
    first_index = lb->end_index_;
    data = data.substr( first_index );
    if ( data.empty() ) {
      return false;
    }
  }
  return true;
}

void Reassembler::scan_storage( Writer& writer )
{
  while ( true ) {
    if ( const auto it = substrings_.find( next_seq_num_ ); it != substrings_.end() ) {
      const size_t size = it->second.size();
      occupied_range_.erase( occupied_range_.find( it->first ) );
      writer.push( std::move( it->second ) );
      substrings_.erase( it );
      bytes_pending_ -= size;
      next_seq_num_ += size;
    } else {
      break;
    }
  }
  check_last_byte_is_pushed( writer );
}

void Reassembler::check_last_byte_is_pushed( Writer& writer ) const
{
  if ( writer.bytes_pushed() == last_substring_end_index_ ) {
    writer.close();
  }
}