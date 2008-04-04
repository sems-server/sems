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

/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
