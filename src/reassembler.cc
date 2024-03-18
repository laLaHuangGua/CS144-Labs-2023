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
    auto [success, hint] = fit_string( data, first_index );
    if ( !success ) {
      return;
    }
    if ( !fit_space( data, output ) ) {
      return;
    }
    bytes_pending_ += data.size();
    occupied_range_.emplace_hint( hint, first_index, first_index + data.size() );
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

bool Reassembler::fit_space( std::string& data, const Writer& output )
{
  if ( data.size() > space( output ) ) {
    return false;
  }
  if ( data.size() == space( output ) ) {
    data.pop_back();
  }
  return true;
}

uint64_t Reassembler::bytes_pending() const
{
  return bytes_pending_;
}

uint64_t Reassembler::space( const Writer& writer ) const
{
  return writer.available_capacity() - bytes_pending_;
}

std::pair<bool, MapIt_t> Reassembler::fit_string( std::string& data, uint64_t& first_index )
{
  if ( occupied_range_.empty() ) {
    return { true, occupied_range_.end() };
  }
  auto end_index = data.size() + first_index;

  // Case 1: lower_bound - 1
  auto it = occupied_range_.lower_bound( first_index );
  if ( it != occupied_range_.begin() ) {
    it--;
    // if overlapping
    if ( it->second > first_index ) {
      if ( it->second >= end_index ) {
        // this string is already contained by the string iterator points to
        return { false, {} };
      }
      // cut off the overlapping part
      data = data.substr( it->second - first_index, data.size() );
      first_index = it->second;
    }
    // whether the first_index is updated or not,
    // previous string is still the lower bound of new string
    it++;
  }
  // the new string is the last string in reassembler storage ( no overlapping )
  if ( it == occupied_range_.end() ) {
    return { true, occupied_range_.end() };
  }

  // Case 2: lower_bound ( equal )
  if ( it->first == first_index ) {
    end_index = data.size() + first_index;
    if ( it->second >= end_index ) {
      // this string is already contained by the string iterator points to
      return { false, {} };
    }
    // erase the string iterator points to and get next string
    it = erase_substring_by( it );
    if ( it == occupied_range_.end() ) {
      return { true, occupied_range_.end() };
    }
  }

  // Case 3: upper bound
  end_index = data.size() + first_index;
  while ( it->first < end_index ) {
    if ( it->second >= end_index ) {
      end_index = it->first;
      data = data.substr( 0, end_index - first_index );
      break;
    }
    it = erase_substring_by( it );
    if ( it == occupied_range_.end() ) {
      return { true, occupied_range_.end() };
    }
  }
  return { true, it };
}

void Reassembler::scan_storage( Writer& writer )
{
  erase_useless_substring();
  for ( auto it = occupied_range_.begin(); it != occupied_range_.end() && next_seq_num_ >= it->first; ) {
    const auto substring_it = substrings_.find( it->first );
    writer.push( substring_it->second.substr( next_seq_num_ - it->first ) );
    next_seq_num_ = it->second;
    it = erase_substring_by( it );
  }
}

void Reassembler::check_last_byte_is_pushed( Writer& writer ) const
{
  if ( writer.bytes_pushed() == last_substring_end_index_ ) {
    writer.close();
  }
}

MapIt_t Reassembler::erase_substring_by( MapIt_t it )
{
  bytes_pending_ -= it->second - it->first;
  substrings_.erase( it->first );
  return occupied_range_.erase( it );
}

void Reassembler::erase_useless_substring()
{
  for ( auto it = occupied_range_.begin(); it != occupied_range_.end() && it->second <= next_seq_num_; ) {
    it = erase_substring_by( it );
  }
}