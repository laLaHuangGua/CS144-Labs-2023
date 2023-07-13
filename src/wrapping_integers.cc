#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return zero_point + n;
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  const uint64_t base = ( ( checkpoint >> 32 ) << 32 );
  const int64_t offset = raw_value_ - zero_point.raw_value_;
  uint64_t val = base + offset;
  if ( base != 0 && offset > 0 ) {
    val -= ( 1UL << 32 );
  };
  val = min( { val, val + ( 1UL << 32 ), val + ( 1UL << 33 ) }, [checkpoint]( const uint64_t a, const uint64_t b ) {
    return DIST( a, checkpoint ) < DIST( b, checkpoint );
  } );
  return val;
}
