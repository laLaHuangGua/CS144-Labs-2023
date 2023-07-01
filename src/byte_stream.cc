#include <stdexcept>

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

void Writer::push( string data )
{
  if ( !is_closed() ) {
    const uint64_t space = min( available_capacity(), data.size() );
    uint64_t i = 0;
    while ( i < space ) {
      stream_.push_back( data[i] );
      i++;
    }
    bytes_pushed_ += i;
  }
}

void Writer::close()
{
  is_closed_ = true;
}

void Writer::set_error()
{
  has_error_ = true;
}

bool Writer::is_closed() const
{
  return is_closed_;
}

uint64_t Writer::available_capacity() const
{
  return static_cast<uint64_t>( capacity_ - stream_.size() );
}

uint64_t Writer::bytes_pushed() const
{
  return bytes_pushed_;
}

string_view Reader::peek() const
{
  if ( stream_.data() == nullptr ) {
    return {};
  }
  return { &stream_.front(), 1 };
}

bool Reader::is_finished() const
{
  return bytes_buffered() == 0 && is_closed_;
}

bool Reader::has_error() const
{
  return has_error_;
}

void Reader::pop( uint64_t len )
{
  if ( !( bytes_buffered() < len ) ) {
    stream_.erase( stream_.begin(), stream_.begin() + static_cast<int64_t>( len ) );
    bytes_poped_ += len;
  } else {
    bytes_poped_ += bytes_buffered();
    stream_.erase( stream_.begin(), stream_.begin() + static_cast<int64_t>( bytes_buffered() ) );
  }
}

uint64_t Reader::bytes_buffered() const
{
  return static_cast<uint64_t>( stream_.size() );
}

uint64_t Reader::bytes_popped() const
{
  return bytes_poped_;
}
