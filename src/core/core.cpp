#include "../../include/core.h"
#include "../../include/error.h"

void Core::checkArity(u8 min, u8 max, bool exclusive, u8 args)
{
    bool bounds{(args == min) || (args == max)};
    if ((args < min) || (args > max) || (exclusive && !bounds))
    {
        if (min == max)
        {
            throw RuntimeError(ARITY_MISMATCH,
                CH_STR("expect {} argument{} but found {}", max,
                    (max == 1 ? "" : "s"), args)
            );
        }
        else if (exclusive || (max == min + 1))
        {
            throw RuntimeError(ARITY_MISMATCH,
                CH_STR("expect {} or {} arguments but found {}", min, max, args)
            );
        }
        else if (min == 0)
        {
            throw RuntimeError(ARITY_MISMATCH,
                CH_STR("expect at most {} arguments but found {}", max, args)
            );
        }
        else
        {
            throw RuntimeError(ARITY_MISMATCH,
                CH_STR("expect {}-{} arguments but found {}", min, max, args)
            );
        }
    }
}