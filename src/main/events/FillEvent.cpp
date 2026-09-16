#include "FillEvent.h"

namespace events {
    unique_ptr<Execution> Execution::clone() const{
        return nullptr;
    }
}