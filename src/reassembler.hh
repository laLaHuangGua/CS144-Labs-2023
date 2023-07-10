#pragma once

#include "byte_stream.hh"

#include <climits>
#include <map>
#include <string>
#include <unordered_map>

using unordered_map_it = std::unordered_map<uint64_t, std::string>::iterator;
using map_it = std::map<uint64_t, uint64_t>::iterator;

class Reassembler
{
private:
  std::unordered_map<uint64_t, std::string> substrings_ {};
  std::map<uint64_t, uint64_t> occupied_range_ {};
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
  bool check_range( std::string& data, uint64_t& first_index );
  void scan_storage( Writer& writer );
  void check_last_byte_is_pushed( Writer& writer ) const;
  void erase_substring_by( uint64_t first_index );
  map_it erase_substring_by( const map_it it );
  void erase_old_substring();
};