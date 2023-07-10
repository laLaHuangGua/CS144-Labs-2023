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
    if ( !check_range( data, first_index, output ) ) {
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

bool Reassembler::check_range( std::string& data, uint64_t& first_index, Writer& writer )
{
  auto end_index = data.size() + first_index;

  // Case 1: lower_bound - 1
  auto it = occupied_range_.lower_bound( first_index );
  if ( it == occupied_range_.end() ) {
    return true;
  }
  if ( it != occupied_range_.begin() ) {
    it--;
    if ( it->end_index_ > first_index ) {
      if ( it->end_index_ < end_index ) {
        data = data.substr( it->end_index_ );
        first_index = it->end_index_;
        it = occupied_range_.lower_bound( first_index );
      } else {
        return false;
      }
    } else {
      it++;
    }
  }
  // Case 2: lower_bound ( equal )
  if ( it->first_index_ == first_index ) {
    end_index = data.size() + first_index;
    if ( it->end_index_ < end_index ) {
      erase_substring_by( it->first_index_, writer, false );
      it = occupied_range_.lower_bound( first_index );
    } else {
      return false;
    }
  }
  if ( it == occupied_range_.end() ) {
    return true;
  }
  // Case 3: upper bound
  end_index = data.size() + first_index;
  while ( it->first_index_ < end_index ) {
    if ( it->end_index_ < end_index ) {
      erase_substring_by( it->first_index_, writer, false );
      it = occupied_range_.lower_bound( first_index );
      if ( it == occupied_range_.end() ) {
        break;
      }
    } else {
      end_index = it->first_index_;
      data = data.substr( first_index, end_index );
      break;
    }
  }
  return true;
}

void Reassembler::scan_storage( Writer& writer )
{
  while ( erase_substring_by( next_seq_num_, writer, true ) ) {
    ;
  }
  erase_old_substring();
  check_last_byte_is_pushed( writer );
}

void Reassembler::check_last_byte_is_pushed( Writer& writer ) const
{
  if ( writer.bytes_pushed() == last_substring_end_index_ ) {
    writer.close();
  }
}

bool Reassembler::erase_substring_by( const uint64_t first_index, Writer& writer, bool flag )
{
  const auto it = substrings_.find( first_index );

  if ( it == substrings_.end() ) {
    return false;
  }

  const size_t size = it->second.size();
  occupied_range_.erase( occupied_range_.find( it->first ) );
  if ( flag ) {
    writer.push( std::move( it->second ) );
  }
  substrings_.erase( it );
  bytes_pending_ -= size;
  next_seq_num_ += size;
  return true;
}

void Reassembler::erase_old_substring()
{
  if ( !occupied_range_.empty() ) {
    for ( auto it = occupied_range_.begin(); it->end_index_ <= next_seq_num_ && it != occupied_range_.end(); ) {
      const auto size = it->end_index_ - it->first_index_;
      substrings_.erase( it->first_index_ );
      it = occupied_range_.erase( it );
      bytes_pending_ -= size;
    }
  }
}