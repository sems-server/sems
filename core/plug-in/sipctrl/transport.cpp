#include "transport.h"
#include <assert.h>

transport::transport(trans_layer* tl)
    : tl(tl)
{
    assert(tl);
}

transport::~transport()
{

}
