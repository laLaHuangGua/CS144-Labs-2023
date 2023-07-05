#include "reassembler.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  const size_t size = data.size();

  if ( first_index < next_seq_num_ ) {
    const uint64_t end_index = first_index + size;

    if ( end_index >= next_seq_num_ ) {
      const auto len = min_space( output, end_index - next_seq_num_ );
      const auto start = next_seq_num_ - first_index;
      output.push( data.substr( start, len ) );
      next_seq_num_ += len;
    }

  } else if ( first_index > next_seq_num_ ) {
    if ( size > output.available_capacity() - bytes_pending_ ) {
      return;
    }

    const auto& [it, success] = substrings_.emplace( first_index, Info { size, is_last_substring, data } );
    if ( success ) {
      bytes_pending_ += size;
    } else if ( size > it->second.size_ ) {
      bytes_pending_ += ( size - it->second.size_ );
      it->second.size_ = size;
      it->second.is_last_substring_ = is_last_substring;
      it->second.substring_ = std::move( data );
    }
    return;

  } else {
    const auto len = min_space( output, size );
    output.push( data.substr( 0, len ) );
    next_seq_num_ += len;
  }

  scan_storage( output );

  if ( is_last_substring ) {
    output.close();
  }
}

uint64_t Reassembler::bytes_pending() const
{
  return bytes_pending_;
}

uint64_t Reassembler::min_space( const Writer& writer, const size_t size ) const
{
  return min( writer.available_capacity() - bytes_pending_, size );
}

void Reassembler::scan_storage( Writer& output )
{
  while ( true ) {
    if ( substrings_.empty() ) {
      break;
    }

    const auto& it = substrings_.begin();
    if ( it->first > next_seq_num_ ) {
      break;
    }

    const auto end_index = it->first + it->second.size_;
    if ( end_index < next_seq_num_ ) {
      bytes_pending_ -= it->second.size_;
      substrings_.erase( it );
      continue;
    }

    const auto start = next_seq_num_ - it->first;
    output.push( it->second.substring_.substr( start ) );

    next_seq_num_ = end_index;
    bytes_pending_ -= it->second.size_;

    if ( it->second.is_last_substring_ ) {
      output.close();
    }
    substrings_.erase( it );
  }
}