#pragma once

#include "byte_stream.hh"

#include <climits>
#include <set>
#include <string>
#include <unordered_map>

struct Index
{
  uint64_t first_index_;
  uint64_t end_index_;

  bool operator<( const Index& other ) const { return first_index_ < other.first_index_; }
};

inline bool operator<( const uint64_t& first_index, const Index& other )
{
  return first_index < other.first_index_;
}
inline bool operator<( const Index& other, const uint64_t& first_index )
{
  return other.first_index_ < first_index;
}

class Reassembler
{
private:
  std::unordered_map<uint64_t, std::string> substrings_ {};
  std::set<Index, std::less<>> occupied_range_ {};
  uint64_t next_seq_num_ = 0;
  uint64_t bytes_pending_ = 0;
  uint64_t last_substring_end_index_ = UINT64_MAX;

public:
  /*
   * Insert a new substring to be reassembled into a ByteStream.
   *   `first_index`: the index of the first byte of the substring
   *   `data`: the substring itself
   *   `is_last_substring`: this substring represents the end of the stream
   *   `output`: a mutable reference to the Writer
   *
   * The Reassembler's job is to reassemble the indexed substrings (possibly out-of-order
   * and possibly overlapping) back into the original ByteStream. As soon as the Reassembler
   * learns the next byte in the stream, it should write it to the output.
   *
   * If the Reassembler learns about bytes that fit within the stream's available capacity
   * but can't yet be written (because earlier bytes remain unknown), it should store them
   * internally until the gaps are filled in.
   *
   * The Reassembler should discard any bytes that lie beyond the stream's available capacity
   * (i.e., bytes that couldn't be written even if earlier gaps get filled in).
   *
   * The Reassembler should close the stream after writing the last byte.
   */
  void insert( uint64_t first_index, std::string data, bool is_last_substring, Writer& output );

  // How many bytes are stored in the Reassembler itself?
  uint64_t bytes_pending() const;

private:
  uint64_t space( const Writer& writer ) const;
  bool check_range( std::string& data, uint64_t& first_index, Writer& writer );
  void scan_storage( Writer& writer );
  void check_last_byte_is_pushed( Writer& writer ) const;
  bool erase_substring_by( uint64_t first_index, Writer& writer, bool flag );
  void erase_old_substring();
};
